/***********************************************************************\
 *       File:           rtr_diags.c                                    *
 *                                                                      *
 *       Contains code for testing links and routers in the sn0net 	*
 * 	 network during power-on. All tests are called by prom code.
 *                                                                      *
 \***********************************************************************/

#ident "$Revision: 1.39 $"
#define TRACE_FILE_NO		4

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <sys/SN/SN0/IP27.h>
#include <sys/SN/vector.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/promcfg.h>

#include <report.h>
#include <hub.h>
#include <libkl.h>

#include "ip27prom.h"
#include "net.h"
#include "rtc.h"
#include "libc.h"
#include "libasm.h"
#include "rtr_diags.h"


/*
 * vector_status
 *
 *   Function to match up vector read and write errors with KLDIAG return codes
 *   contained in kldiag.h.
 */

int vector_status(int status)
{
    short	diag_parm, diag_val;
    int		diag_return_code;

    switch(status) {

	case NET_ERROR_NONE:
	    diag_parm = DIAG_PARM_TBD;
	    diag_val = KLDIAG_PASSED;
	    break;

	case NET_ERROR_TIMEOUT:

	case NET_ERROR_ROUTERLOCK:
	    diag_parm = DIAG_PARM_TBD;
	    diag_val = KLDIAG_NI_RTR_TIMEOUT;
	    break;

	case NET_ERROR_OVERRUN:

	case NET_ERROR_REPLY:
	    diag_parm = DIAG_PARM_TBD;
	    diag_val = KLDIAG_NI_RTR_BAD_RPLY;
	    break;

	case NET_ERROR_ADDRESS:

	case NET_ERROR_COMMAND:
	    diag_parm = DIAG_PARM_TBD;
	    diag_val = KLDIAG_NI_RCVD_LNK_ERR;
	    break;

	case NET_ERROR_HARDWARE:
	    diag_parm = DIAG_PARM_TBD;
	    diag_val = KLDIAG_NI_RTR_HW_ERROR;
	    break;

	case NET_ERROR_PROT:
	    diag_parm = DIAG_PARM_TBD;
	    diag_val = KLDIAG_NI_RTR_PROT_ERR;
	    break;

	case NET_ERROR_VECTOR:
	    diag_parm = DIAG_PARM_TBD;
	    diag_val = KLDIAG_NI_RTR_INV_VEC;
	    break;

	default:
	    diag_parm = DIAG_PARM_TBD;
	    diag_val = KLDIAG_NI_RCVD_MISC_ERR;

	}

	diag_return_code = make_diag_rc(diag_parm, diag_val);
	return diag_return_code;
}


/*
 * hub_local_link_diag
 *
 *   Perform a vector read of the local router's STATUS/REV_ID register
 *   to verify that the connection between the hub and router works.
 *
 *   This test is written for the local hub/router, and hardwires the
 *   nasid value to 0.
 *
 *   It is set to run in normal, heavy and mfg diagnostic modes right now.
 */

int hub_local_link_diag(int diag_mode)
{
    int		i, diag_rc, status;
    int		verbose = 0;
    ulong	cb_errors, value;
    __uint64_t	read_result;


    if (diag_mode == DIAG_MODE_NONE) {
	return 0;
    }

    else {

	if (diag_mode == DIAG_MODE_MFG)
		verbose = 1;

	/*
	 * Check if llp link is operational on NI side
	 * and just exit if it is not (there may be nothing
	 * attached).
	 */

	if(!(net_link_up())) {

	    if (verbose) {
		if (net_link_down_reason())
		    printf("LLP Link never came out of reset! \n");
		else
		    printf("LLP Link failed after reset!\n");
	    }

	    return 0;
	}

	/* clear NI_PORT_ERROR register by reading clear address */
	value = LD(LOCAL_HUB(NI_PORT_ERROR_CLEAR));

	/* Read the status rev id register because it works for hub or rtr */
	for (i = 0; i < 32; i++) {

	    status = vector_read(0, 0, NI_STATUS_REV_ID, &read_result);

	    if (status) {
		diag_rc = vector_status(status);
		result_diagnosis("hub_local_link_diag", hub_slot_get(),
						     hub_cpu_get());
		result_fail("hub_local_link_diag", diag_rc, 
				get_diag_string(diag_rc));
		return diag_rc;
	    }

	} /* for i */

	/* Check if hub NI_PORT_ERROR register logged any errors */

	value = LD(LOCAL_HUB(NI_PORT_ERROR));

	if (value) {

	    cb_errors = ((value & NPE_CBERRCOUNT_MASK) >>
				RSERR_CBERRCNT_SHFT);

	    if (cb_errors > MED_CB_ERR_THRESHOLD) {

		diag_rc = make_diag_rc(DIAG_PARM_TBD,
				KLDIAG_NI_RCVD_LNK_ERR);
		result_diagnosis("hub_local_link_diag", hub_slot_get(),
						     hub_cpu_get());
		result_fail("hub_local_link_diag", diag_rc, 
				get_diag_string(diag_rc));
		return diag_rc;
	    }
	}

	diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_PASSED);
	result_pass("hub_local_link_diag test", diag_mode);
	return diag_rc;
    }
}

/*
 * hub_send_data_error_test
 *
 *   Program the local hub to send the local hub or router a data error,
 *   and then make sure that that chip detects it.
 *
 *   Note: Currently this test is only executable from POD mode
 *   for the local hub and router, and hardwires the nasid value to 0.
 *
 */

