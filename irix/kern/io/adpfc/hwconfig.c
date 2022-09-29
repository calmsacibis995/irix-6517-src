/***************************************************************************
*									   *
* Copyright 1997 Adaptec, Inc.,  All Rights Reserved.			   *
*									   *
* This software contains the valuable trade secrets of Adaptec.  The	   *
* software is protected under copyright laws as an unpublished work of	   *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.	The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.							   *
*									   *
***************************************************************************/

/*
*
*  Module Name:   hwconfig.c
*
*  Description:
*		  Code to initialize Emerald chip
*
*  Owners:	  ECX Fibre Channel CHIM Team
*    
*  Notes:
*
*  Entry Point(s):
*
*		  
*		sh_get_configuration
*		sh_initialize
*		sh_read_link_speed
*		sh_prepare_config
*		sh_emerald_get_config
*		sh_get_driver_config
*		sh_init_hardware
*		sh_init_1160_ram
*		sh_start_sequencer
*
*/

#include "hwgeneral.h"
#include "hwcustom.h"
#include "ulmconfig.h"
#include "hwtcb.h"
#include "hwequ.h"
#include "hwref.h"
#include "hwdef.h"
#include "hwerror.h"
#include "seq_off.h"
#include "hwproto.h"

static INT sh_read_link_speed( REGISTER base , UBYTE *i);
static void sh_prepare_config(struct control_blk *c);
static INT sh_init_hardware(struct control_blk *c);
static INT sh_find_payload_size(int size);
static UBYTE sh_init_1160_ram(struct control_blk *c);
static UBYTE sh_run_bist(struct control_blk *c);


/* sh_get_configuration()
 *
 * In: pointer to shim_config, containing base address of adapter
 *
 * Out: INT, status on stack
 *
 *	in shim_config:
 *	USHORT	number_tcbs;
 *	USHORT	num_unsol_tcbs;
 *	UBYTE	link_speed;
 *
 *
 * Interrupts are turned off on the board. It is up to the user
 * to call sh_enable_irq() when the ULM is ready to take interrupts
 * Note: the caller must single thread access to the controller
 *	 before calling this function.
 */
INT
sh_get_configuration (struct shim_config *c)
{
	struct shim_config *s = c;
	register REGISTER base = s->base_address;
	UBYTE  size_select;
	USHORT	MaxTcbs_adjust, MaxTcbs[5] =
		{MAX3_TCBS,MAX21_TCBS,MAX2_TCBS,MAX11_TCBS,MAX1_TCBS};
	INT j ;

	while (!(INBYTE(AICREG(CMC_HCTL)) & HPAUSEACK)) {
		OUTBYTE(AICREG(CMC_HCTL), 
		(INBYTE(AICREG(CMC_HCTL)) | HPAUSE));
	}
	OUTBYTE(AICREG(POST_STAT0), 0xff);
	OUTBYTE(AICREG(POST_STAT1), 0xff);

	s->slimhim_version    = (USHORT) SLIM_HIM_VERSION;

	/* Clear All possible PCI errors here because only this
	 * should only be called during power-on initialization
	 * or after chip reset.  At this point, no DMA is active
	 * and it is safe to clear those PCI errors.
	 */
	OUTPCIBYTE(PCIREG(STATUS1), STATUS1_DPE|STATUS1_SERR
		|STATUS1_RMA|STATUS1_RTA|STATUS1_STA|STATUS1_DPR,c->ulm_info);
	OUTPCIBYTE(PCIREG(DMA_ERROR0), HR_DMA_RMA|HR_DMA_RTA
		|HR_DMA_DPR|HS_DMA_DPE|HS_DMA_RMA|HS_DMA_RTA,c->ulm_info);
	OUTPCIBYTE(PCIREG(DMA_ERROR1), CIP_DMA_RMA|CIP_DMA_RTA
		|CIP_DMA_DPE|CIP_DMA_DPR|CP_DMA_DPE|CP_DMA_RMA
		|CP_DMA_RTA|CP_DMA_DPR,c->ulm_info);
	OUTPCIBYTE(PCIREG(DMA_ERROR2), T_DPE,c->ulm_info);

	OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);

	/* Memory-port block Init    */

