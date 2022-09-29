
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: prom_error.c
 *	save/restore of error registers. Other miscellaneous functionality
 *	for error reporting.
 */
#include <sys/types.h>
#include <sys/sbd.h>
#include <libkl.h>
#include <sys/SN/SN0/IP27.h>
#include "ip27prom.h"
#include "libasm.h"
#include "mdir.h"

extern int kl_error_check_power_on(void);

int
error_check_reset(void)
{
    int errors = 0;
    int crb;
    hubreg_t err;

    err = LOCAL_HUB_L(PI_ERR_INT_PEND);
    while (err) {
	errors++;
	err &= (err - 1);
    }

    err = LOCAL_HUB_L(NI_PORT_ERROR);
    err &= 0x3fff000000;
    while (err) {
	errors++;
	err &= (err - 1);
    }
    
    if (LOCAL_HUB_L(MD_PROTOCOL_ERROR) & PROTO_ERROR_VALID_MASK) 
	errors++;

    if (LOCAL_HUB_L(MD_MISC_ERROR) & MISC_ERROR_VALID_MASK)
	errors++;
	    
    if (LOCAL_HUB_L(MD_MEM_ERROR) & MEM_ERROR_VALID_UCE)
	errors++;

    if (LOCAL_HUB_L(MD_DIR_ERROR) & (DIR_ERROR_VALID_UCE |
				     DIR_ERROR_VALID_AE))
	errors++;
    
    for (crb = 0; crb < IIO_NUM_CRBS; crb++) {
	icrba_t  crba;
	crba.reg_value = LOCAL_HUB_L(IIO_ICRB_A(crb));
	if (crba.icrba_fields_s.error)
	    errors++;
    }
    
    if (errors > 15)
	return 0;
    return 1;
}

/*
 * Function	: error_save_reset
 * Parameters	: none
 * Purpose	: to save the reset values of hardware error registers for
 *		  later perusal.
 * Assumptions	: none
 * Returns	: none
 * NOTE		: This routine saves the error registers in a special area
 *		  in the cache, since memory has not yet been initialized.
 *		  This will be copied out to memory soon after it gets
 *		  initialized.
 */

void
error_save_reset(void)
{
    __psunsigned_t pod_error = POD_ERRORVADDR;

    *(__uint64_t *)pod_error = error_check_reset();
    pod_error += 8;

    /*
     * Link reset, spurious message and crazy bit are turned on at system
     * reset?
     */

    {
	hubreg_t nierr_mask;  
  
	LOCAL_HUB_S(PI_ERR_INT_PEND, (PI_ERR_SPUR_MSG_A | PI_ERR_SPUR_MSG_B));
	nierr_mask = 	
	    (NPE_LINKRESET | NPE_INTERNALERROR | NPE_BADMESSAGE | NPE_BADDEST 
	     | NPE_FIFOOVERFLOW | NPE_CREDITTO_MASK | NPE_TAILTO_MASK);

	if ((LOCAL_HUB_L(NI_PORT_ERROR) & nierr_mask) == NPE_LINKRESET)
	    LOCAL_HUB_L(NI_PORT_ERROR_CLEAR);

	LOCAL_HUB_S(IIO_IECLR, IECLR_CRAZY);
    }

    save_hub_pi_error((__psunsigned_t)LOCAL_HUB(0), (hub_pierr_t *)pod_error);
    pod_error += sizeof(hub_pierr_t);

    save_hub_md_error((__psunsigned_t)LOCAL_HUB(0), (hub_mderr_t *)pod_error);
    pod_error += sizeof(hub_mderr_t);

    save_hub_io_error((__psunsigned_t)LOCAL_HUB(0), (hub_ioerr_t *)pod_error);
    pod_error += sizeof(hub_ioerr_t);

    save_hub_ni_error((__psunsigned_t)LOCAL_HUB(0), (hub_nierr_t *)pod_error);
    pod_error += sizeof(hub_nierr_t);

    save_hub_err_ext((__psunsigned_t)LOCAL_HUB(0), (klhub_err_ext_t *) pod_error);
    pod_error += sizeof(klhub_err_ext_t);
}