int hub_send_data_error_test(int diag_mode)
{
    int		in_port, status, diag_rc, cb_errors, sn_errors;
    int		verbose = 0;
    ulong	value, reg_value, send_data_error;
    __uint64_t	read_result;

    if (diag_mode == DIAG_MODE_NONE) {
	return 0;
    }

    else {

	if (diag_mode == DIAG_MODE_MFG)
		verbose = 1;

	/* check if network link is up */
	if (! (net_link_up())) {
	    printf("HUB LLP port is not up!\n");
	    return 0;
	}

	/* clear source hub ni_port_error register */
	reg_value = LD(LOCAL_HUB(NI_PORT_ERROR_CLEAR)); /* read clear */

	/* set send_llp_error bit in NI_DIAG_PARMS register */
	reg_value = LD(LOCAL_HUB(NI_DIAG_PARMS));
	send_data_error = reg_value ^ NDP_SENDERROR;
	SD(LOCAL_HUB(NI_DIAG_PARMS), send_data_error);

	/* check if router or hub is attached */
	status = vector_read(0, 0, NI_STATUS_REV_ID, &value);

	switch (GET_FIELD(value, NSRI_CHIPID)) {
	case CHIPID_HUB:

	    if (verbose)
		printf("Attached chip is hub.\n");

	    /* clear attached hub's ni_port_error register */
	    status = vector_read(0, 0, NI_PORT_ERROR_CLEAR, &read_result);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("hub_send_data_error", diag_rc,
						get_diag_string(diag_rc));
		return diag_rc;
	    }

	    /* do vector write to hub (use int_mask register) */
	    status = vector_write(0, 0, NI_IO_PROTECT, PATTERNF);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("hub_send_data_error", diag_rc,
						get_diag_string(diag_rc));
		return diag_rc;
	    }

	    /* check that hub detected error */
	    status = vector_read(0, 0, NI_PORT_ERROR_CLEAR, &read_result);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("hub_send_data_error", diag_rc,
						get_diag_string(diag_rc));
		return diag_rc;
	    }

	    cb_errors = ((read_result & NPE_CBERRCOUNT_MASK)
				>>  NPE_CBERRCOUNT_SHFT);
	    sn_errors = ((read_result & NPE_SNERRCOUNT_MASK)
				>>  NPE_SNERRCOUNT_SHFT);

	    if (verbose)
		printf("CB_ERR: %d	SN_ERR: %d\n", cb_errors, sn_errors);

	    if (cb_errors | sn_errors) {    /* success--hub detected error */

		diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_PASSED);
		result_pass("hub_send_data_error_test", diag_mode);

	    } else { /* hub failed to detect error when it should */

		diag_rc = make_diag_rc(DIAG_PARM_TBD,
					KLDIAG_NI_FAIL_ERR_DET);
		result_fail("hub_send_data_error", diag_rc,
					get_diag_string(diag_rc));
	    }

	    break;

	case CHIPID_ROUTER:

	    if (verbose)
		printf("Attached chip is router.\n");

	    /* check which port local hub is connected to */
	    status = vector_read(0, 0, RR_STATUS_REV_ID, &read_result);
	    if (status) {
		diag_rc = vector_status(status);
		return diag_rc;
	    }

	    in_port = (int) ((read_result & RSRI_INPORT_MASK) >>
				RSRI_INPORT_SHFT);

	    /* clear attached router port error registers */
	    status = vector_write(0, 0, RR_ERROR_CLEAR(in_port), 0);
	    if (status) {
		diag_rc = vector_status(status);
		return diag_rc;
	    }

	    /* do vector write to router (use diag_parms register) */

	    vector_read(0, 0, RR_DIAG_PARMS, &read_result);
	    status = vector_write(0, 0, RR_DIAG_PARMS, read_result);

	    if (status) {
		diag_rc = vector_status(status);
		result_fail("hub_send_data_error", diag_rc, 
				get_diag_string(diag_rc));
		return diag_rc;
	    }

	    /* check that the router detected the error */

	    status = vector_read(0, 0, RR_STATUS_ERROR(in_port), &read_result);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("hub_send_data_error", diag_rc, 
					get_diag_string(diag_rc));
		return diag_rc;
	    }

	    cb_errors = ((read_result & RSERR_CBERRCNT_MASK)
				>>  RSERR_CBERRCNT_SHFT);
	    sn_errors = ((read_result & RSERR_SNERRCNT_MASK)
				>>  RSERR_SNERRCNT_SHFT);

	    if (verbose)
		printf("CB_ERR: %d      SN_ERR: %d\n", cb_errors, sn_errors);

	    if (cb_errors | sn_errors) {    /* success--router detected error */

		diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_PASSED);
		result_pass("hub_send_data_error_test", diag_mode);

	    } else { /* router failed to detect error when it should */

		diag_rc = make_diag_rc(DIAG_PARM_TBD,
					KLDIAG_RTR_FAIL_ERR_DET);
		result_fail("hub_send_data_error", diag_rc,
					get_diag_string(diag_rc));
	    }
	    break;

	} /* switch status */

	return diag_rc;
    }
}



/*
 * test_hub_error_detection
 *
 *   Program the source chip to send the attached hub a data error, and
 *   then make sure that the attached hub detects it. Called by discover.
 *
 *   This test is currently set to only run in heavy and mfg modes.
 */

int test_hub_error_detection(int diag_mode,
			 pcfg_t *p,
			 int from_index,	/* Index on source chip */
			 int from_port,		/* Valid if from router */
			 int index,		/* Index on new chip */
			 net_vec_t from_path,	/* Path to source chip */
			 net_vec_t path)	/* Path to new chip */