#ifdef _ADDRESS_WIDTH32
	/* 4 byte PCI, 4 PVAR pages, 0 EVAR */
	size_select = SGEL_SIZE16 | PVAR_PAGE4;
#else
	/* 8 byte PCI, 4 PVAR pages, 0 EVAR */
	size_select = SGEL_SIZE8 | PVAR_PAGE4;
#endif

	OUTBYTE(AICREG(SIZE_SELECT), size_select);

	if (sh_read_link_speed(base, (UBYTE *) &s->link_speed)) {
		s->link_speed = (UBYTE) 0;
		return (CANT_FIND_LINK_SPEED);
	}

#if SHIM_DEBUG
	OUTBYTE(AICREG(MODE_PTR) , CMC_MODE4); 
	OUTBYTE(AICREG(0x61) , 0x01);	
	OUTBYTE(AICREG(0x17) , 0x80);	      
#endif

/*	The last TCB number in SRAM includes following adjustments
		MVAR -- which is always 1
		PVAR -- which depends on how many PVAR pages allocated
		EVAR -- which depends on how many EVAR pages allocated
		1    -- TCB index is zero based				*/

	MaxTcbs_adjust = (USHORT) (1	  /* MVAR */
		+ (1 << (UBYTE)(size_select & PVAR_PAGE_MASK) ) /* PVAR */
		+ (2 * ( 1 << (UBYTE)				/* EVAR */
			((size_select & EVAR_PAGE_MASK) >> 3) - 1))
		+ 1);
	j = 0 ;
    
	while (j < ((sizeof MaxTcbs) / sizeof(USHORT))) {

		/* Load tcb pointer with max.  */
		MaxTcbs[j] = (USHORT) (MaxTcbs[j] - MaxTcbs_adjust);
		OUTWORD(AICREG(TCBPTR), MaxTcbs[j]);

		/* write test pattern */
		OUTBYTE(TCBMEM(TCB_DAT0)  , 0x66);
		OUTBYTE(TCBMEM(TCB_DAT0+1), 0xdd);    
		OUTBYTE(TCBMEM(TCB_DAT0+2), 0x55);
		OUTBYTE(TCBMEM(TCB_DAT0+3), 0xaa);

		/* read back test pattern */
		if ( (INBYTE(TCBMEM(TCB_DAT0)) != 0x66)
			|| (INBYTE(TCBMEM(TCB_DAT0+1)) != 0xdd)
			|| (INBYTE(TCBMEM(TCB_DAT0+2)) != 0x55)
			|| (INBYTE(TCBMEM(TCB_DAT0+3)) != 0xaa) ) {
			j++ ;
		} else 
			break ;

	}
  
	if (j == ((sizeof MaxTcbs) / sizeof(USHORT))) {
		return(NO_SRAM);
	}

	s->chip_version = INPCIBYTE(PCIREG(DEVREVID),c->ulm_info);

	s->number_tcbs = (USHORT) (MaxTcbs[j] + 1);

	s->num_unsol_tcbs = (USHORT) (s->number_tcbs / 10);
	s->done_q_size = (USHORT) (sizeof(struct _doneq) * s->number_tcbs);
	s->doneq_elem_size = sizeof(struct _doneq);
	s->emerald_states_size = sizeof(struct emerald_states);
	s->control_blk_size = sizeof( struct control_blk);
	s->lip_timeout = DEFAULT_LIP_TIMEOUT; 

	return (0);
}

/* sh_read_link_speed()
 *
 * In:	base
 *	pointer to INT
 *
 * Out: returns 0 for success, else failure
 *	sets INT *i to divisor of 2 Gigabaud
 *
 *	This function is called at get configuration time and the
 *	value loaded into CLK_GEN_CTL register has its least two bits
 *	being invalidated in order to work behind the hub.  Even if
 *	the least 2 bits returning 11b upon reading the register, it
 *	should actually be 10b when the link is up.  With a different
 *	hardware, this may work differently.
 */