/*
 * Function	: error_clear_hub_regs
 * Parameters	: none
 * Purpose	: clear the error registers in the hub
 * Assumptions	: none
 * Returns	: none
 */

void
error_clear_hub_regs(void)
{
    SD(LOCAL_HUB(PI_ERR_INT_PEND), -1);
    LD(LOCAL_HUB(PI_ERR_STATUS1_A_RCLR));
    LD(LOCAL_HUB(PI_ERR_STATUS1_B_RCLR));
    LD(LOCAL_HUB(MD_DIR_ERROR_CLR));
    LD(LOCAL_HUB(MD_MEM_ERROR_CLR));
    LD(LOCAL_HUB(MD_MISC_ERROR_CLR));
    LD(LOCAL_HUB(MD_PROTOCOL_ERROR_CLR));

    SD(LOCAL_HUB(IIO_IO_ERR_CLR), -1);

    LD(LOCAL_HUB(NI_PORT_ERROR_CLEAR));

}

/*
 * Function	: error_move_reset
 * Parameters	: none
 * Purpose	: move the reset error registers from cache to initialized
 *			memory.
 * Assumptions	: none
 * Returns	: none.
 */

void
error_move_reset(void)
{
    char *err_dump = (char *) TO_NODE(get_nasid(), IP27PROM_ERRDMP);

    memcpy(err_dump, (char *) POD_ERRORVADDR, POD_ERRORSIZE);
}


/*
 * Function	: error_log_reset
 * Parameters	: board pointer (klconfig)
 * Purpose	: to save the reset error state from early memory into klconfig
 * Assumptions	: none
 * Returns	: none
 */


void
error_log_reset(lboard_t *brd)
{
    klinfo_t *klhubp;
    klhub_err_t *klhub_err;
    hub_error_t *hub_err;
    hub_pierr_t *pi_err;
    hub_mderr_t *md_err;
    hub_ioerr_t *io_err;
    hub_nierr_t *ni_err;
    klhub_err_ext_t *klhub_err_ext;
    __psunsigned_t err_dump;
    __psunsigned_t poweron;
    int rc = 0;

    err_dump = TO_NODE(get_nasid(), IP27PROM_ERRDMP);

    poweron = err_dump;
    
    if (*(__uint64_t *)poweron == 0) {
	rc = 1;
    }
    err_dump += 8;


    if (klhubp = find_component(brd, NULL, KLSTRUCT_HUB)) {
        klhub_err = HUB_COMP_ERROR(brd, klhubp);
        hub_err = HUB_ERROR_STRUCT(klhub_err, KL_ERROR_LOG);
    }
    else {
	printf("error_log_reset: cant find hub component\n");
	return;
    }

    hub_err->reset_flag = rc;

    pi_err = &hub_err->hb_pi;
    memcpy((char *) pi_err, (char *) err_dump, sizeof(hub_pierr_t));
    err_dump += sizeof(hub_pierr_t);

    md_err = &hub_err->hb_md;
    memcpy((char *) md_err, (char *) err_dump, sizeof(hub_mderr_t));
    err_dump += sizeof(hub_mderr_t);

    io_err = &hub_err->hb_io;
    memcpy((char *) io_err, (char *) err_dump, sizeof(hub_ioerr_t));
    err_dump += sizeof(hub_ioerr_t);

    ni_err = &hub_err->hb_ni;
    memcpy((char *) ni_err, (char *) err_dump, sizeof(hub_nierr_t));
    err_dump += sizeof(hub_nierr_t);

    klhub_err_ext = (klhub_err_ext_t *) NODE_OFFSET_TO_K1(NASID_GET(brd),
                klhub_err->he_extension);
    memcpy((char *) klhub_err_ext, (char *) err_dump, sizeof(klhub_err_ext_t));
    err_dump += sizeof(klhub_err_ext_t);
    
    return;
}


void
error_show_reset(void)
{
    kl_error_show_log("Hardware Error State at System Reset\n",
		      "End Hardware Error State (at System Reset)\n");
}