{
    int		status, diag_rc;
    int		cb_errors, new_cb_errors, sn_errors, new_sn_errors;
    __uint64_t	value, send_data_error;

    /* Test will not execute for none or normal modes */

    if (diag_mode == DIAG_MODE_NONE | diag_mode == DIAG_MODE_NORMAL) {
	return 0;
    }

    else {

	/* get current value of hub_under_test's error regs*/

	status = vector_read(path, 0,  NI_PORT_ERROR, &value);

	if (status) {
	    diag_rc = vector_status(status);
	    result_fail("test_hub_error_detection", diag_rc,
					get_diag_string(diag_rc));
	    return diag_rc;
	} /* if status */

	cb_errors = ((value & NPE_CBERRCOUNT_MASK) >>
				NPE_CBERRCOUNT_SHFT);
	sn_errors = ((value & NPE_SNERRCOUNT_MASK) >>
				NPE_SNERRCOUNT_SHFT);

	/*
	 * Set up source chip to send a data error to hub_under_test.
	 */

	if (p->array[from_index].any.type == PCFG_TYPE_HUB) { /* hub */

	    /* check if source chip is the local hub */

	    if (from_path == 0) { /* it is local hub */

		/* set send_llp_error bit in NI_DIAG_PARMS register */

		value = LD(LOCAL_HUB(NI_DIAG_PARMS));
		send_data_error = value ^ NDP_SENDERROR;
		SD(LOCAL_HUB(NI_DIAG_PARMS), send_data_error);

	    } else { /* it is remote hub */

		/* set send_llp_error bit in NI_DIAG_PARMS register */

		status = vector_read(from_path, 0, NI_DIAG_PARMS, &value);
		if (status) {
		    diag_rc = vector_status(status);
		    result_fail("test_hub_error_detection", diag_rc,
				get_diag_string(diag_rc));
		    return diag_rc;
		} /* status */

		send_data_error = value ^ NDP_SENDERROR;

		status = vector_write(from_path, 0, NI_DIAG_PARMS,
							send_data_error);
		if (status) {
		    diag_rc = vector_status(status);
		    result_fail("test_hub_error_detection", diag_rc,
			get_diag_string(diag_rc));
		    return diag_rc;
		} /* status */

	    } /* source chip is hub */

	} else { /*  source chip is router */

	    /* set up to send data error from source router */

	    status = vector_read(from_path, 0, RR_DIAG_PARMS, &value);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("test_hub_error_detection", diag_rc,
				get_diag_string(diag_rc));	
		return diag_rc;
	    } /* status */

	    send_data_error = value ^ RDPARM_SENDERROR(from_port);

	    status = vector_write(from_path, 0, RR_DIAG_PARMS, send_data_error);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("test_hub_error_detection", diag_rc,
				get_diag_string(diag_rc));
		return diag_rc;
	    } /* status */

	} /* else source chip is router */

	/*
         * Perform vector op to hub_under_test (use NI_IO_PROTECT
         * register for write operation). This will force an
         * LLP error to the hub_under_test.
         */

	status = vector_write(path, 0, NI_IO_PROTECT, PATTERNF);
	if (status) {
	    diag_rc = vector_status(status);
	    result_fail("test_hub_error_detection", diag_rc,
			get_diag_string(diag_rc));
	    return diag_rc;
	}

	/*
 	 * Check that the hub_under_test detected the error
	 */

	status = vector_read(path, 0, NI_PORT_ERROR, &value);
	if (status) {
	    diag_rc = vector_status(status);
	    result_fail("test_hub_error_detection", diag_rc, 
			get_diag_string(diag_rc));
	    return diag_rc;
	}

	new_cb_errors = ((value & RSERR_CBERRCNT_MASK)
				>>  RSERR_CBERRCNT_SHFT);
	new_sn_errors = ((value & RSERR_SNERRCNT_MASK)
				>>  RSERR_SNERRCNT_SHFT);

	if ((new_cb_errors > cb_errors) | (new_sn_errors > sn_errors)) {

	    diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_PASSED);
	    result_pass("test_hub_error_detection", diag_mode);

	} else {

	    diag_rc = make_diag_rc(DIAG_PARM_TBD,
					KLDIAG_NI_FAIL_ERR_DET);
	    result_fail("test_hub_error_detection", diag_rc,
				get_diag_string(diag_rc));
	}

	return diag_rc;
    }
}

/*
 * rtr_send_data_error_test
 *
 *   Program the attached hub or router to send the local hub a data error,
 *   and then make sure that the hub detects it.
 *
 *   Note:  This test uses the local hub and router, and hardwires the
 *   nasid value to 0.
 *
 *   This test is currently only executable via pod mode.
 */

