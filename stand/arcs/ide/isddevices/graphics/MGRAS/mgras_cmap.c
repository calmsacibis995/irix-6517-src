/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  CMAP Diagnostics.
 *  	Address Bus Test  :- Walking Ones and Zeros.
 *  	Data Bus Test :- Walking Ones & Zeros .
 *  	Full Memory Pattern Test.
 *
 *  $Revision: 1.70 $
 */

#include "libsc.h"
#include "libsk.h"
#include "uif.h"
#include "ide_msg.h"
#include <math.h>
#include "sys/mgrashw.h"
#include "mgras_hq3.h"
#include "mgras_diag.h"
#include "parser.h"

/*
 * The following constant makes sure that the tests
 * use the general purpose scratch buffer rather than
 * a dedicated buffer for storing cmap values
 */
#define CmapBuf mgras_scratch_buf
#if defined(IP30)
#define new_mgras_cmapFIFOWAIT(base)                    \
    {                               \
        mgras_BFIFOWAIT(base,HQ3_BFIFO_MAX);            \
        while ((base->cmap0.status & 0x04));            \
        while ((base->cmap1.status & 0x04));            \
    }
#endif
#ifdef MFG_USED
__uint32_t	mg_cmap_count = 0;
void
mg_clear_cmap_count(void)
{
	mg_cmap_count = 0;
}
void mg_disp_cmap_count(void)
{
	msg_printf(SUM, "mg_cmap_count %d\n", mg_cmap_count);
}
#endif

#undef mgras_CmapSetup
void
mgras_CmapSetup(mgras_hw *mgbase)
{
	mgras_xmapSetPP1Select(mgbase,MGRAS_XMAP_WRITEALLPP1);
	/* Select Second byte of the config reg :- for AutoInc */
	mgras_xmapSetAddr(mgbase, 0x1) ;        /* Do NOT REMOVE THIS */
	mgras_xmapSetConfig(mgbase, 0xff000000); /* Hack for Auto Inc */
}

/***********************************************************************/

void
mgras_LoadCmap(mgras_hw *mgbase, __uint32_t StAddr, char *data,  __uint32_t length) 
{
	 __uint32_t i, addr;

	addr = MGRAS_8BITCMAP_BASE + StAddr;
	/* First Drain the BFIFO - DCB FIFO*/
	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);


	for (i = 0; i < length; i++, addr++, data+=4) {

        /* Setting the address twice every time we check cmapFIFOWAIT	 */
        /* is a s/w work around for the cmap problem. The address	 */
        /* is lost ramdomly when the bus is switched from write to	 */
        /* read mode or vice versa.					 */


                /* Every 16 colors, check cmap FIFO.
                 * Need to give 2 dummy writes after the read.
                 */
                if ((i == 0) || ((i & 0x1F) == 0x1F)) {
			mgras_cmapToggleCblank(mgbase, 0); /* Enable cblank */
                        /* cmapFIFOWAIT calls BFIFOWAIT */
#if (defined(IP30))
                        new_mgras_cmapFIFOWAIT(mgbase);
#else
                        mgras_cmapFIFOWAIT(mgbase);
#endif
			mgras_cmapToggleCblank(mgbase, 1); /* disable cblank*/

                        mgras_cmapSetAddr(mgbase, addr);
                        mgras_cmapSetAddr(mgbase, addr);

                }

#if 0
                if ((i & 0xF) == 0xF)
                {
                        /* Wait before next stream of 4*16 writes */
                        mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
                }
#endif


		mgras_cmapSetRGB(mgbase, *(data+1), *(data+2), *(data+3) );
		msg_printf(6, "LoadCmap addr %x R %x G %x B %x\n",
			addr,*(data+1), *(data+2), *(data+3));
	/* flush_cache(); */
	 } /* for */

	mgras_cmapToggleCblank(mgbase, 0); /* Enable cblank */
#if 0 /* HQ4 */
	 flush_cache(); 
#endif
       /* cmapFIFOWAIT calls BFIFOWAIT */
#if (defined(IP30))
       new_mgras_cmapFIFOWAIT(mgbase);
#else
       mgras_cmapFIFOWAIT(mgbase);
#endif
}

static ushort_t cmap_addr_patrn [] = {
	0x0001,	0x0002,	0x0004,	0x0008,
	0x0010,	0x0020,	0x0040,	0x0080,
	0x0100,	0x0200,	0x0400,	0x0800,
	0x1000,

	0xFFFE,	0xFFFE,	0xFFFB,	0xFFF7,
	0xFFEF,	0xFFEF,	0xFFBF,	0xFF7F,
	0xFEFF,	0xFEFF,	0xFBFF,	0xF7FF,
	0x0FFF,
};

void
mgras_ReadCmap( __uint32_t CmapID,  __uint32_t StAddr, char *data,  __uint32_t length) 
{
	char dummy, r, g, b;
	 __uint32_t i, addr, count;

	addr = MGRAS_8BITCMAP_BASE + StAddr ;
	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);

	/********************************************************************/
        /* 	First read operation immediately after a write is not       */
        /* 	guaranteed. So we start read at 8191 and ignore the first   */
        /* 	read. The counter automatically resets to 0 after 8K.	    */
	/********************************************************************/