static INT
sh_read_link_speed( REGISTER base , UBYTE *i)
{
	INT ret_status = 0; 
	UBYTE val;

	OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
	val = (UBYTE) ((INBYTE(AICREG(CLK_GEN_CTL)) & TX_CLK_CTL_MASK) | FULL_REF_CLK);

	switch(val) {
		case 0xbe:
			*i = 1;
			break;
		case 0x2e:
		case 0xfe:
			*i = 2;
			break;
		case 0x6e:
			*i = 4;
			break;
		default:
			ret_status = 1;
			break;
	}
	return(ret_status);
}

static UBYTE
sh_init_1160_ram(struct control_blk *c)
{
	register REGISTER base = c->base;
	USHORT i, j;

/*	When the SIZE_SELECT is changed for number of PVAR pages
 *	this field needs to be changed as well. Init and test PVAR.
 *	The init and test of PVAR pages will exclude page 3 which
 *	is reserved for BIOS's future expansion.
 */
	for (i = 0; i < 3; i++) {
		OUTBYTE(AICREG(PVARPTR), i);
		for (j = 0; j < 128; j++) {
			if (i == 0 && j == 126)
				break;
			OUTBYTE(SEQMEM(PVAR_DAT0 + j), j);
		}
	}

	for (i = 0; i < 3; i++) {
		OUTBYTE(AICREG(PVARPTR), i);
		for (j = 0; j < 128; j++) {
			if (i == 0 && j == 124)
				break;
			if (INBYTE(SEQMEM(PVAR_DAT0 + j)) != j) {
				return(1);
			}
		}
	}

	for (i = 0; i < 3; i++) {
		OUTBYTE(AICREG(PVARPTR), i);
		for (j = 0; j < 128; j+=2) {
			if (i == 0 && j == 126)
				break;
			OUTWORD(SEQMEM(PVAR_DAT0 + j), 0x55aa);
		}
	}
 
	for (i = 0; i < 3; i++) {
		OUTBYTE(AICREG(PVARPTR), i);
		for (j = 0; j < 128; j+=2) {
			if (i == 0 && j == 124)
				break;
			if (INWORD(SEQMEM(PVAR_DAT0 + j)) != 0x55aa) {
				return(1);
			}
		}
	}

/*	Init and test MVAR					*/
	for (i = CMC_MODE0; i <= CMC_MODE7; i += 0x11)
	{
		OUTBYTE(AICREG(MODE_PTR), i);
		for (j = 0; j < 32; j++)
			OUTBYTE(AICREG(MVAR_DAT0+j), j);
	}

	for (i = CMC_MODE0; i <= CMC_MODE7; i += 0x11)
	{
		OUTBYTE(AICREG(MODE_PTR), i);
		for (j = 0; j < 32; j++)
			if (INBYTE(AICREG(MVAR_DAT0+j)) != j)
				return(2);
	}

	for (i = CMC_MODE0; i <= CMC_MODE7; i += 0x11)
	{
		OUTBYTE(AICREG(MODE_PTR), i);
		for (j = 0; j < 32; j+=2)
			OUTWORD(AICREG(MVAR_DAT0+j), 0x55aa);
	}
 
	for (i = CMC_MODE0; i <= CMC_MODE7; i += 0x11)
	{
		OUTBYTE(AICREG(MODE_PTR), i);
		for (j = 0; j < 32; j+=2)
			if (INWORD(AICREG(MVAR_DAT0+j)) != 0x55aa)
				return(2);
	}

/*	Init and test all TCBs specified by the number_tcbs field in
 *	shim_config structure.	number_tcbs could potentially be less
 *	than the actual physical TCBs in the external SRAM.  For TCBs
 *	beyond number_tcbs field, only initialize them without verifying.
 *	After initialization is done, init the RUN_STATUS field and
 *	SG_CACHE_PTR field in TCB to invalidate all TCBs.
 */

	/* Initialize all TCBs and S/G RAM		*/
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE2);
	for (i = 0; i < c->s->number_tcbs; i++)
	{
		OUTWORD(AICREG(TCBPTR), i);
		for (j = 0; j < 128; j++)
			OUTBYTE(TCBMEM(TCB_DAT0 + j), j);

		OUTBYTE(AICREG(SG_CACHEPTR), 0);
		for (j = 0; j < 128; j++) 
			OUTBYTE(AICREG(SGEDAT0), j);
	}

	/* verify all TCBs and S/G RAM			*/
	for (i = 0; i < c->s->number_tcbs; i++)
	{
		OUTWORD(AICREG(TCBPTR), i);
		for (j = 0; j < 128; j++)
			if (INBYTE(TCBMEM(TCB_DAT0 + j)) != j)
				return(3);

		OUTBYTE(AICREG(SG_CACHEPTR), 0);
		for (j = 0; j < 128; j++) 
			if (INBYTE(AICREG(SGEDAT0)) != j)
				return(4);
	}

 /*	Initialize all TCBs and S/G RAM with second pattern		*/
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE2);
	for (i = 0; i < c->s->number_tcbs; i++)
	{
		OUTWORD(AICREG(TCBPTR), i);
		for (j = 0; j < 128; j+=2)
			OUTWORD(TCBMEM(TCB_DAT0 + j), 0x55aa);
 
		OUTBYTE(AICREG(SG_CACHEPTR), 0);
		for (j = 0; j < 128; j++) 
			OUTBYTE(AICREG(SGEDAT0), 0xa5);
	}

	/* Initialize TCBs beyond number_tcbs field	*/
	for (i = c->s->number_tcbs; i < MAX3_TCBS; i++)
	{
		OUTWORD(AICREG(TCBPTR), i);
		for (j = 0; j < 128; j++)
			OUTBYTE(TCBMEM(TCB_DAT0 +j), j);

		OUTBYTE(AICREG(SG_CACHEPTR), 0);
		for (j = 0; j < 128; j++) 
			OUTBYTE(AICREG(SGEDAT0), j);
	}