int rtr_send_data_error_test(int diag_mode)
{
    int		in_port, status, diag_rc;
    int		verbose;
    ulong	cb_errors, sn_errors, value;
    __uint64_t	send_data_error, read_result;


    if (diag_mode == DIAG_MODE_NONE) {
	return 0;
    }

    else {

	if (diag_mode == DIAG_MODE_MFG)
		verbose = 1;

	/* check if network link is up */
	if (!(net_link_up())) {
	    printf("HUB LLP port is not up!\n");
	    return 0;
	}

	/* clear source hub ni_port_error register */
	value = LD(LOCAL_HUB(NI_PORT_ERROR_CLEAR)); /* read clear */

	/* check if router or hub is attached */
	status = vector_read(0, 0, NI_STATUS_REV_ID, &value);

	switch (GET_FIELD(value, NSRI_CHIPID)) {
	case CHIPID_HUB:
	    if (verbose)
		printf("Attached chip is hub.\n");

	    /* set send_llp_error bit in NI_DIAG_PARMS register */

	    status = vector_read(0, 0, NI_DIAG_PARMS, &value);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("test_hub_error_detection", diag_rc,
				get_diag_string(diag_rc));
		return diag_rc;
	    } /* status */

	    send_data_error = value ^ NDP_SENDERROR;

	    status = vector_write(0, 0, NI_DIAG_PARMS,
							send_data_error);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("test_hub_error_detection", diag_rc,
				get_diag_string(diag_rc));
		return diag_rc;
	    } /* status */

	    /* do read of remote hub register to detect error */
	    status = vector_read(0, 0, NI_DIAG_PARMS, &value);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("test_hub_error_detection", diag_rc,
				get_diag_string(diag_rc));
		return diag_rc;
	    } /* status */

	    break;

	case CHIPID_ROUTER:

	    if (verbose)
		printf("Attached chip is router.\n");

	    /* check which router port hub is connected to */

	    status = vector_read(0, 0, RR_STATUS_REV_ID, &read_result);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("rtr_send_data_error", diag_rc, 
				get_diag_string(diag_rc));
		return diag_rc;
	    }
	    in_port = (int) ((read_result & RSRI_INPORT_MASK) >>
				RSRI_INPORT_SHFT);

	    /* set send_llp_error bit in RR_DIAG_PARMS register */

	    status = vector_read(0, 0, RR_DIAG_PARMS, &read_result);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("rtr_send_data_error", diag_rc,
				get_diag_string(diag_rc)); 
		return diag_rc;
	    }

	    send_data_error = read_result ^ RDPARM_SENDERROR(in_port);

	    status = vector_write(0, 0, RR_DIAG_PARMS, send_data_error);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("rtr_send_data_error", diag_rc, 
				get_diag_string(diag_rc));
		return diag_rc;
	    }

	    /* do read of router register (use diag_parms register) */
	    status = vector_read(0, 0, RR_DIAG_PARMS, &read_result);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("rtr_send_data_error", diag_rc, 
					get_diag_string(diag_rc));
		return diag_rc;
	    }

	    break;
	} /* switch */

	/* check that the local hub detected the error */

	value = LD(LOCAL_HUB(NI_PORT_ERROR)); /* read clear */

	cb_errors = ((value & NPE_CBERRCOUNT_MASK)
				>>  NPE_CBERRCOUNT_SHFT);
	sn_errors = ((value & NPE_SNERRCOUNT_MASK)
				>>  NPE_SNERRCOUNT_SHFT);
	if (verbose)
	    printf("CB_ERR: %d      SN_ERR: %d\n", cb_errors, sn_errors);

	if (cb_errors | sn_errors) {	/* success--hub detected error */

	    diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_PASSED);
	    result_pass("rtr_send_data_error_test", diag_mode);
	}
	else {		/* hub failed to detect error when it should */

	    diag_rc = make_diag_rc(DIAG_PARM_TBD,
				   KLDIAG_NI_FAIL_ERR_DET);
	    result_fail("rtr_send_data_error", diag_rc,
				get_diag_string(diag_rc));
	}

	return diag_rc;

    } /* else */

} /* rtr_send_data_error test */

/*
 * rtr_error_detection_diag
 *
 *   Program the source chip to send the attached rtr a data error, and
 *   then make sure that the attached rtr detects it. Called by discover.
 *
 *   This test is currently set to only run in heavy and mfg modes.
 */

int rtr_error_detection_diag(int diag_mode,
			 pcfg_t *p,
			 int from_index,	/* Index on source chip */
			 int from_port,		/* Valid if from router */
			 int index,		/* Index on new chip */
			 net_vec_t from_path,	/* Path to source chip */
			 net_vec_t path)	/* Path to new chip */

{
    int			status,
			diag_rc = 0;
    int			in_port,
			new_cb_errors,
			new_sn_errors,
			cb_errors,
			sn_errors;
    __uint64_t		value,
			send_data_error,
			read_result;

    /* Test will not execute for none or normal modes */

    if (diag_mode == DIAG_MODE_NONE | diag_mode == DIAG_MODE_NORMAL) {
	return 0;
    }

    else {

	/* get current value of rtr_under_test's error regs*/
	status = vector_read(path, 0, RR_STATUS_REV_ID, &value);

	if (status) {
	    diag_rc = vector_status(status);
	    result_fail("rtr_error_detection_diag", diag_rc,
				get_diag_string(diag_rc));
	    return diag_rc;
	} /* if status */

	in_port = (int) ((value & RSRI_INPORT_MASK) >>
				   RSRI_INPORT_SHFT);

	/* get current error count on remote router-under-test */
	status = vector_read(path, 0, RR_STATUS_ERROR(in_port), &value);
	if (status) {
	    diag_rc = vector_status(status);
	    result_fail("rtr_error_detection_diag", diag_rc,
			get_diag_string(diag_rc));
	    return diag_rc;
	} /* if status */

	if (value) {
	    cb_errors = ((value & RSERR_CBERRCNT_MASK) >>
				      RSERR_CBERRCNT_SHFT);
	    sn_errors = ((value & RSERR_SNERRCNT_MASK) >>
				      RSERR_SNERRCNT_SHFT);
	} /* if value */

	/*
	 * Set up source chip to send a data error to rtr_under_test.
	 */

	if (p->array[from_index].any.type == PCFG_TYPE_HUB) { /* hub */

	    /* check if source chip is the local hub */

	    if (from_path == 0) { /* it is local hub */

		/* set send_llp_error bit in NI_DIAG_PARMS register */

		value = LD(LOCAL_HUB(NI_DIAG_PARMS));
		send_data_error = value ^ NDP_SENDERROR;
		SD(LOCAL_HUB(NI_DIAG_PARMS), send_data_error);

	    } else { /* it is remote hub */

		/* set send_llp_error bit in NI_DIAG_PARMS register */

		status = vector_read(from_path, 0, NI_DIAG_PARMS, &value);
		if (status) {
		    diag_rc = vector_status(status);
		    result_fail("rtr_error_detection_diag", diag_rc,
				get_diag_string(diag_rc));
		    return diag_rc;
		} /* status */

		send_data_error = value ^ NDP_SENDERROR;

		status = vector_write(from_path, 0, NI_DIAG_PARMS,
							send_data_error);
		if (status) {
		    diag_rc = vector_status(status);
		    result_fail("rtr_error_detection_diag", diag_rc,
				get_diag_string(diag_rc));
		    return diag_rc;
		} /* status */

	    } /* source chip is hub */

	} else { /*  source chip is router */

	    status = vector_read(from_path, 0, RR_STATUS_REV_ID, &value);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("rtr_error_detection_diag", diag_rc,
					get_diag_string(diag_rc));
		return diag_rc;
	    }


	    /* set up to send data error from source router */

	    status = vector_read(from_path, 0, RR_DIAG_PARMS, &value);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("rtr_error_detection_diag", diag_rc,
				get_diag_string(diag_rc));
		return diag_rc;
	    } /* status */

	    send_data_error = value ^ RDPARM_SENDERROR(from_port);

	    status = vector_write(from_path, 0, RR_DIAG_PARMS, send_data_error);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("rtr_error_detection_diag", diag_rc,
				get_diag_string(diag_rc));
		return diag_rc;
	    } /* status */

	} /* else source chip is router */

	/*
         * Perform vector op to rtr_under_test.
	 * Also restores SENDERROR back to normal.
         */

	status = vector_write(path, 0, RR_DIAG_PARMS, value);

	if (status) {
	    diag_rc = vector_status(status);
	    result_fail("rtr_error_detection_diag", diag_rc,
				get_diag_string(diag_rc));
	    return diag_rc;
	}

	/*
 	 * Check that the rtr_under_test detected the error
	 */

	/* get current error count on remote router-under-test */
	status = vector_read(path, 0, RR_STATUS_ERROR(in_port), &value);
	if (status) {
	    diag_rc = vector_status(status);
	    result_fail("rtr_error_detection_diag", diag_rc,
				get_diag_string(diag_rc));
	    return diag_rc;
	}

	new_cb_errors = ((value & RSERR_CBERRCNT_MASK)
				>>  RSERR_CBERRCNT_SHFT);
	new_sn_errors = ((value & RSERR_SNERRCNT_MASK)
				>>  RSERR_SNERRCNT_SHFT);

	if ((new_cb_errors > cb_errors) | (new_sn_errors > sn_errors)) {

	    diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_PASSED);
	    result_pass("rtr_error_detection_diag", diag_mode);

	} else {

	    diag_rc = make_diag_rc(DIAG_PARM_TBD,
					KLDIAG_RTR_FAIL_ERR_DET);

	    result_fail("rtr_error_detection_diag", diag_rc,
			get_diag_string(diag_rc));
	}

	return diag_rc;
    }
}