/*			us_delay(1000); */

	mgras_cmapToggleCblank(mgbase, 1); /* disable cblank*/

	mgras_cmapSetAddr(mgbase, 8191);
			/* us_delay(1000); */
	mgras_cmapGetRGB(mgbase, CmapID, *(data+1), *(data+2),*(data+3));
	/* flush_cache(); */
	msg_printf(DBG, "Throw away first read\n");

	/*********************************************************************/
	/*	Rev 2.0 Cmaps Only :-					     */
	/*	  set cmap in diag read mode				     */
	/*********************************************************************/
	mgras_cmapSetDiag(mgbase, CmapID, 0);


	for (i = 0; i < length; i++, addr++, data+=4) {
		mgras_cmapSetAddr(mgbase, addr); /* Do not use auto inc mode */
			/* us_delay(1000); */
		mgras_cmapGetRGB(mgbase, CmapID, *(data+1), *(data+2),*(data+3));
#ifdef MFG_USED
		count = 10;
		while (count--) {
		    r = *(data+1); g = *(data+2); b = *(data+3);
		    *(data+1) = *(data+2) = *(data+3) = 0x0;
		    mgras_cmapSetAddr(mgbase, addr); 
			/* us_delay(10000); */
		    mgras_cmapGetRGB(mgbase, CmapID, *(data+1), 
				*(data+2),*(data+3));
#if 0 /* HQ4 */
			 flush_cache(); 
#endif
		    if((r == *(data+1))&&(g == *(data+2))&&(b == *(data+3))) {
			break;

			/* flush_cache(); */
		    }
		    mg_cmap_count++;
		}
#endif
	}
	mgras_cmapToggleCblank(mgbase, 0); /* Enable cblank */

	/*********************************************************************/
	/*	Rev 2.0 Cmaps Only :-					     */
	/*	  Read cmap diag read mode (Take it out of the diag mode )   */
	/*********************************************************************/
	mgras_cmapGetDiag(mgbase, CmapID, dummy);
}

__uint32_t 
_mgras_CmapAddrsBusTest( __uint32_t WhichCmap)
{
	 __uint32_t   	addr    = 0;
	 __uint32_t 	index  	= 0;
	 __uint32_t 	errors 	= 0;
	 __uint32_t 	length 	= 1;
	 __uint32_t 	mask   	= 0xffffff;
	 __uint32_t 	exp    	= 0xfeedbaad;
	 __uint32_t 	rcv    	= 0xdeadbeef;
	
	MGRAS_GFX_CHECK();
	mgras_CmapSetup(mgbase);
	msg_printf(VRB, "CMAP %d Address Bus Test\n", WhichCmap);
	for(index=0; index <sizeof(cmap_addr_patrn)/sizeof(ushort_t); index++) {
		exp = ~cmap_addr_patrn[index];
		addr = cmap_addr_patrn[index] & CMAP_RAM_MASK;
		mgras_LoadCmap(mgbase, addr, (char *)&exp, length);
	}

	for(index=0; index <sizeof(cmap_addr_patrn)/sizeof(ushort_t) ; index++) {
		exp = ~cmap_addr_patrn[index];
		rcv = 0xdeadbeef;
		addr = cmap_addr_patrn[index] & CMAP_RAM_MASK;
		mgras_ReadCmap(WhichCmap, addr, (char*)&rcv, length);
		CMAP_COMPARE("Cmap_AddrsBus_Test ", WhichCmap, addr++,exp, rcv , mask, errors);
	}

	if (WhichCmap){
		REPORT_PASS_OR_FAIL((&BackEnd[CMAP1_ADDR_BUS_TEST]), errors);
	} else {
		REPORT_PASS_OR_FAIL((&BackEnd[CMAP0_ADDR_BUS_TEST]), errors);
	}
}

__uint32_t 
_mgras_CmapAddrsUniqTest( __uint32_t WhichCmap)
{
	 __uint32_t   	addr    = 0;
	 __uint32_t 	index  	= 0;
	 __uint32_t 	errors 	= 0;
	 __uint32_t 	mask   	= 0xffffff;
	 __uint32_t 	exp;
	 __uint32_t 	rcv;
	
	
	MGRAS_GFX_CHECK() ;
	mgras_CmapSetup(mgbase);
	msg_printf(VRB, "CMAP%d Address Uniqueness Test\n", WhichCmap);

	/* A[i] = i	*/
	for(addr=0, index=0; index < MGRAS_CMAP_NCOLMAPENT; addr++, index++) {
                exp = index & 0xff;
                exp = (exp << 16) | (exp << 8) | exp ;
		mgras_LoadCmap(mgbase, addr, (char *)&exp, 1);
	}

	for(addr=0, index=0; index < MGRAS_CMAP_NCOLMAPENT ; addr++, index++) {
                exp = index & 0xff;
                exp = (exp << 16) | (exp << 8) | exp ;
		rcv = 0xdeadbeef;
		mgras_ReadCmap(WhichCmap, addr, (char*)&rcv, 1);
		CMAP_COMPARE("Cmap_AddrsUniq_Test ", WhichCmap, addr,exp, rcv , mask, errors);
	}

	/* A[i] = ~i	*/
	for(addr=0, index=0; index < MGRAS_CMAP_NCOLMAPENT; addr++, index++) {
                exp = (~index) & 0xff;
                exp = (exp << 16) | (exp << 8) | exp ;
		mgras_LoadCmap(mgbase, addr, (char *)&exp, 1);
	}

	for(addr=0, index=0; index < MGRAS_CMAP_NCOLMAPENT ; addr++, index++) {
                exp = (~index) & 0xff;
                exp = (exp << 16) | (exp << 8) | exp ;
		rcv = 0xdeadbeef;
		mgras_ReadCmap(WhichCmap, addr, (char*)&rcv, 1);
		CMAP_COMPARE("Cmap_AddrsUniq_Test ", WhichCmap, addr,exp, rcv , mask, errors);
	}

	if (WhichCmap){
		REPORT_PASS_OR_FAIL((&BackEnd[CMAP1_ADDR_UNIQ_TEST]), errors);
	} else{
		REPORT_PASS_OR_FAIL((&BackEnd[CMAP0_ADDR_UNIQ_TEST]), errors);
	}
}