/*	Invalidate all TCBs by turning on ON_QUEUE flag for run_status
 *	and DATA_DONE bit for sg_cache_ptr.		
 */
	for (i = 0; i < MAX3_TCBS; i++)
	{
		OUTWORD(AICREG(TCBPTR), i);
		OUTBYTE(TCBMEM(RUN_STATUS), 0x02);

		OUTBYTE(TCBMEM(SG_CACHE_PTR), 0x04);
	}

/*	Currently there is no EVAR defined in SIZE_SELECT and hence
 *	no need for initialization.
 */

/*	Initialize and test internal SRAMs and buffers which includes
 *	GVAR and CMC buffer.
 */
/*	Init and test GVAR				*/
	for (i = 0; i < 8; i += 2)
		OUTWORD(AICREG(GVARDAT_START+i), 0x55aa);

	for (i = 0; i < 8; i += 2)
		if (INWORD(AICREG(GVARDAT_START+i)) != 0x55aa)
			return(5);

/*	Init and test CMC buffer			*/
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
	OUTBYTE(AICREG(CMC_BADR), 0);
	for (i = 0; i < 128; i += 2)
		OUTWORD(AICREG(CMC_BDAT0), 0x55aa);

	OUTBYTE(AICREG(CMC_BADR), 0);
	for (i = 0; i < 128; i += 2)
		if (INWORD(AICREG(CMC_BDAT0)) != 0x55aa)
			return(6);

	return(0);
}