/*
 * hub_link_vector_diag
 *
 *   This test is called by the discover_hub function during
 *   discovery and is used to verify basic link functionality by doing
 *   a series of vector operations.
 *
 *   It is set to run in normal, heavy, and mfg diagnostic modes right now.
 */

int hub_link_vector_diag(int diag_mode,
			 pcfg_t *p,
			 int from_index,	/* Index on source chip */
			 int from_port,		/* Valid if from router */
			 int index,		/* Index on new chip */
			 net_vec_t from_path,	/* Path to source chip */
			 net_vec_t path)	/* Path to new chip */
{
    int			i,
			in_port,
			status;
    int			cb_errs = 0,
			rem_cb_errs = 0,
			loc_cb_errs = 0,
			diag_rc = 0;
    __uint64_t		value = 0;
    pcfg_hub_t		*ph;
    pcfg_router_t	*pr;

    if (diag_mode == DIAG_MODE_NONE)
	return 0;

    else {

	/* get current value of hub_under_test's error registers */

	status = vector_read(path, 0,  NI_PORT_ERROR, &value);

	if (status) {
	    diag_rc = vector_status(status);
	    result_diagnosis("hub_link_vector_diag", hub_slot_get(),
						     hub_cpu_get());
	    result_fail("hub_link_vector_diag", diag_rc, 
			get_diag_string(diag_rc));
	    return diag_rc;
	} /* if status */

	rem_cb_errs = (int) ((value & NPE_CBERRCOUNT_MASK) >>
				NPE_CBERRCOUNT_SHFT);

	/* check if source chip is hub or router and get its error count */

	if (p->array[from_index].any.type == PCFG_TYPE_HUB) { /* hub */

	    /* check if it is the local hub */

	    if (from_path == 0) { /* if local hub do load */

		value = LD(LOCAL_HUB(NI_PORT_ERROR));
	    } /* local hub */

	    else { /* remote hub so do vector_read */

		status = vector_read(from_path, 0, NI_PORT_ERROR, &value);

		if (status) {
		    diag_rc = vector_status(status);
		    result_diagnosis("hub_link_vector_diag", hub_slot_get(),
						     hub_cpu_get());
		    result_fail("hub_link_vector_diag", diag_rc, 
					get_diag_string(diag_rc));
		    return diag_rc;
		} /* if status */

	    } /* else remote hub */

	    loc_cb_errs = (int)((value & NPE_CBERRCOUNT_MASK) >>
				    NPE_CBERRCOUNT_SHFT);
	} /* if hub */

	else { /* current chip is router */
	    status = vector_read(from_path, 0, RR_STATUS_ERROR(from_port),
						&value);
	    if (status) {
		diag_rc = vector_status(status);
		result_diagnosis("hub_link_vector_diag", hub_slot_get(),
						     hub_cpu_get());
		result_fail("hub_link_vector_diag", diag_rc,
				get_diag_string(diag_rc));
		return diag_rc;
	    }

	    loc_cb_errs = (int)((value & RSERR_CBERRCNT_MASK) >>
				    RSERR_CBERRCNT_SHFT);
	} /* else router */

	/* Perform vector ops to make traffic across link */

	for (i = 0; i < 32; i++) {

	    /* read scratch registers because this has no side effects */
	    status = vector_read(path, 0, NI_SCRATCH_REG0, &value);
	    if (status) {
		diag_rc = vector_status(status);
		result_diagnosis("hub_link_vector_diag", hub_slot_get(),
						     hub_cpu_get());

		result_fail("hub_link_vector_diag", diag_rc,
				get_diag_string(diag_rc));
		return diag_rc;
	    } /* if status */

	    status = vector_read(path, 0, NI_SCRATCH_REG1, &value);
	    if (status) {
		diag_rc = vector_status(status);
		result_diagnosis("hub_link_vector_diag", hub_slot_get(),
						     hub_cpu_get());
		result_fail("hub_link_vector_diag", diag_rc,
				get_diag_string(diag_rc));
		return diag_rc;
	    } /* if status */

	    /* write default value to NI_IO_PROTECT reg */
	    status = vector_write(path, 0, NI_IO_PROTECT, PATTERNF);
	    if (status) {
		diag_rc = vector_status(status);
		result_diagnosis("hub_link_vector_diag", hub_slot_get(),
						     hub_cpu_get());
		result_fail("hub_link_vector_diag", diag_rc,
				get_diag_string(diag_rc));
		return diag_rc;
	    } /* if status */
	} /* for */

	/* Get new number of errors on remote side of link */
	status = vector_read(path, 0, NI_PORT_ERROR, &value);
	if (status) {
	    diag_rc = vector_status(status);
	    result_diagnosis("hub_link_vector_diag", hub_slot_get(),
						     hub_cpu_get());
	    result_fail("hub_link_vector_diag", diag_rc,
			get_diag_string(diag_rc));
	    return diag_rc;
	}

	/* Test for increase in error count */

	if (value) {
	    cb_errs = (int) ((value & NPE_CBERRCOUNT_MASK) >>
				NPE_CBERRCOUNT_SHFT);

	    /* compare against initial number of errors and threshold value */
	    if (cb_errs > rem_cb_errs) {
		if ((cb_errs - rem_cb_errs) > MED_CB_ERR_THRESHOLD) {

		    printf("hub_link_vector_diag: Detected %d CB errors on remote side of link\n", (cb_errs - rem_cb_errs));
		    printf("hub_link_vector_diag: Starting cb_err value was %d\n", 
					rem_cb_errs); 

		    if (p->array[from_index].any.type == PCFG_TYPE_HUB) {

			/* update local promcfg entry with bad link info */
			ph = &p->array[from_index].hub;
			ph->flags |= PCFG_HUB_PORTSTAT1;

			/* return error code */
			diag_rc = make_diag_rc(DIAG_PARM_TBD,
				KLDIAG_NI_RCVD_LNK_ERR);
			result_diagnosis("hub_link_vector_diag",
				hub_slot_get(), hub_cpu_get());
			result_fail("hub_link_vector_diag", diag_rc,
					get_diag_string(diag_rc));
		    } /* if hub */

		    else { /* router */

			/* update local rtr promcfg entry with bad link info */
			pr = &p->array[from_index].router;
			pr->portstat1 |= 1 << from_port;

			/* return error code */
			diag_rc = make_diag_rc(DIAG_PARM_TBD,
					KLDIAG_RTR_RCVD_LNK_ERR);
			result_diagnosis("hub_link_vector_diag",
					hub_slot_get(), hub_cpu_get());
			result_fail("hub_link_vector_diag", diag_rc,
					get_diag_string(diag_rc));
		    } /* router */

		    /* update remote hub's promcfg entry with bad link info */
		    ph = &p->array[index].hub;
		    ph->flags |= PCFG_HUB_PORTSTAT1;

		    return diag_rc;

		} /* if cb_errs - rem_cb_errs > MED_CB_ERR_THRESHOLD */

	    } /* if cb_errs  */

	} /* if value */

	/* Check for errors on this side of link */

	if (p->array[from_index].any.type == PCFG_TYPE_HUB) {

	    /* check if it is the local hub */

	    if (from_path == 0) { /* if local hub do load */

		value = LD(LOCAL_HUB(NI_PORT_ERROR));

	    } /* local hub */

	    else { /* remote hub so do vector_read */

		status = vector_read(from_path, 0, NI_PORT_ERROR, &value);

		if (status) {
		    diag_rc = vector_status(status);
		    result_diagnosis("hub_link_vector_diag", hub_slot_get(),
						     hub_cpu_get());
		    result_fail("hub_link_vector_diag", diag_rc,
					get_diag_string(diag_rc));
		    return diag_rc;
		} /* if status */

	    } /* else remote hub */

	    if (value) {
		cb_errs = (int) ((value & NPE_CBERRCOUNT_MASK) >>
				    NPE_CBERRCOUNT_SHFT);
	    } /* if value */


	} /* if hub */

	else { /* current chip is router */

	    status = vector_read(from_path, 0, RR_STATUS_ERROR(from_port),
					&value);
	    if (status) {
		diag_rc = vector_status(status);
		result_diagnosis("hub_link_vector_diag", hub_slot_get(),
						     hub_cpu_get());
		result_fail("hub_link_vector_diag", diag_rc,
				get_diag_string(diag_rc)); 
		return diag_rc;
	    } /* if status */

	    if (value) {
		cb_errs = (int) ((value & RSERR_CBERRCNT_MASK) >>
				    RSERR_CBERRCNT_SHFT);
	    } /* if value */

	} /* else router */

	/*
	 * Since errors were found, compare count against initial
	 * number of errors and threshold value. Mark remote and
	 * local promcfg entries with bad link info.
	 */

	if (cb_errs > loc_cb_errs)  {

	    if ((cb_errs - loc_cb_errs) > MED_CB_ERR_THRESHOLD) {

		printf("hub_link_vector_diag: Detected %d CB errors on local side of link\n", (cb_errs - loc_cb_errs));
		printf("hub_link_vector_diag: Starting cb_err value was %d\n", 
					loc_cb_errs); 

		/* update remote hub's promcfg entry */
		ph = &p->array[index].hub;
		ph->flags |= PCFG_HUB_PORTSTAT1;

		/* update local hub or router promcfg entry */
		if (p->array[from_index].any.type == PCFG_TYPE_HUB) {

		    /* update local hub promcfg entry */
		    ph = &p->array[from_index].hub;
		    ph->flags |= PCFG_HUB_PORTSTAT1;

		    /* set up error code and failure info */
		    diag_rc = make_diag_rc(DIAG_PARM_TBD,
				KLDIAG_NI_RCVD_LNK_ERR);
		    result_diagnosis("hub_link_vector_diag", hub_slot_get(),
						     hub_cpu_get());
		    result_fail("hub_link_vector_diag", diag_rc,
					get_diag_string(diag_rc));
		} /* if hub */

		else { /* router */

		    /* update local rtr promcfg entry */
		    pr = &p->array[from_index].router;
		    pr->portstat1 |= 1 << from_port;

		    /* set up error code and failure info */
		    diag_rc = make_diag_rc(DIAG_PARM_TBD,
				KLDIAG_RTR_RCVD_LNK_ERR);
		    result_diagnosis("hub_link_vector_diag", hub_slot_get(),
						     hub_cpu_get());
		    result_fail("hub_link_vector_diag", diag_rc,
			get_diag_string(diag_rc));

		} /* else router */

		return diag_rc;

	    } /* if cb_errs - loc_cb_errs > MED_CB_ERR_THRESHOLD */

	} /* if cb_errs > loc_cb_errs */

	result_pass("hub_link_vector_diag", diag_mode);
	return diag_rc;

   } /* else if heavy or mfg modes */

} /* hub_link_vector_diag */