#if HQ4
static __uint32_t dcbdma_buffer[MGRAS_CMAP_NCOLMAPENT*2];
__uint32_t *dcbdma_buffer_ptr;
extern __uint32_t   _mgras_rttexture_setup(uchar_t, uchar_t,__uint32_t,__uint32_t *); 
void
_mgras_create_buffer ()
{
   __paddr_t    pgMask = ~0x0;

   pgMask = PAGE_MASK(pgMask);

	dcbdma_buffer_ptr = (__uint32_t *)((((__paddr_t)dcbdma_buffer) + 0x1000) & pgMask) ;
}

void
_fill_cmapbuffers (__uint32_t *to, __uint32_t *from, __uint32_t to_len, __uint32_t from_len)
{
 __uint32_t   i, j;
   for (i = 0, j = 0; i < to_len ; i++, j = i%from_len ) {
   *(to+i) = *(from+j) ;
   }
}
#endif

__uint32_t
_mgras_CmapPatrnTest1( __uint32_t , __uint32_t ,__uint32_t ); 
#if (defined(IP30))
__uint32_t
_mgras_CmapPatrnTest1( __uint32_t WhichCmap,  __uint32_t saddr, __uint32_t eaddr )
{
	 __uint32_t 	patrn1, patrn2, patrn3, patrn4, patrn5, patrn6;
	 __uint32_t 	addr 	= 0;
	 __uint32_t 	loop	= 0;
	 __uint32_t 	index	= 0;
	 __uint32_t 	errors 	= 0;
	 __uint32_t  	length	= 1;
	 __uint32_t 	mask 	= 0xffffff;
	 __uint32_t 	rcv 	= 0xdeadbeef;
	 __uint32_t Cmap0Rev;
	char *ptr ;
	

	msg_printf(VRB, "saddr %d eaddr %d \n", saddr, eaddr);
	MGRAS_GFX_CHECK();

	/* Read CMAP0 Revision(Assumin CMAP1 Rev is the same as CMAP0) */
	mgras_cmapToggleCblank(mgbase, 1); /* disable cblank*/
	mgras_cmapGetRev(mgbase, Cmap0Rev, 0);
	Cmap0Rev &= 0x7;

	mgras_CmapSetup(mgbase);
	msg_printf(VRB, "CMAP%d Pattern Test\n", WhichCmap);
	for(loop=0; loop < 6; loop++) {
		patrn1 = *(patrn + 0 + loop); 
		patrn2 = *(patrn + 1 + loop); 
		patrn3 = *(patrn + 2 + loop);
		patrn4 = *(patrn + 3 + loop);
		patrn5 = *(patrn + 4 + loop);
		patrn6 = *(patrn + 5 + loop);

		msg_printf(VRB, "Loop %d Writing Pattern\n", loop);
		msg_printf(VRB,"0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",patrn1,patrn2,patrn3,patrn4,patrn5,patrn6);
		mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
	
		mgras_cmapToggleCblank(mgbase, 0);
        new_mgras_cmapFIFOWAIT(mgbase);

	/* If Old rev. did the same thing as before */
        if (Cmap0Rev  < CMAP_REV_F)
		mgras_cmapToggleCblank(mgbase, 1);  /* disable cblank */

 		mgras_cmapSetAddr(mgbase, saddr);
		mgras_cmapSetAddr(mgbase, saddr);

		for(index=0, addr=saddr; addr <= eaddr; addr+=6) {
			/* Write to Both CMAPS :- Broadcast Mode Write */
		/* If new cmap Checkfifo empty before every 64 words write */
		if ((Cmap0Rev  >= CMAP_REV_F) && !(index % 64)) 
			mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
		if (addr <= eaddr)
		 	{ptr = (char*)&patrn1; mgras_cmapSetRGB(mgbase, *(ptr+1), *(ptr+2), *(ptr+3) );}
		if (addr+1 <= eaddr)
			{ptr = (char*)&patrn2; mgras_cmapSetRGB(mgbase, *(ptr+1), *(ptr+2), *(ptr+3) );}
		if (addr+2 <= eaddr)
			{ptr = (char*)&patrn3; mgras_cmapSetRGB(mgbase, *(ptr+1), *(ptr+2), *(ptr+3) );}
		if (addr+3 <= eaddr)
			{ptr = (char*)&patrn4; mgras_cmapSetRGB(mgbase, *(ptr+1), *(ptr+2), *(ptr+3) );}
		if (addr+4 <= eaddr)
			{ptr = (char*)&patrn5; mgras_cmapSetRGB(mgbase, *(ptr+1), *(ptr+2), *(ptr+3) );}
		if (addr+5 <= eaddr)
			{ptr = (char*)&patrn6; mgras_cmapSetRGB(mgbase, *(ptr+1), *(ptr+2), *(ptr+3) );}
		index += 6;
		}
		mgras_cmapToggleCblank(mgbase, 0); /* enable cblank*/
		new_mgras_cmapFIFOWAIT(mgbase);
	
		rcv = 0xdead1111;

		/* ReadBack all the CMAP's */
		mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);
		mgras_cmapToggleCblank(mgbase, 1);
		mgras_cmapSetAddr(mgbase, 8191);
		ptr = (char*)(&rcv); mgras_cmapGetRGB(mgbase, WhichCmap, *(ptr+1), *(ptr+2),*(ptr+3));
		mgras_cmapSetDiag(mgbase, WhichCmap, 0);
		mgras_cmapSetAddr(mgbase, saddr);  /* Do not use auto inc mode */

		for(index=0, addr=saddr; addr <= eaddr; addr+=6) {
			if (addr <= eaddr){
			ptr = (char*)(&rcv); mgras_cmapGetRGB(mgbase, WhichCmap, *(ptr+1), *(ptr+2),*(ptr+3));
			CMAP_COMPARE("Cmap_Patrn_Test", WhichCmap, (addr+0), patrn1, rcv, mask, errors);
			}
			if (addr+1 <= eaddr){
			ptr = (char*)(&rcv); mgras_cmapGetRGB(mgbase, WhichCmap, *(ptr+1), *(ptr+2),*(ptr+3));
			CMAP_COMPARE("Cmap_Patrn_Test", WhichCmap, (addr+1),patrn2, rcv, mask, errors);
			}
			if (addr+2 <= eaddr){
			ptr = (char*)(&rcv); mgras_cmapGetRGB(mgbase, WhichCmap, *(ptr+1), *(ptr+2),*(ptr+3));
			CMAP_COMPARE("Cmap_Patrn_Test", WhichCmap, (addr+2),patrn3, rcv, mask, errors);
			}
			if (addr+3 <= eaddr){
			ptr = (char*)(&rcv); mgras_cmapGetRGB(mgbase, WhichCmap, *(ptr+1), *(ptr+2),*(ptr+3));
			CMAP_COMPARE("Cmap_Patrn_Test", WhichCmap, (addr+3),patrn4, rcv, mask, errors);
			}
			if (addr+4 <= eaddr){
			ptr = (char*)(&rcv); mgras_cmapGetRGB(mgbase, WhichCmap, *(ptr+1), *(ptr+2),*(ptr+3));
			CMAP_COMPARE("Cmap_Patrn_Test", WhichCmap, (addr+4),patrn5, rcv, mask, errors);
			}
			if (addr+5 <= eaddr){
			ptr = (char*)(&rcv); mgras_cmapGetRGB(mgbase, WhichCmap, *(ptr+1), *(ptr+2),*(ptr+3));
			CMAP_COMPARE("Cmap_Patrn_Test", WhichCmap, (addr+5),patrn6, rcv, mask, errors);
			}
		}
	mgras_cmapToggleCblank(mgbase, 0); /* disable cblank*/
	mgras_cmapGetDiag(mgbase, WhichCmap, *ptr); 
	}
	return (errors);
}
#else
__uint32_t
_mgras_CmapPatrnTest1( __uint32_t WhichCmap,  __uint32_t saddr, __uint32_t eaddr )
{
         __uint32_t     addr    = 0;
         __uint32_t     loop    = 0;
         __uint32_t     index   = 0;
         __uint32_t     errors  = 0;
         __uint32_t     length  = 1;
         __uint32_t     mask    = 0xffffff;
         __uint32_t     rcv     = 0xdeadbeef;
         __uint32_t     *patrn1, *patrn2, *patrn3, *patrn4, *patrn5, *patrn6;
        
        MGRAS_GFX_CHECK();
        mgras_CmapSetup(mgbase);
        msg_printf(VRB, "CMAP%d Pattern Test\n", WhichCmap);
        for(loop=0; loop < 5; loop++) {
                patrn1 = (patrn + loop) ; 
                patrn2 = patrn1 + 1;
                patrn3 = patrn1 + 2;
                patrn4 = patrn1 + 3;
                patrn5 = patrn1 + 4;
                patrn6 = patrn1 + 5;
                msg_printf(VRB, "Loop %d Writing Pattern\n", loop);
                msg_printf(VRB,"0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",*patrn1, *patrn2, *patrn3, *patrn4,*patrn5,*patrn6);

        if (_mgras_dcbdma_enabled) {
#if HQ4
#define CMAP_DCB_DEV 3
     mgbase->dcb_dma_con =
                ((CMAP_DCB_DEV << 6) | (2 << 3) | (0 << 2) | 3 ) ;
        us_delay(10000);
                _mgras_create_buffer ();
         mgras_cmapSetAddr(mgbase, (addr+MGRAS_8BITCMAP_BASE)); 
        /* First Drain the BFIFO - DCB FIFO*/
         mgras_cmapFIFOWAIT_which(WhichCmap, mgbase);
        _fill_buffers((uchar_t *)dcbdma_buffer_ptr, (uchar_t *)(patrn+loop), 4*MGRAS_CMAP_NCOLMAPENT, 4*6);
     _mgras_rttexture_setup(TEX_DCB_IN, 0, MGRAS_CMAP_NCOLMAPENT*sizeof(__uint32_t), dcbdma_buffer_ptr);
        us_delay(10000);
#endif
        } else {

                for(index=0, addr=0; index< MGRAS_CMAP_NCOLMAPENT/6; index++) {
                        /* Write to Both CMAPS :- Broadcast Mode Write */
/* XXX */ 
                        mgras_LoadCmap(mgbase, addr++, (char*)patrn1, length);
                        mgras_LoadCmap(mgbase, addr++, (char*)patrn2, length);
                        mgras_LoadCmap(mgbase, addr++, (char*)patrn3, length);
                        mgras_LoadCmap(mgbase, addr++, (char*)patrn4, length);
                        mgras_LoadCmap(mgbase, addr++, (char*)patrn5, length);
                        mgras_LoadCmap(mgbase, addr++, (char*)patrn6, length);
                }
                mgras_LoadCmap(mgbase, addr++, (char*)patrn1, length);
                mgras_LoadCmap(mgbase, addr++, (char*)patrn2, length);
        }

                rcv = 0xdead1111;
        if (_mgras_dcbdma_enabled ) {
#if HQ4
                for ( addr = 0; addr < MGRAS_CMAP_NCOLMAPENT;  addr++) {
                mgras_ReadCmap(WhichCmap, addr, (char*)&rcv, length);   
                CMAP_COMPARE("Cmap_Patrn_Test ", WhichCmap, addr, (*(dcbdma_buffer_ptr+addr) >> 8), rcv, mask, errors);
                }
#endif
        } else {
                /* ReadBack all the CMAP's */
                for(index=0, addr=0; index< MGRAS_CMAP_NCOLMAPENT/6; index++) {
                        mgras_ReadCmap(WhichCmap, addr++, (char*)&rcv, length);
                        CMAP_COMPARE("Cmap_Patrn_Test ", WhichCmap, addr, *patrn1, rcv, mask, errors);

                        mgras_ReadCmap(WhichCmap, addr++, (char*)&rcv, length);
                        CMAP_COMPARE("Cmap_Patrn_Test ", WhichCmap, addr,*patrn2, rcv, mask, errors);

                        mgras_ReadCmap(WhichCmap, addr++, (char*)&rcv, length);
                        CMAP_COMPARE("Cmap_Patrn_Test ", WhichCmap, addr,*patrn3, rcv, mask, errors);

                        mgras_ReadCmap(WhichCmap, addr++, (char*)&rcv, length);
                        CMAP_COMPARE("Cmap_Patrn_Test ", WhichCmap, addr,*patrn4, rcv, mask, errors);

                        mgras_ReadCmap(WhichCmap, addr++, (char*)&rcv, length);
                        CMAP_COMPARE("Cmap_Patrn_Test ", WhichCmap, addr,*patrn5, rcv, mask, errors);

                        mgras_ReadCmap(WhichCmap, addr++, (char*)&rcv, length);
                        CMAP_COMPARE("Cmap_Patrn_Test ", WhichCmap, addr,*patrn6, rcv, mask, errors);
                }
                mgras_ReadCmap(WhichCmap, addr++, (char*)&rcv, length);
                CMAP_COMPARE("Cmap_Patrn_Test ", WhichCmap, addr,*patrn1, rcv, mask, errors);

                mgras_ReadCmap(WhichCmap, addr++, (char*)&rcv, length);
                CMAP_COMPARE("Cmap_Patrn_Test ", WhichCmap, addr,*patrn2, rcv, mask, errors);
        }
        }

        if (WhichCmap){
                REPORT_PASS_OR_FAIL((&BackEnd[CMAP1_PATRN_TEST]), errors);
        } else{
                REPORT_PASS_OR_FAIL((&BackEnd[CMAP0_PATRN_TEST]), errors);
        }
}
#endif