void
sh_invalidate_tcbs(struct control_blk *c)
{
	register REGISTER base = c->base;
	USHORT i;

	for (i = 0; i < c->s->number_tcbs; i++)
	{	/* Assuming the maximum number of TCBs	*/
		OUTWORD(AICREG(TCBPTR), i);
		/* set on_queue flag for run_status in TCB */
		OUTBYTE(TCBMEM(RUN_STATUS), 0x02);
		/* set the data_done bit for sg_cache_ptr in TCB */
		OUTBYTE(TCBMEM(SG_CACHE_PTR), 0x04);
		/* Invalidate OX_IDs in the TCB */
		OUTWORD(TCBMEM(0x80), 0xfefe);
		/* Turn on the purge in the frame header of TCB */
		OUTBYTE(TCBMEM(0x8a), 0x08);
	}
}
static UBYTE
sh_run_bist(struct control_blk *c)
{
	register REGISTER base = c->base;
	UBYTE val;

	val =  INPCIBYTE(PCIREG(BIST),c->ulm_info);
	if (!(val & PCI_BIST_CAPABLE )) {
#if SHIM_DEBUG
		ulm_printf("Device does not support BIST\n");
#endif
		return((UBYTE)0);
	}

	/* Start Bist */
	OUTPCIBYTE(PCIREG(BIST), (val | START_BIST),c->ulm_info);

	ulm_delay(c->ulm_info, 2000);

	val = (UBYTE) (INPCIBYTE(PCIREG(BIST),c->ulm_info) & PCI_BIST_MASK);

#if	SHIM_DEBUG
	if (val & MTPE_BIST_FAIL)
		ulm_printf("MTPE BIST failed\n");
	if (val & RPB_BIST_FAIL)
		ulm_printf("RPB BIST failed\n");
	if (val & SPB_BIST_FAIL)
		ulm_printf("SPB BIST failed\n");
#endif

	return(val);
}


/*
*
* sh_initialize
*
* Initialize hardware
*
* Return Value: return status, 0 for success, see hwerror.h for failure
*		   
* Parameters:	pointer to shim_config structure
*
* Remarks:	Before calling this routine ULM must allocate memory
*		   
*/
INT
sh_initialize(struct shim_config *s, void *ulm_info)
{
	register struct control_blk *c = (struct control_blk *)
		s->control_blk;
	register REGISTER base = s->base_address;
	UBYTE val;

	c->s = s;
	c->base = base;
	c->ulm_info = ulm_info;
	c->seqmembase = s->seqmembase;
/* NEW */
	c->pcicfgbase = s->pcicfgbase;
	c->chip_version = s->chip_version;
	c->dma = (struct _dma_stat *) c->s->dma;

/*	start by collecting configuration information			*/
	sh_prepare_config(c);

	if (s->flag & SH_POWERUP_INITIALIZATION)
	{
/*		enable parity checking					*/
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
		OUTBYTE(AICREG(HCOMMAND0), HCOMMAND0_DEFAULT);

		if (sh_init_1160_ram(c))
			return (RAM_TEST_FAILED);

/*		ensure that SPB payload size is 2k			*/
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE0);
		OUTBYTE(AICREG(HST_CFG_CTL), SPB_PAYLOAD_SIZE_2048);
 
/*		ensure that RPB payload size is 2k			*/
		OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
		OUTBYTE(AICREG(RPB_PLD_SIZE), RPB_PLD_2048);
								
		if (sh_run_bist(c))
			return (BIST_FAILED);
	}
	else
		sh_invalidate_tcbs(c);

#if 0
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
	if (INBYTE(AICREG(CLK_GEN_CTL)) & 0x40)
		OUTBYTE(AICREG(CLK_GEN_CTL), 0xfe);
	else
		OUTBYTE(AICREG(CLK_GEN_CTL), 0x2e);
#endif

	/* set TX_CLK_CTRL1-0 bits as '10' (use REF_CLKI) and leave other bits alone */
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
	val = (UBYTE) ((INBYTE(AICREG(CLK_GEN_CTL)) & TX_CLK_CTL_MASK) | FULL_REF_CLK);
	OUTBYTE(AICREG(CLK_GEN_CTL), val);