/*
 * Function	: hubpi_stack_params
 * Parameters	: nasid -> nasid
 *		: off_p -> pointer to memory offset, start of stack.
 *		: size_p-> pointer to size of stack (as specified by the
 *			the hub pi)
 * Purpose	: none
 * Assumptions	: none
 * Returns	: none
 */

static void
hubpi_stack_params(nasid_t nasid, unsigned int *off_p, unsigned int *size_p)
{
    unsigned int offset;
    size_t size, size_per_cpu;
    unsigned int size_bytes;

    offset = PI_ERROR_OFFSET(nasid);
    size_per_cpu  = PI_ERROR_SIZE(nasid) >> 1;

    size_per_cpu >>= PI_STACK_SIZE_SHFT;
    size = 0;
    while (size_per_cpu) {
	size++;
	size_per_cpu >>= 1;
    }
    /*
     * get the size in bytes. If the offset is not on the size boundary,
     * switch to the next lower acceptable size AND set the offset to
     * the next higher boundary based on size. This ensures that we
     * always have the sufficient space available.
     */
    size_bytes =  ERR_STACK_SIZE_BYTES(size);
    if (offset & (size_bytes - 1)) {
	if (size) size--;
	size_bytes >>= 1;
	offset = ((offset + (size_bytes - 1)) & ~(size_bytes - 1));
    }

    *size_p = size;
    *off_p = offset;
}

/*
 * Function	: error_setup_pi_stack
 * Parameters	: nasid -> node id on which to program the pi stack values.
 * Purpose	: to setup the hubpi spooling so that we capture error info.
 *		  This is a requirement for HUB1 as well.
 * Assumptions	: none
 * Returns	: none
 */

void
error_setup_pi_stack(nasid_t nasid)
{
    unsigned int offset;
    unsigned int size;

    hubpi_stack_params(nasid, &offset, &size);

    if (*LOCAL_HUB(PI_CPU_PRESENT_A))
	*LOCAL_HUB(PI_ERR_STACK_ADDR_A) = (hubreg_t)offset;

    offset += ERR_STACK_SIZE_BYTES(size);

    if (*LOCAL_HUB(PI_CPU_PRESENT_B))
	*LOCAL_HUB(PI_ERR_STACK_ADDR_B) = (hubreg_t)offset;

    *LOCAL_HUB(PI_ERR_STACK_SIZE) = (hubreg_t)size;
}

void
error_clear_io6(void)
{
#if 0
    printf("fixme: error_clear_io6\n");
#endif
}

void
error_disable_checking(int sysad_chk, int dis_eintr, int master)
{
    hubreg_t sysad_reg;

    if (master) {
	sysad_reg = LD(LOCAL_HUB(PI_SYSAD_ERRCHK_EN));
	sysad_reg &= ~sysad_chk;
	SD(LOCAL_HUB(PI_SYSAD_ERRCHK_EN), sysad_reg);
    }
#if 0    
    if (dis_eintr)
	disable_cpu_intrs();
#endif
}