__uint32_t 
_mgras_CmapDataBusTest( __uint32_t WhichCmap)
{
	 __uint32_t 	addr 	= 0;
	 __uint32_t  	length  = 1;
	 __uint32_t 	errors 	= 0;
	 __uint32_t  	index	= 0;
	 __uint32_t 	mask 	= 0xffffff;
	 __uint32_t  	exp	= 0xdeadbeef;
	 __uint32_t  	rcv	= 0xdeadbeef;

	MGRAS_GFX_CHECK() ;
	mgras_CmapSetup(mgbase);
	msg_printf(VRB, "CMAP%d data bus test\n" ,WhichCmap);
	for(index=0; index<sizeof(walking_patrn)/sizeof(ushort_t); index++) {
		exp  = walking_patrn[index];
		mgras_LoadCmap(mgbase, addr, (char*)&exp, length);
		CORRUPT_DCB_BUS();
		rcv = 0xdeadbeef;
		mgras_ReadCmap(WhichCmap, addr, (char*)&rcv, length);

		/* Compare & Verify the Results */
		CMAP_COMPARE("Cmap_DataBus_Test ", WhichCmap, addr, walking_patrn[index], rcv, mask, errors);
	}

	if (WhichCmap){
		REPORT_PASS_OR_FAIL((&BackEnd[CMAP1_DATA_BUS_TEST]), errors);
	} else{
		REPORT_PASS_OR_FAIL((&BackEnd[CMAP0_DATA_BUS_TEST]), errors);
	}

}