/*	First thing to do during initialization is to make sure		*/
/*	that the SFC is not sending out garbage				*/
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);
	if (s->flag & SH_SET_ALPA)
	{
		OUTBYTE(AICREG(OUR_AIDA0),s->al_pa[0]);
		OUTBYTE(AICREG(OUR_AIDA1),s->al_pa[1]);
		OUTBYTE(AICREG(OUR_AIDA2),s->al_pa[2]);

		OUTBYTE(AICREG(OUR_AIDA_CFG), OUR_AIDA_VALID);

                /* OUTBYTE(AICREG(ORDR_CTRL), SEND_OS_CONT+LIPf8ALPA); */
/* NEW */
                OUTBYTE(AICREG(ORDR_CTRL), SEND_OS_CONT+IDLES); 
	}
	else
	{
		OUTBYTE(AICREG(OUR_AIDA0),0xef);
		OUTBYTE(AICREG(OUR_AIDA1),0x00);
		OUTBYTE(AICREG(OUR_AIDA2),0x00);

		OUTBYTE(AICREG(OUR_AIDA_CFG),0x00);
                 
                /* OUTBYTE(AICREG(ORDR_CTRL), SEND_OS_CONT+LIPf8ALPA); */
/* NEW */
                OUTBYTE(AICREG(ORDR_CTRL), SEND_OS_CONT+IDLES);
	}

	
/*	now we are ready to start initialization process		*/

	sh_setup_emerald_software(c);

#if 0
/* XXX debug */
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE4);
	OUTBYTE(AICREG(TRACESEL), 1);
	{
                uint8_t orig;
                orig = INBYTE(AICREG(0x17));
                OUTBYTE(AICREG(0x17), orig & ~0x60);
	}
#endif

/*	initialize hardware						*/
	sh_init_hardware(c);
	
/*	RFC block Init							*/
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);
	if (INBYTE(AICREG(SERDES_CTRL)) & SYNC_EN)
		OUTBYTE(AICREG(SERDES_CTRL), 
			(INBYTE(AICREG(SERDES_CTRL)) & ~SYNC_EN));

	c->miatype_vs = (UBYTE) (INBYTE(AICREG(SERDES_STATUS)) & 0x03);

	OUTBYTE(AICREG(SERDES_CTRL), SYNC_EN);
	ulm_delay(c->ulm_info, 1000);

	OUTBYTE(AICREG(SERDES_CTRL), SYNC_EN | LOCK_REF_CLK_L);
	ulm_delay(c->ulm_info, 1000);

	c->miatype_cs = (UBYTE) (INBYTE(AICREG(SERDES_STATUS)) & 0x03);

/*	Set the participating bit when SH_SET_ALPA is true		*/	  
	if (s->flag & SH_SET_ALPA)
		OUTBYTE(AICREG(ALC_LPSM_CMD), REQ_ENABLE | PARTICIPATING); 
	else
		OUTBYTE(AICREG(ALC_LPSM_CMD), REQ_ENABLE); 

/*	Reset and enable the link					*/
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE3);
	OUTBYTE(AICREG(LINK_ERRST), LINKRST);
	OUTBYTE(AICREG(LINK_ERRST), LINKEN);

	OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);

/*	Always enable the FAULT going to interrupt generator		*/
	OUTBYTE(AICREG(SERDES_CTRL), 
		INBYTE(AICREG(SERDES_CTRL)) | FAULT_INT_EN);

	OUTBYTE(AICREG(SFC_CMD0),
		(INBYTE(AICREG(SFC_CMD0)) & ~DISABLE_ALC_CONTROL));

/*	after V52, check falg to en/dis-able SYNC			*/
	if (s->flag & SH_ENABLE_SYNC_C_S)
		OUTBYTE(AICREG(ALC_MISC_CNTRL),
			(INBYTE(AICREG(ALC_MISC_CNTRL)) | ENABLE_SYNC_C_S)); 
	else
	    OUTBYTE(AICREG(ALC_MISC_CNTRL),
		(INBYTE(AICREG(ALC_MISC_CNTRL)) & ~ENABLE_SYNC_C_S)); 