void
error_enable_checking(int sysad_chk, int en_eintr, int master)
{
    int en_sys_state;
    hubreg_t rev;

    if (master) {
	/*
	 * Enable sys_state only if both cpus are present.
	 */
	en_sys_state = sysad_chk & PI_SYSAD_ERRCHK_STATE;
	sysad_chk &= ~PI_SYSAD_ERRCHK_STATE;

	SD(LOCAL_HUB(PI_SYSAD_ERRCHK_EN), sysad_chk);

	/* 
	 * The sysstate parity checking cannot be enabled if either
	 * cpu is not present.
	 */
	if (en_sys_state && LD(LOCAL_HUB(PI_CPU_PRESENT_A)) 
	    && LD(LOCAL_HUB(PI_CPU_PRESENT_B)))
	    SD(LOCAL_HUB(PI_SYSAD_ERRCHK_EN), 
	       LD(LOCAL_HUB(PI_SYSAD_ERRCHK_EN)) | PI_SYSAD_ERRCHK_STATE);

	/*
	 * Now setup the NACK_CNT registers so that we dont get into an 
	 * infinite nack situation.
	 */

	SD(LOCAL_HUB(PI_NACK_CMP), PI_NACK_CNT_MAX);
#ifdef HUB2_NACK_WAR
	rev = LD(LOCAL_HUB(NI_STATUS_REV_ID));
	if (((rev & NSRI_REV_MASK) >> NSRI_REV_SHFT) == 2) {
		printf("HUB2: PI_NACK_CNT set to 0\n");
		SD(LOCAL_HUB(PI_NACK_CNT_A), 0);
		SD(LOCAL_HUB(PI_NACK_CNT_B), 0);
	}
	else
#endif
	{
	if (LD(LOCAL_HUB(PI_CPU_PRESENT_A)))
	    SD(LOCAL_HUB(PI_NACK_CNT_A), (1L << PI_NACK_CNT_EN_SHFT));
	if (LD(LOCAL_HUB(PI_CPU_PRESENT_B)))
	    SD(LOCAL_HUB(PI_NACK_CNT_B), (1L << PI_NACK_CNT_EN_SHFT));
	}

        if (en_eintr) {	
	    /*
	     * Setup error intr masks.
	     */

	    if (LD(LOCAL_HUB(PI_CPU_PRESENT_B)) == 0) /* only A present */
		SD(LOCAL_HUB(PI_ERR_INT_MASK_A), 
		   PI_FATAL_ERR_CPU_A | PI_MISC_ERR_CPU_A | PI_ERR_GENERIC);
	    else if (LD(LOCAL_HUB(PI_CPU_PRESENT_A)) == 0)
		/* only B present */
		SD(LOCAL_HUB(PI_ERR_INT_MASK_B), 
		   PI_FATAL_ERR_CPU_B | PI_MISC_ERR_CPU_B | PI_ERR_GENERIC);
	    else {	/* Both A and B present */
		SD(LOCAL_HUB(PI_ERR_INT_MASK_A),
		   PI_FATAL_ERR_CPU_B | PI_MISC_ERR_CPU_A | PI_ERR_GENERIC);
		SD(LOCAL_HUB(PI_ERR_INT_MASK_B), 
		   PI_FATAL_ERR_CPU_A | PI_MISC_ERR_CPU_B);
	    }
	    
	}
    }
#if 0
    if (en_eintr) {
	    enable_cpu_intrs();
    }
#endif
}


void 
error_clear_hub_erreg(nasid_t nasid)
{
    SD(REMOTE_HUB(nasid, PI_ERR_INT_PEND), -1);
    LD(REMOTE_HUB(nasid, PI_ERR_STATUS1_A_RCLR));
    LD(REMOTE_HUB(nasid, PI_ERR_STATUS1_B_RCLR));
    
    LD(REMOTE_HUB(nasid, MD_DIR_ERROR_CLR));
    LD(REMOTE_HUB(nasid, MD_MEM_ERROR_CLR));
    LD(REMOTE_HUB(nasid, MD_PROTOCOL_ERROR_CLR));
    LD(REMOTE_HUB(nasid, MD_MISC_ERROR_CLR));

    /*
     * XXX FIXME: Take care of local hub ii and bridge etc.
     */

    LD(REMOTE_HUB(nasid, NI_PORT_ERROR_CLEAR));
}


      
void
error_clear_exception()
{
    pcfg_hub_t *hub_cf;
    int i;

    printf("Clearing hub registers\n");

    error_clear_hub_erreg(get_nasid());

    printf("done Clearing hub registers\n");
    for (i = 0; i < PCFG(get_nasid())->count; i++) {
	hub_cf = &PCFG_PTR(get_nasid(), i)->hub;
	if (hub_cf->type != PCFG_TYPE_HUB) continue;
	printf("Clearing node %lx hub registers\n", hub_cf->nasid);	    
	if ((hub_cf->nasid != get_nasid()) &&
	    (hub_cf->nasid != INVALID_NASID))
	    error_clear_hub_erreg(hub_cf->nasid);
    }
    set_cop0(C0_SR, (get_cop0(C0_SR) & ~SR_EXL));
}