__uint32_t 
_mgras_Cmap( __uint32_t WhichCmap) 
{
	 __uint32_t errors = 0, straddr=0, endaddr = 8191;

	errors += _mgras_CmapDataBusTest(WhichCmap);
	if (!errors) {
		errors += _mgras_CmapAddrsBusTest(WhichCmap);
	} else 
		return (errors) ;
	if (!errors) {
		errors += _mgras_CmapAddrsUniqTest(WhichCmap);
	}
	if (!errors) {
		errors += _mgras_CmapPatrnTest1(WhichCmap, straddr, endaddr);
	}

	REPORT_PASS_OR_FAIL((&BackEnd[CMAP0_TEST]), errors);
}

__uint32_t
mgras_CmapUniqTest(void) 
{
	 __uint32_t errors, rcv, exp;
	 __uint32_t Addr, WhichCmap ;
	 __uint32_t dummy, data;

	MGRAS_GFX_CHECK() ;
	mgras_CmapSetup(mgbase);
	msg_printf(VRB, "CMAP Uniqness test\n");

	/******************************************************************/
	/* 2.   Enable Only Cmap B 					  */	
	/* 3.	Write to a CmapB[Addr] = 0xValueB 			  */
	/* 4.	Enable Only Cmap A 					  */
	/* 5.	Read CmapA[Addr] 					  */
	/* 6.	Enable CmapB						  */
	/* 7.	Read CmapB[Addr]					  */
	/******************************************************************/

	errors = 0;
	Addr = 0x10;

	mgras_cmapToggleCblank(mgbase, 1); /* disable cblank*/

	mgras_BFIFOWAIT(mgbase,HQ3_BFIFO_MAX);

	mgras_cmapSetAddr(mgbase, Addr);
	data = 0xdddead;
	Mgras_WriteCmap(0, data);

	mgras_cmapSetAddr(mgbase, Addr);
	data = 0xfffeed;
	Mgras_WriteCmap(1, data);

	mgras_cmapToggleCblank(mgbase, 0); /* Enable cblank */


        /************************************************************************/
        /*      First read operation immediately after a write is not           */
        /*      guaranteed. So we start read at 8191 and ignore the firs        */
        /*      read. The counter automatically resets to 0 after 8K.           */
        /************************************************************************/
        mgras_cmapToggleCblank(mgbase, 1); /* disable cblank*/

        mgras_cmapSetAddr(mgbase, 8191);
        mgras_cmapGetRGB(mgbase, 0, dummy, dummy, dummy);
        mgras_cmapGetRGB(mgbase, 1, dummy, dummy, dummy);
        msg_printf(DBG, "Throw away first read\n");

        mgras_cmapToggleCblank(mgbase, 0); /* Enable cblank */

	/************************************************************************/
        /*      Rev 2.0 Cmaps Only :-                                           */
        /*        set cmap in diag read mode                                    */
        /************************************************************************/
        mgras_cmapSetDiag(mgbase, 0, 0);
        mgras_cmapSetDiag(mgbase, 1, 0);

	rcv = 0; exp = 0xdddead;
	WhichCmap = 0;
	mgras_ReadCmap(WhichCmap, Addr, (char *)&rcv, 1);
	CMAP_COMPARE("Cmap_UniqTest ", WhichCmap, Addr, exp, rcv, 0xffffff, errors);
	rcv = 0;
	exp = 0xfffeed;
	WhichCmap = 1;
	mgras_ReadCmap(WhichCmap, Addr, (char *)&rcv, 1);
	CMAP_COMPARE("Cmap_UniqTest ", WhichCmap, Addr, exp, rcv, 0xffffff, errors);

	/************************************************************************/
        /*      Rev 2.0 Cmaps Only :-                                           */
        /*        Read cmap diag read mode (Take it out of the diag mode )      */
        /************************************************************************/

	mgras_cmapGetDiag(mgbase, 0, dummy);
	mgras_cmapGetDiag(mgbase, 1, dummy);

	REPORT_PASS_OR_FAIL((&BackEnd[CMAP_UNIQ_TEST]), errors);
}