/*	always load mainline MTPE code because BIOS may
	   have been running an incompatible version;
	   this is moved from middle of this routine
	   after v52.							*/
	if (sh_load_sequencer(c, SH_NORMAL_SEQ_CODE))
		return(CANNOT_LOAD_SEQUENCER);

/*	Start the mainline sequencer code				*/
	sh_start_mainline_sequencer(c);
	   
	return(0);
}

/*
*
* sh_prepare_config
*
* This routine will prepare before collecting configuration information
*
* Return Value: none
*		   
* Parameters:	control_blk pointer
*
* Remarks:	This routine should only be called at adapter init time.
*
*/
static void
sh_prepare_config(struct control_blk *c)
{
	register REGISTER base = c->base;

/*	Make sure the chip is paused and interrupt is disabled		*/
	OUTBYTE(AICREG(CMC_HCTL), HPAUSE);
}

/*
*
* sh_init_hardware
*
* This routine will initialize hardware configuration which is
* required at initialization time
*
* Return Value: return 0 or BAD_DONEQ_SIZE
*		   
* Parameters:	control_blk pointer
*
* Remarks:	This routine should only be called at adapter init time
*
*/
static INT
sh_init_hardware(struct control_blk *c)
{
	register REGISTER base = c->base;
	BUS_ADDRESS busAddress;
	UBYTE val;

/*	set up address of top of idle loop				*/
/*	multiply by 4 to convert from Sequencer to RISC address		*/
	val = (IDLE_LOOP0 >> 2) & 0xff;
/*	set up lower 8 bits						*/
	OUTBYTE(AICREG(TILPADDR0), val);

/*	get upper 2 address bits and convert again			*/
	val = (IDLE_LOOP0 >> 10) & 0xff;
/*	preserve any trace selections from run time			*/
	val |= (INBYTE(AICREG(TILPADDR1)) & TILPADDR_TRACE_MASK);
	OUTBYTE(AICREG(TILPADDR1), val);

	/* Turn off led later */

	/* Need to store self port LogIn parameters (some pulled */
	/* from seeprom), in scratch  */

	/* init sequencer registers */
	OUTBYTE(AICREG(SINDEX),0x00);	/* init with pvardat address */
	OUTBYTE(AICREG(DINDEX),0x00);	/* lower byte to beginning = 0 */
	OUTBYTE(AICREG(SINDEX+1),0x01); /* upper byte 1 = address 100 */
	OUTBYTE(AICREG(DINDEX+1),0x01);

	
/*	This is necessary because of a bug				*/
	val = INPCIBYTE(PCIREG(DEVCONFIG),c->ulm_info);
	OUTPCIBYTE(PCIREG(DEVCONFIG), val|CIP_RD_DIS,c->ulm_info);

	c->dma->stat0 = 0;
	c->dma->stat1 = 0;
	c->error = 0;

	OUTBYTE(AICREG(MODE_PTR),CMC_MODE4);
	busAddress = ulm_kvtop((VIRTUAL_ADDRESS) c->s->dma, c->ulm_info);

/*	If low/high bus address regs not contiguous, will need to be changed */
	sh_output_paddr(c, CIP_DMA_LADR0, busAddress, 0);
	OUTBYTE(AICREG(CMC_HST_RTC1), CIP_DMAEN);

	OUTBYTE(AICREG(MODE_PTR),CMC_MODE4);
	OUTBYTE(AICREG(IPEN0),IPEN0_INIT_STATE);/* initialize Intrs	    */
	OUTBYTE(AICREG(IPEN1),IPEN1_INIT_STATE);		

	OUTWORD(AICREG(SNTCB_QOFF0),0);   

	/* According to Emerald Init instructions */
	OUTWORD(AICREG(SDTCB_QOFF0),0);

/*	initialize queue touch register					*/
	OUTWORD(AICREG(HNTCB_QOFF),0);

	/* Host block Init */
	if (c->s->flag & SH_USE_HCOMMAND1)
	{
		OUTBYTE(AICREG(HCOMMAND1), c->s->hcommand1);
	}
	else
	{
		OUTBYTE(AICREG(HCOMMAND1), HCOMMAND1_DEFAULT);
	}		
	OUTBYTE(AICREG(HCOMMAND0), HCOMMAND0_DEFAULT);

	OUTBYTE(AICREG(PVARPTR),0x00);	  /* select PVAR page 0 */

	/* init receive payload buff size			*/
	OUTBYTE(AICREG(MODE_PTR),(UBYTE) CMC_MODE2);
	OUTBYTE(AICREG(RPB_PLD_SIZE),(UBYTE) sh_find_payload_size(
		(int)c->s->recv_payload_size));

/*	set up payload buffer		XXX			*/
/*	temporarily enable detection of loss of signal here	*/
	OUTBYTE(AICREG(RPB_BUF_CTL), RPB_BUF_CTL_DEFAULT);

	/* SPB block Init  */

	/* NONE 10/26, HIM init params. in TCB, seq loads in SPB regs */

	/* RFC block Init  */
	OUTBYTE(AICREG(MODE_PTR),CMC_MODE3);
	OUTBYTE(AICREG(DAT_MSKC), DAT_MSKC_DEFAULT);
	OUTBYTE(AICREG(DAT_MSKD), DAT_MSKD_DEFAULT);
	OUTBYTE(AICREG(RFC_MODE), RFC_MODE_DEFAULT);

	/* SFC block Init */
	OUTBYTE(AICREG(MODE_PTR), CMC_MODE1);
	OUTBYTE(AICREG(AVAIL_RCV_BUF), 0);
	OUTBYTE(AICREG(SFC_CMD0),DISABLE_ALC_CONTROL);

/*	Set LIP timeout in ALC	(mode 1)				*/
	OUTBYTE(AICREG(AL_TIME_OUT) , c->s->lip_timeout);

	OUTBYTE(AICREG(OUR_AIDB0),0x00);
	OUTBYTE(AICREG(OUR_AIDB1),0x00);
	OUTBYTE(AICREG(OUR_AIDB2),0x00);
	OUTBYTE(AICREG(OUR_AIDB_CFG),0x00);  /* Address B not valid */

	OUTBYTE(AICREG(ACK_WAIT_TIME),0x00);  /*  class-2,1	*/
	OUTBYTE(AICREG(AL_LAT_TIME) , 0);

	return (0);
}