/*
 * rtr_link_vector_diag
 *
 *   This test is called by the discover_router function during
 *   discovery and is used to verify basic link functionality by doing
 *   a series of vector operations.
 *
 *   It is set to run in normal, heavy, and mfg diagnostic modes right now.
 */

int rtr_link_vector_diag(int diag_mode,
			 pcfg_t *p,
			 int from_index,	/* Index on source chip */
			 int from_port,		/* Valid if from router */
			 int index,		/* Index on new chip */
			 net_vec_t from_path,	/* Path to source chip */
			 net_vec_t path)	/* Path to new chip */
{
    int			i,
			in_port,
			status;
    int			diag_rc = 0,
    			loc_cb_errs = 0,
			loc_sn_errs = 0,
			rem_cb_errs = 0,
			rem_sn_errs = 0,
			cb_errs = 0,
			sn_errs = 0;
    __uint64_t          value = 0;
    pcfg_hub_t		*ph;
    pcfg_router_t	*pr, *pfr;


    if (diag_mode == DIAG_MODE_NONE)
	return 0;

    else {

	/* figure out port number on remote router */

	status = vector_read(path, 0, RR_STATUS_REV_ID, &value);

	if (status) {
	    diag_rc = vector_status(status);
	    result_fail("rtr_link_vector_diag", diag_rc, 
			get_diag_string(diag_rc));
	    return diag_rc;
	} /* if status */

	in_port = (int) ((value & RSRI_INPORT_MASK) >>
				   RSRI_INPORT_SHFT);

	/* get current error count on remote router-under-test */
	status = vector_read(path, 0, RR_STATUS_ERROR(in_port), &value);
	if (status) {
	    diag_rc = vector_status(status);
	    result_fail("rtr_link_vector_diag", diag_rc, 
				get_diag_string(diag_rc));
	    return diag_rc;
	} /* if status */

	if (value) {
	    rem_cb_errs = (int) ((value & RSERR_CBERRCNT_MASK) >>
				      RSERR_CBERRCNT_SHFT);
	    rem_sn_errs = (int) ((value & RSERR_SNERRCNT_MASK) >>
				      RSERR_SNERRCNT_SHFT);
	} /* if value */


	/* check if source chip is hub or router and get its error count */

	if (p->array[from_index].any.type == PCFG_TYPE_HUB) { /* hub */

	    /* check if it is the local hub */

	    if (from_path == 0) {

		/* if local hub do load */
		value = LD(LOCAL_HUB(NI_PORT_ERROR));
	    }

	    else { /* remote hub so do vector_read */

		status = vector_read(from_path, 0, NI_PORT_ERROR, &value);

		if (status) {
		    diag_rc = vector_status(status);
		    result_fail("rtr_link_vector_diag", diag_rc,
					get_diag_string(diag_rc));
		    return diag_rc;
		} /* if status */
	    }

	    loc_cb_errs = (int) ((value & NPE_CBERRCOUNT_MASK) >>
				    NPE_CBERRCOUNT_SHFT);
#if 0
	    loc_sn_errs = (int) ((value & NPE_SNERRCOUNT_MASK) >>
				    NPE_SNERRCOUNT_SHFT);
#endif

	} /* if hub */

	else { /* current chip is router */


	    status = vector_read(from_path, 0, RR_STATUS_ERROR(from_port),
					&value);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("rtr_link_vector_diag", diag_rc,
				get_diag_string(diag_rc));
		return diag_rc;
	    }

	    loc_cb_errs = (int) ((value & RSERR_CBERRCNT_MASK) >>
				    RSERR_CBERRCNT_SHFT);
#if 0
	    loc_sn_errs = (int) ((value & RSERR_SNERRCNT_MASK) >>
				    RSERR_SNERRCNT_SHFT);
#endif

	} /* else router */

	/* Perform vector ops just to make traffic across link */

	for (i = 0; i < 64; i++){ /* for loop for test pattern 1 */
	    /* read scratch register because it has no side effects */
	    status = vector_read(path, 0, RR_SCRATCH_REG0, &value);
	    if (status) {
		diag_rc = vector_status(status);
		result_fail("rtr_link_vector_diag", diag_rc, 
			get_diag_string(diag_rc));
		return diag_rc;
	    }

	} /* for */

	/* Check if errors were seen on remote side of link */

	status = vector_read(path, 0, RR_STATUS_ERROR(in_port), &value);
	if (status) {
	    diag_rc = vector_status(status);
	    result_fail("rtr_link_vector_diag", diag_rc,
				get_diag_string(diag_rc)); 
	    return diag_rc;
	}

	if (value) {
	    cb_errs = (int) ((value & RSERR_CBERRCNT_MASK) >>
				RSERR_CBERRCNT_SHFT);
#if 0
	    sn_errs = (int) ((value & RSERR_SNERRCNT_MASK) >>
				RSERR_SNERRCNT_SHFT);
#endif

	    /*
	     * Since errors were found, compare count against initial
	     * number of errors and threshold value. Mark remote and
	     * local promcfg entries with bad link info.
	     */

		if (cb_errs > rem_cb_errs) {

		    if ((cb_errs - rem_cb_errs) > MED_CB_ERR_THRESHOLD) {

			/* Update remote rtr's promcfg entry */

			pr = &p->array[index].router;
			pr->portstat1 |= 1 << in_port;

			/* Update local hub or router promcfg entry */

			if (p->array[from_index].any.type == PCFG_TYPE_HUB) {

			    /* update local promcfg entry with bad link info */
			    ph = &p->array[from_index].hub;
			    ph->flags |= PCFG_HUB_PORTSTAT1;
			    diag_rc = make_diag_rc(DIAG_PARM_TBD,
					    KLDIAG_RTR_RCVD_LNK_ERR);
			    result_fail("rtr_link_vector_diag", diag_rc,
					get_diag_string(diag_rc));
			    return diag_rc;

			} /* if hub */

			else { /* router */

			    /* update local rtr entry with bad link info */
			    pfr = &p->array[from_index].router;
			    pfr->portstat1 |= 1 << from_port;
			    diag_rc = make_diag_rc(DIAG_PARM_TBD,
						KLDIAG_RTR_RCVD_LNK_ERR);
			    result_fail("rtr_link_vector_diag", diag_rc,
					get_diag_string(diag_rc));
			    return diag_rc;

			} /* router */

		    } /* if cb_errs - rem_cb_errs > MED_CB_ERR_THRESHOLD */

		} /* if cb_errs > rem_cb_errs */

	    } /* if value */

	    /* Check for errors on this side of the link */

	    if (p->array[from_index].any.type == PCFG_TYPE_HUB) {

		/* check if it is the local hub */

		if (from_path == 0) {

		    /* if local hub do load */
		    value = LD(LOCAL_HUB(NI_PORT_ERROR));
		}

		else { /* remote hub so do vector_read */

		    status = vector_read(from_path, 0, NI_PORT_ERROR, &value);

		    if (status) {
			diag_rc = vector_status(status);
			result_fail("rtr_link_vector_diag", diag_rc,
				get_diag_string(diag_rc));
			return diag_rc;
		    } /* if status */

		} /* else remote hub */

		if (value) {
		    cb_errs = (int) ((value & NPE_CBERRCOUNT_MASK) >>
					NPE_CBERRCOUNT_SHFT);
#if 0
		    sn_errs = (int) ((value & NPE_SNERRCOUNT_MASK) >>
					NPE_SNERRCOUNT_SHFT);
#endif

		} else {
		    cb_errs = 0;
#if 0
		    sn_errs = 0;
#endif

		} /* if value */

	    } /* if hub */

	    else { /* current chip is router */

		status = vector_read(from_path, 0, RR_STATUS_ERROR(from_port),
					&value);
		if (status) {
		    diag_rc = vector_status(status);
		    result_fail("rtr_link_vector_diag", diag_rc,
				get_diag_string(diag_rc));
		    return diag_rc;
		} /* if status */

		if (value) {
		    cb_errs = (int) ((value & RSERR_CBERRCNT_MASK) >>
				    RSERR_CBERRCNT_SHFT);
#if 0
		    sn_errs = (int) ((value & RSERR_SNERRCNT_MASK) >>
				    RSERR_SNERRCNT_SHFT);
#endif

		} else {
		    cb_errs = 0;
#if 0
		    sn_errs = 0;
#endif
		}/* if value */

	    } /* else router */

	/*
         * Since errors were found, compare count against initial
         * number of errors and threshold value. Mark remote and
         * local promcfg entries with bad link info.
         */

	if (cb_errs > loc_cb_errs) {

	    if ((cb_errs - loc_cb_errs) > MED_CB_ERR_THRESHOLD) {

		/* update remote rtr's promcfg entry */

		pr = &p->array[index].router;
		pr->portstat1 |= 1 << in_port;

		/* update local hub or router promcfg entry */

		if (p->array[from_index].any.type == PCFG_TYPE_HUB) {

		    ph = &p->array[from_index].hub;
		    ph->flags |= PCFG_HUB_PORTSTAT1;
		    diag_rc = make_diag_rc(DIAG_PARM_TBD,
					    KLDIAG_NI_RCVD_LNK_ERR);
		    result_fail("rtr_link_vector_diag", diag_rc,
					get_diag_string(diag_rc));
		    return diag_rc;

		} /* if hub */

		else { /* router */

		    pfr = &p->array[from_index].router;
		    pfr->portstat1 |= 1 << from_port;

		    diag_rc = make_diag_rc(DIAG_PARM_TBD,
				KLDIAG_RTR_RCVD_LNK_ERR);
		    result_fail("rtr_link_vector_diag", diag_rc,
				get_diag_string(diag_rc));
		    return diag_rc;

		} /* router */

	    } /* if cb_errs - loc_cb_errs > MED_CB_ERR_THRESHOLD */

	} /* if cb_errs > loc_cb_errs */

	diag_rc = KLDIAG_PASSED;
	result_pass("rtr_link_vector_diag", diag_mode);
	return diag_rc;

    } /* else do test */

} /* rtr_link_vector_diag */