__uint32_t 
mgras_CmapRevision(void)
{
	 __uint32_t Cmap0Rev;
	 __uint32_t Cmap1Rev;
	 __uint32_t Exp_d;
	 __uint32_t Exp_e;
	 __uint32_t Exp_f;

	MGRAS_GFX_CHECK() ;
	mgras_CmapSetup(mgbase);

	Exp_f = CMAP_REV_F;
	Exp_d = CMAP_REV_D;
	Exp_e = CMAP_REV_E;

	/* Read CMAPA Revision */
	mgras_cmapToggleCblank(mgbase, 1); /* disable cblank*/

	mgras_cmapGetRev(mgbase, Cmap0Rev, 0);
	Cmap0Rev &= 0x7;
#if 0
	mgras_cmapToggleCblank(mgbase, 0); /* Enable cblank */
#endif

	/* Read CMAPB Revision */
#if 0
	mgras_cmapToggleCblank(mgbase, 1); /* disable cblank*/
#endif
	mgras_cmapGetRev(mgbase, Cmap1Rev, 1);
	Cmap1Rev &= 0x7;
	mgras_cmapToggleCblank(mgbase, 0); /* Enable cblank */

	msg_printf(DBG, "CMAP0 revision = %d CMAP1 revision = %d\n",
		Cmap0Rev, Cmap1Rev);

	if ((Cmap0Rev  > Exp_f) || (Cmap1Rev  > Exp_f)) {
		msg_printf(SUM,"WARNING CMAP REVISION GREATER THAN EXPECTED.\n"
			       "SOFTWARE MAY NOT BE COMPATIBLE\n");
	}

	if (Cmap0Rev  < Exp_d) {
		msg_printf(ERR, "CMAP Revisions Mismatch\n"
				"CMAP0 revision %d Should be %d or %d\n",
			Cmap0Rev, Exp_d, Exp_e);
		return (-1);
	}

	if (Cmap1Rev < Exp_d) {
		msg_printf(ERR, "CMAP Revisions Mismatch\n"
				"CMAP1 revision %d Should be %d or %d\n",
			Cmap1Rev, Exp_d, Exp_e);
		return (-1);
	}

	return(Cmap0Rev);
}