/*
*
* 
*
* Return code for receive payload buffer
*
* Return Value: none
*		   
* Parameters:	In: user value from shim_config
		Out: code
*
* Remarks:	Will return largest value, or valid user value
*
*/
static INT
sh_find_payload_size(int size)
{
	int code, i;
#ifndef BIOS

	static int payloads[] = {128, 256, 512, 1024, 2048};

#else

	int payloads[] = {128, 256, 512, 1024, 2048};

#endif
/*	init receive payload buff size			*/
	for (i=0; i < sizeof(payloads) / sizeof(int);i++) {
		if (payloads[i] == size) {
			code = i;
			break;
		}
	}

/*	take largest size if user does not have good value */
	if (i == (sizeof(payloads) / sizeof(int)))
			code = RPB_PLD_2048;
	
	return (code);
}

/*
* 
* sh_enable_seq_breakpoint
* 
*
* Return Value: none
*		   
* Parameters:	control_blk pointer
*
* Remarks:	to enable breakpoints on sequencer instructions,
*		or on the not of BRKDIS, i. e. to enable breakpoint stops
*		turn off BRKDIS
*/
void
sh_enable_seq_breakpoint(struct control_blk *c)
{
	register REGISTER base = c->base;

	OUTBYTE(AICREG(BRKADDR1),INBYTE(AICREG(BRKADDR1)) & ~BRKDIS);
}