int
mgras_CmapAddrsBusTest(int argc, char **argv)
{
	 __uint32_t WhichCmap;

	GET_PATHID(WhichCmap);
	return(_mgras_CmapAddrsBusTest(WhichCmap));
}

int
mgras_CmapAddrsUniqTest(int argc, char **argv)
{
	 __uint32_t WhichCmap;

	GET_PATHID(WhichCmap);
	return(_mgras_CmapAddrsUniqTest(WhichCmap));
}

int
mgras_CmapPatrnTest(int argc, char **argv)
{
	 __uint32_t WhichCmap = 0, straddr = 0, endaddr = 8191, errors;
#if 0
	GET_PATHID(WhichCmap);
#else
	argc--; argv++;
   while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
        case 'l':
            if (argv[0][2]=='\0') {
                atob(&argv[1][0], (int*)&endaddr);
                argc--; argv++;
            } else {
                atob(&argv[0][2], (int*)&endaddr);
            }
            break;
        case 'f':
            if (argv[0][2]=='\0') {
                atob(&argv[1][0], (int*)&straddr);
                argc--; argv++;
            } else {
                atob(&argv[0][2], (int*)&straddr);
            }
            break;
        case 'w':
            if (argv[0][2]=='\0') {
                atob(&argv[1][0], (int*)&WhichCmap);
                argc--; argv++;
            } else {
                atob(&argv[0][2], (int*)&WhichCmap);
            }
            break;
        default: break;
    }
    argc--; argv++;
   }

#endif

	errors = _mgras_CmapPatrnTest1(WhichCmap, straddr, endaddr );
		if (errors) {
			msg_printf(SUM, " Failed  \n" );
		}
#if (defined(IP30))
	if (WhichCmap){
		REPORT_PASS_OR_FAIL((&BackEnd[CMAP1_PATRN_TEST]), errors);
	} else{
		REPORT_PASS_OR_FAIL((&BackEnd[CMAP0_PATRN_TEST]), errors);
	}
#endif
}

int
mgras_CmapDataBusTest(int argc, char **argv)
{
	 __uint32_t WhichCmap;

	GET_PATHID(WhichCmap);
	return(_mgras_CmapDataBusTest(WhichCmap));
}

int
mgras_Cmap(int argc, char **argv)
{
	 __uint32_t WhichCmap;
	
	GET_PATHID(WhichCmap);
	return(_mgras_Cmap(WhichCmap));
}


void
CmapLoad( ushort_t Ramp)
{
         __uint32_t index ;
         __uint32_t value;

	mgras_CmapSetup(mgbase);
        msg_printf(DBG, "CmapLoad Ramp = 0x%x\n", Ramp);
        switch (Ramp) {
                case LINEAR_RAMP :
                        for(index = 0; index < MGRAS_CMAP_NCOLMAPENT; index++) {
                                value = index & 0xff;
                                CmapBuf[index] = ((value << 16) | (value << 8) | value);
                        }
                        break;
                case RAMP_FLAT_0 :
                        for(index = 0; index < MGRAS_CMAP_NCOLMAPENT; index++)
                                CmapBuf[index] = 0x000000 ;
                        break;
                case RAMP_FLAT_5 :
                        for(index = 0; index < MGRAS_CMAP_NCOLMAPENT; index++)
                                CmapBuf[index] = 0x555555 ;
                        break;
                case RAMP_FLAT_F :
                        for(index = 0; index < MGRAS_CMAP_NCOLMAPENT; index++)
                                CmapBuf[index] = 0xffffff ;
                        break;
                case RAMP_FLAT_A :
                        for(index = 0; index < MGRAS_CMAP_NCOLMAPENT; index++)
                                CmapBuf[index] = 0xaaaaaa ;
                        break;
        }
                        msg_printf(DBG, "Leaving switch\n");

        mgras_LoadCmap(mgbase, 0x0, (char*)CmapBuf, MGRAS_CMAP_NCOLMAPENT);
        msg_printf(DBG, "CmapLoad Loaded\n");
}

#ifdef MFG_USED
int 
mgras_CmapProg(int argc, char **argv)
{
	 __uint32_t RampType = 0x0; 	

	if (argc == 1)
		RampType = 0x0;
	else
		atohex(argv[1], (int*)&RampType);

	if (RampType > RAMP_TYPES) {
		msg_printf(SUM, "mg_cmapprog RampType\n");
		msg_printf(SUM, "0x0 :LINEAR_RAMP\n");
		msg_printf(SUM, "0x1 : RAMP_FLAT_0\n");
		msg_printf(SUM, "0x2 : RAMP_FLAT_5\n");
		msg_printf(SUM, "0x3 : RAMP_FLAT_A\n");
		msg_printf(SUM, "0x4 : RAMP_FLAT_F\n");

		return -1;
	}

	CmapLoad(RampType);
	return(0);
}

int 
mgras_PokeCmap(int argc, char **argv)
{
	__uint32_t loopcount;
	 __uint32_t CmapReg;		/* cmap register offset */
	 __uint32_t  Data;		/* data */
	 __uint32_t CmapLength;
	struct range range;
	__uint32_t bad_arg = 1;
	char    **argv_save;

	MGRAS_GFX_CHECK() ;
	mgras_CmapSetup(mgbase);
	loopcount = 1;
	argv_save = argv;

	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'a':
				/* Address Range */
				if (argv[0][2]=='\0') {
					--bad_arg;
					if (!(parse_range(&argv[1][0], sizeof(ushort_t), &range))) {
						msg_printf(SUM, "Syntax error in address range\n");
						bad_arg++;
					}
					msg_printf(SUM, "start = 0x%x; end = 0x%x\n", range.ra_base, (range.ra_base + (range.ra_size*range.ra_count)));
					argc--; argv++;
				} else {
					--bad_arg;
					if (!(parse_range(&argv[0][2], sizeof(ushort_t), &range))) {
						msg_printf(SUM, "Syntax error in address range\n");
						bad_arg++;
					}
					msg_printf(SUM, "start = 0x%x; end = 0x%x\n", range.ra_base, (range.ra_base + (range.ra_size*range.ra_count)));
				}
					
				break;
			case 'd':
				/* Data to be Written */
				if (argv[0][2]=='\0') {	/* Skip White Space */
					atohex(&argv[1][0], (int*)&Data);
					argc--; argv++;
				} else {
					atohex(&argv[0][2], (int*)&Data);
				}
				break;
                        case 'l':
                                if (argv[0][2]=='\0') { /* has white space */
                                        atohex(&argv[1][0], (int*)&loopcount);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atohex(&argv[0][2], (int*)&loopcount);
                                }
                                if (loopcount <= 0) {
					msg_printf(SUM, "loopcount must be > 0\n");
					bad_arg++;
                                }
                                break;
                        default: 
				bad_arg++; 
				/* Shouldn't be here */
				msg_printf(SUM, "mgras_PokeCmap ERROR!\n");
				break;

		}
		argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
		msg_printf(SUM, "%s [-l loopcount] -a Addrs Range -d Data\n", argv_save[0]);
		return -1;
	}
	
	CmapReg = (__uint32_t)range.ra_base;
	CmapLength = (__uint32_t)range.ra_count * (sizeof(ushort_t));
	for(; loopcount > 0; --loopcount) {
		msg_printf(VRB, "%s Loop %d Loading Cmaps\n" ,argv_save[0], loopcount);
		CmapLength = (__uint32_t)range.ra_count * (sizeof(ushort_t));
		CmapReg = (__uint32_t)range.ra_base;
		for(; CmapLength > 0; CmapLength--) {
			mgras_LoadCmap(mgbase, CmapReg++, (char*)&Data, 1);
		}
	}

	return 0;

}

int 
mgras_PeekCmap(int argc, char **argv)
{
	__int32_t loopcount;
	 __uint32_t WhichCmap; 	/* Select PP1-CMAP Path */
	 __uint32_t CmapReg;		/* cmap register offset */
	 __uint32_t  *Data, *Data_save;		/* data */
	 __uint32_t index, CmapLength;
	struct range range;
	__uint32_t bad_arg = 1;
	char    **argv_save;

	MGRAS_GFX_CHECK() ;
	mgras_CmapSetup(mgbase);
	loopcount = 1;
	WhichCmap = 0;
	argv_save = argv;

	argc--; argv++;
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'a':
				/* Address Range */
				if (argv[0][2]=='\0') {
					--bad_arg;
					if (!(parse_range(&argv[1][0], sizeof(ushort_t), &range))) {
						msg_printf(SUM, "Syntax error in address range\n");
						bad_arg++;
					}
					argc--; argv++;
					msg_printf(DBG, "start = 0x%x; end = 0x%x\n", range.ra_base, (range.ra_base + (range.ra_size*range.ra_count)));
				} else {
					--bad_arg;
					if (!(parse_range(&argv[0][2], sizeof(ushort_t), &range))) {
						msg_printf(SUM, "Syntax error in address range\n");
						bad_arg++;
					}
					msg_printf(DBG, "start = 0x%x; end = 0x%x\n", range.ra_base, (range.ra_base + (range.ra_size*range.ra_count)));
				}
					
				break;
			case 'c':
				/* WhichCmap Select */
				if (argv[0][2]=='\0') {	/* Skip White Space */
					atohex(&argv[1][0], (int*)&WhichCmap);
					argc--; argv++;
				}
				else
					atohex(&argv[0][2], (int*)&WhichCmap);

				break;
                        case 'l':
                                if (argv[0][2]=='\0') { /* has white space */
                                        atohex(&argv[1][0], (int*)&loopcount);
                                        argc--; argv++;
                                }
                                else { /* no white space */
                                        atohex(&argv[0][2], (int*)&loopcount);
                                }
                                if (loopcount < 0) {
                                        msg_printf(SUM, "Error Loop must be > 0\n");
                                        bad_arg++;
                                }
                                break;
                        default: 
				bad_arg++; 
				msg_printf(SUM, "****Shouldn't be here!\n");
				break;

		}
		argc--; argv++;
	}

	if ((bad_arg) || (argc)) {
		msg_printf(SUM, "%s [-c WhichCmap] [-l loopcount] -a Addrs Range\n", argv_save[0]);
		return -1;
	}
	

	CmapReg = (__uint32_t)range.ra_base;
	CmapLength = (__uint32_t)range.ra_count * sizeof(ushort_t);
	if ((Data = ( __uint32_t *) malloc(CmapLength)) == NULL)  {
		msg_printf(SUM, "Cannot malloc buffer!\n");
		return -1;
	}

	Data_save = Data;
	for(; loopcount > 0; --loopcount) {
		Data = Data_save;
 		mgras_ReadCmap(WhichCmap, CmapReg, (char*)Data, CmapLength) ;

		Data = Data_save;
		for(index=0; index < CmapLength; index++) 
			msg_printf(VRB, "loop %d Addr= 0x%x Data= 0x%x\n" , loopcount, (CmapReg + index), *(Data + index));
	}

	return 0;

}
#endif /* MFG_USED */
