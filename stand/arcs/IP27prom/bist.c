/***********************************************************************\
*	File:		bist.c						*
*									*
*	Library routines for setting up and executing the SN0 hub and	*
*	router built-in self-tests.					*
*									*
*	Data for TDR write should be a bit vector padded (on the	*
*	most significant side) to a multiple of 64 bits in length.	*
*	Data for TDR read is padded on the other side.			*
*									*
*	Warning:							*
*									*
*	The hub chip resets itself and the T5 upon completion of	*
*	a BIST.  The status bit in the BIST data can be used to		*
*	determine if the bist passed or failed.				*
*									*
\***********************************************************************/

#ident "$Revision: 1.30 $"

#define PDEBUG		0

#include <sys/types.h>
#include <sys/SN/addrs.h>
#include <sys/SN/kldiag.h>
#include <libkl.h>
#include <report.h>
#include <hub.h>
#include <ksys/elsc.h>

#include "ip27prom.h"
#if PDEBUG
#include "libc.h"
#endif
#include "bist.h"
#include "rtr_diags.h"
#include "bist_data.c"

/*
 * BIST routines require a callback routine that performs the actual reads
 * and writes of various BIST registers.  This supports both hub and router
 * versions of BIST.
 *
 * The callback should read or write (as appropriate) the requested function
 * register and return BIST_OK or one of the error codes.  The parameter
 * "any" is passed straight through to the callback for convenience's sake.
 */

#define BIST_CB_WRITE_DATA	0
#define BIST_CB_READ_DATA	1
#define BIST_CB_COUNT_TARGET	2
#define BIST_CB_READY		3
#define BIST_CB_SHIFT_LOAD	4
#define BIST_CB_SHIFT_UNLOAD	5
#define BIST_CB_ENTER_RUN	6

typedef int (*bist_callback_t)(int request,		/* BIST_CB_xxx     */
			       __uint64_t *data,	/* Read/write data */
			       __psunsigned_t userdata);

#define BIST_TARGET_UNDEF	0
#define BIST_TARGET_LBIST	1
#define BIST_TARGET_ABIST	2
#define BIST_TARGET_CLOCK	3

/*
 * Miscellaneous utilities
 */

#define CTTARG(count, target)		((target) << 6 | (count) & 63)
#define CTTARG_COUNT(cttarg)		((cttarg) & 63 ? (cttarg) : 64)
#define CTTARG_TARGET(cttarg)		((cttarg) >> 6 & 3)

static __uint64_t
reverse_dw(__uint64_t t)
{
    t = t << 32			     | t >> 32			   ;
    t = t << 16 & 0xffff0000ffff0000 | t >> 16 & 0x0000ffff0000ffff;
    t = t <<  8 & 0xff00ff00ff00ff00 | t >>  8 & 0x00ff00ff00ff00ff;
    t = t <<  4 & 0xf0f0f0f0f0f0f0f0 | t >>  4 & 0x0f0f0f0f0f0f0f0f;
    t = t <<  2 & 0xcccccccccccccccc | t >>  2 & 0x3333333333333333;
    t = t <<  1 & 0xaaaaaaaaaaaaaaaa | t >>  1 & 0x5555555555555555;

    return t;
}

static __uint64_t
get_field(__uint64_t *dwords, int bitno, int count)
{
    __uint64_t		value;

    /* count is the number of bits to fetch */
    for (value = 0; count--; bitno++)
	value = value << 1 | dwords[bitno / 64] >> 63 - (bitno & 63) & 1;

    return value;
}

static int
wait_ready(bist_callback_t callback, __psunsigned_t userdata)
{
    __uint64_t	value;
    int		r;

    do
	if ((r = (*callback)(BIST_CB_READY, &value, userdata)) < 0)
	    return r;
    while ((value & 1) == 0);

    return 0;
}

/*
 * bist_write
 *
 *   Writes a bit vector of data of arbitrary length into a specified
 *   target BIST TDR register.  Includes a bit-reversal to IBM format.
 */

static int
bist_write(bist_callback_t callback, __psunsigned_t userdata,
	   int target, __uint64_t *data, int bits)
{
    __uint64_t		value, cttarg, zero = 0;
    int			dw, count, i, r;

    dw	= (bits + 63) / 64;

    for (i = 0; i < dw; i++) {

	count	= (i < dw - 1) ? 64 : (bits & 63);
	cttarg	= CTTARG(count, target);

        value   = reverse_dw(data[dw - 1 - i]);

	if ((r = wait_ready(callback, userdata)) < 0) 
	    goto done;

	if ((r = (*callback)(BIST_CB_COUNT_TARGET, &cttarg, userdata)) < 0) 
	    goto done;
	
	if ((r = wait_ready(callback, userdata)) < 0) 
	    goto done;

	if ((r = (*callback)(BIST_CB_WRITE_DATA, &value, userdata)) < 0) 
	    goto done;
	

	if ((r = (*callback)(BIST_CB_SHIFT_LOAD, &zero, userdata)) < 0) 
	    goto done;

	if ((r = wait_ready(callback, userdata)) < 0)
	    goto done;
    }

    r	= 0;

 done:
    return r;
}

/*
 * bist_read
 *
 *   Reads a bit vector of data or arbitrary length from a specified
 *   target BIST TDR register.  Includes a bit-reversal from IBM format.
 */

static int
bist_read(bist_callback_t callback, __psunsigned_t userdata,
	  int target, __uint64_t *data, int bits)
{
    __uint64_t		value, cttarg, zero = 0;
    int			dw, count, i, r;

    dw	= (bits + 63) / 64;

    for (i = 0; i < dw; i++) {

	count	= (i < dw - 1) ? 64 : (bits & 63);

	cttarg	= CTTARG(count, target);

	if ((r = wait_ready(callback, userdata)) < 0)
	    goto done;

	if ((r = (*callback)(BIST_CB_COUNT_TARGET, &cttarg, userdata)) < 0)
	    goto done;

	if ((r = wait_ready(callback, userdata)) < 0)
	    goto done;

	if ((r = (*callback)(BIST_CB_SHIFT_UNLOAD, &zero, userdata)) < 0)
	    goto done;

	if ((r = wait_ready(callback, userdata)) < 0)
	    goto done;

	if ((r = (*callback)(BIST_CB_READ_DATA, &value, userdata)) < 0)
	    goto done;

	data[dw - 1 - i] = reverse_dw(value << (64 - count));

    }

    r	= 0;

 done:
    return r;
}

/*
 * bist_execute
 *
 *   Starts the BIST using the requested TDR.  
 *
 *   Note:  When the LBIST or ABIST TDR is executed, the CLOCK TDR is
 *   implicitly executed first.
 */

static int
bist_execute(bist_callback_t callback, __psunsigned_t userdata, int target)
{
    __uint64_t		cttarg, zero	= 0;
    int			r;

    if ((r = wait_ready(callback, userdata)) < 0)
	goto done;

    if ((r = (*callback)(BIST_CB_ENTER_RUN, &zero, userdata)) < 0)
	goto done;

    /* Should not be reached */

    if ((r = wait_ready(callback, userdata)) < 0)
	goto done;

    r = 0;

 done:
    return r;
}

/*
 * Hub-specific BIST
 */

extern __uint64_t	cbist_hub_tdr[];	/* See bist_data.c */
extern __uint64_t	lbist_hub_tdr[];
extern __uint64_t	abist_hub_tdr[];

/*ARGSUSED*/
static int bist_hub_cb(int request,
		       __uint64_t *data,
		       __psunsigned_t userdata)
{
    nasid_t		nasid	= (nasid_t) userdata;

    switch (request) {
    case BIST_CB_WRITE_DATA:
#if PDEBUG
	printf("W WRITE_DATA   %lx\n", *data);
#endif
	SD(REMOTE_HUB(nasid, PI_BIST_WRITE_DATA), *data);
	break;
    case BIST_CB_READ_DATA:
	*data = LD(REMOTE_HUB(nasid, PI_BIST_READ_DATA));
#if PDEBUG
	printf("R READ_DATA    %lx\n", *data);
#endif
	break;
    case BIST_CB_COUNT_TARGET:
#if PDEBUG
	printf("W COUNT_TARGET %#lx\n", *data);
#endif
	SD(REMOTE_HUB(nasid, PI_BIST_COUNT_TARG), *data);
	break;
    case BIST_CB_READY:
#ifdef SABLE
	*data = 1;
#else
	*data = LD(REMOTE_HUB(nasid, PI_BIST_READY));
#endif
#if PDEBUG
	printf("R READY        %ld\n", *data);
#endif
	break;
    case BIST_CB_SHIFT_LOAD:
#if PDEBUG
	printf("W SHIFT_LOAD   %ld\n", *data);
#endif
	SD(REMOTE_HUB(nasid, PI_BIST_SHIFT_LOAD), *data);
	break;
    case BIST_CB_SHIFT_UNLOAD:
#if PDEBUG
	printf("W SHIFT_UNLOAD %ld\n", *data);
#endif
	SD(REMOTE_HUB(nasid, PI_BIST_SHIFT_UNLOAD), *data);
	break;
    case BIST_CB_ENTER_RUN:
#if PDEBUG
	printf("W ENTER_RUN    %ld\n", *data);
#endif
	SD(REMOTE_HUB(nasid, PI_BIST_ENTER_RUN), *data);
	break;
    default:
	return -999;
    }

    return 0;
}

/*
 * lbist_hub_execute
 *
 *   Executes the logic BIST, which checks every logic gate throughout
 *   the HUB chip.  WARNING: The T5 resets after executing a HUB logic BIST.
 */

int lbist_hub_execute(nasid_t nasid)
{
    int			r;

    if ((r = bist_write(bist_hub_cb, (__psunsigned_t) nasid,
			BIST_TARGET_CLOCK,
			cbist_hub_tdr, CBIST_HUB_TDR_BITS)) < 0)
	return r;

    if ((r = bist_write(bist_hub_cb, (__psunsigned_t) nasid,
			BIST_TARGET_LBIST,
			lbist_hub_tdr, LBIST_HUB_TDR_BITS)) < 0)
	return r;

    if ((r = bist_execute(bist_hub_cb, (__psunsigned_t) nasid,
			  BIST_TARGET_LBIST)) < 0)
	return r;

    return 0;
}


/*
 * lbist_hub_results
 *
 *   Displays MISR field and status fields for hub lbist ops. Checks these
 *   against correct values for recognized revisions. 
 *  
 */

int lbist_hub_results(nasid_t nasid, int diag_mode)
{
    __uint64_t          result[LBIST_HUB_TDR_DWORDS];
    __uint64_t          value, misr;
    int                 hub_rev, r;
    int                 verbose = 0,
			diag_rc = 0;


    if (diag_mode == DIAG_MODE_MFG)
	verbose = 1;
	
    if ((r = bist_read(bist_hub_cb, (__psunsigned_t) nasid,
                       BIST_TARGET_LBIST,
                       result, LBIST_HUB_TDR_BITS)) < 0)
        return r;

    misr = (__uint64_t) get_field(result, LBIST_HUB_TDR_MISR, 62);
    r = (int) get_field(result, LBIST_HUB_TDR_STATUS, 2);

    if (verbose) {
        printf("MISR:   %-62lx\n", misr);
        printf("Status: %02d\n", r);
    }

    value = LD(LOCAL_HUB(NI_STATUS_REV_ID));
    hub_rev = (int) ((value & NSRI_REV_MASK) >> NSRI_REV_SHFT);

    /*
     * Check BIST results after making sure the status value is correct
     */

    if (r == STATUS_BIST_RAN) {
    	if (hub_rev == HUB2) {
            if ((misr == HUB2_LBIST_RESULTS) | (misr == HUB2_LBIST_ALT)) {
                diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_PASSED);
                result_pass("hub_lbist", diag_mode);
            } else {
                diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_HUB_FAIL_LBIST);
		result_diagnosis("hub_lbist", hub_slot_get(), hub_cpu_get());
                result_fail("hub_lbist", diag_rc, "lbist has bad signature");
            }

        } else if (hub_rev == HUB2_1) {
            if (misr == HUB2_1_LBIST_RESULTS) {
                diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_PASSED);
                result_pass("hub_lbist", diag_mode);
            } else {
                diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_HUB_FAIL_LBIST);
                result_diagnosis("hub_lbist", hub_slot_get(), hub_cpu_get());
                result_fail("hub_lbist", diag_rc, "lbist has bad signature");
            }

    	} else {
	    if (verbose)
            	printf("Can't check results--unknown HUB revision: %d\n", 
		hub_rev);

            diag_rc = 0;
        }

    } else {  

	/* The status value was bad */

	result_diagnosis("hub_lbist", hub_slot_get(), hub_cpu_get());
	result_fail("hub_lbist", diag_rc, "lbist had incomplete/error status");
    }
	    
    return diag_rc;
}

/*
 * get_hub_bist_status
 *
 *   Checks the bist bits in the elsc nvram to see what bist ops 
 *   (if any) have run. This function expects to retrieve the bist_
 *   status as a char but returns it as an int.
 */

int get_hub_bist_status(int slot_num)

{
    int 	bist_status;

    bist_status = (int) ((elsc_bist_get(get_elsc()) & BIST_SLOT_MASK(slot_num))
				>> BIST_SLOT_SHIFT(slot_num));
    return bist_status;
}

/*
 * set_hub_bist_status
 *
 *   Sets the bist bits in the PROMOP_REG to keep track of which bist 
 *   ops have run. This function gets the bist status as an int, but
 *   manipulates it to be in the correct bit positions, and then 
 *   writes it to the elsc nvram as a char.
 */

void set_hub_bist_status(int slot_num, int bist_status)
{
    char	value;

    value = elsc_bist_get(get_elsc());

    /* make a char value with only 2 bits set */
    bist_status = (char) ((bist_status << BIST_SLOT_SHIFT(slot_num)) 
				& BIST_SLOT_MASK(slot_num));

    /* mask off appropriate part of current elsc nvram value */
    value = (char) (value & (~BIST_SLOT_MASK(slot_num)));
    
    /* get composite new value to write back */
    bist_status = bist_status | value;

    elsc_bist_set(get_elsc(), bist_status); 

}

/*
 * abist_hub_execute
 *
 *   Executes the hub array BIST, which checks every storage cell throughout
 *   the HUB chip.  WARNING: The T5 resets after executing an array BIST.
 */

int abist_hub_execute(nasid_t nasid)
{
    int			r;

    if ((r = bist_write(bist_hub_cb, (__psunsigned_t) nasid,
			BIST_TARGET_CLOCK,
			cbist_hub_tdr, CBIST_HUB_TDR_BITS)) < 0)
	return r;

    if ((r = bist_write(bist_hub_cb, (__psunsigned_t) nasid,
			BIST_TARGET_ABIST,
			abist_hub_tdr, ABIST_HUB_TDR_BITS)) < 0)
	return r;

    if ((r = bist_execute(bist_hub_cb, (__psunsigned_t) nasid,
			  BIST_TARGET_ABIST)) < 0)
	return r;

    return 0;
}


/*
 * abist_hub_results
 *
 *   Displays the results of the last array bist (if verbose is true) and
 *   returns the BIST status.
 */

int abist_hub_results(nasid_t nasid, int diag_mode)
{
    __uint64_t          value, result[ABIST_HUB_TDR_DWORDS];
    int                 hub_rev, r, gram_results;
    int                 verbose = 0,
			diag_rc = 0;

    if (diag_mode == DIAG_MODE_MFG)
        verbose = 1;

    if ((r = bist_read(bist_hub_cb, (__psunsigned_t) nasid,
                       BIST_TARGET_ABIST,
                       result, ABIST_HUB_TDR_BITS)) < 0)
        return r;

    gram_results = (int) get_field(result, ABIST_HUB_TDR_GRAM_RESULTS, 12);
    r = (int) get_field(result, ABIST_HUB_TDR_STATUS, 2);

    if (verbose) {
        printf("Gram_Results:   %-012b\n", gram_results);
        printf("Status:         %02d\n", r);
    }

    value = LD(LOCAL_HUB(NI_STATUS_REV_ID));   
    hub_rev = (int) ((value & NSRI_REV_MASK) >> NSRI_REV_SHFT);

    /* Check for a good status value before checking gram_results */

    if (r == STATUS_BIST_RAN) {
	
	/* Only check results for recognized hub revision levels */

        if ((hub_rev == HUB2) | (hub_rev == HUB2_1)) { 

	    if (gram_results == HUB2_ABIST_RESULTS) {
		diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_PASSED);
		result_pass("hub_abist", diag_mode);
	    } else {
		diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_HUB_FAIL_ABIST);
		result_diagnosis("hub_abist", hub_slot_get(), hub_cpu_get());
		result_fail("hub_abist", diag_rc, "hub abist test failed");
	    }
	} else {
	    if (verbose)
		printf("Can't check results--unknown HUB revision: %d\n", 
		hub_rev);
	    diag_rc = 0;
	}	
    } 

    /* The status field was bad--BIST never completed successfully */

    else {
	diag_rc = make_diag_rc(DIAG_PARM_TBD, KLDIAG_HUB_FAIL_ABIST);
	result_fail("hub_abist", diag_rc, "abist had incomplete/error status");
    }

    return diag_rc;
}

/*
 * Router-specific BIST
 */

extern __uint64_t	cbist_router_tdr[];	/* See bist_data.c */
extern __uint64_t	lbist_router_tdr[];
extern __uint64_t	abist_router_tdr[];

static int bist_router_cb(int request,
			  __uint64_t *data,
			  __psunsigned_t userdata)
{
    int			r, reg, vread;

    switch (request) {
    case BIST_CB_WRITE_DATA:
	vread	= 0;
	reg	= RR_BIST_DATA;
	break;
    case BIST_CB_READ_DATA:
	vread	= 1;
	reg	= RR_BIST_DATA;
	break;
    case BIST_CB_COUNT_TARGET:
	vread	= 0;
	reg	= RR_BIST_COUNT_TARG;
	break;
    case BIST_CB_READY:
	vread	= 1;
	reg	= RR_BIST_READY;
	break;
    case BIST_CB_SHIFT_LOAD:
	vread	= 0;
	reg	= RR_BIST_SHIFT_LOAD;
	break;
    case BIST_CB_SHIFT_UNLOAD:
	vread	= 0;
	reg	= RR_BIST_SHIFT_UNLOAD;
	break;
    case BIST_CB_ENTER_RUN:
	vread	= 0;
	reg	= RR_BIST_ENTER_RUN;
	break;
    default:
	return -999;
    }

    if (vread)
	/* nasid is hardwired to 0 in both vector_read and vector_write below */
	r = vector_read((net_vec_t) userdata, 0, reg, data);
    else
	r = vector_write((net_vec_t) userdata, 0, reg, *data);

    return r;
}

/*
 * lbist_router_execute
 *
 *   Executes the logic BIST, which checks every logic gate throughout
 *   the router chip.  WARNING: Router is reset after BIST.
 */

int lbist_router_execute(net_vec_t vec)
{
    int			r;

    if (!(r = net_link_up())) {
	if (r = net_link_down_reason())
	    printf("LLP Link never came out of reset; cannot talk to router!\n");
	else
	    printf("LLP Link failed after reset; cannot talk to router!\n");
	return KLDIAG_HUB_LLP_DOWN;
    } 

    if ((r = bist_write(bist_router_cb, (__psunsigned_t) vec,
			BIST_TARGET_CLOCK,
			cbist_router_tdr, CBIST_ROUTER_TDR_BITS)) < 0)
	return r;

    if ((r = bist_write(bist_router_cb, (__psunsigned_t) vec,
			BIST_TARGET_LBIST,
			lbist_router_tdr, LBIST_ROUTER_TDR_BITS)) < 0)
	return r;

    if ((r = bist_execute(bist_router_cb, (__psunsigned_t) vec,
			  BIST_TARGET_LBIST)) < 0)
	return r;

    return 0;
}

/*
 * lbist_router_results
 *
 *   Displays the results of the last logic bist (if verbose is true) and
 *   returns the BIST status.
 */

int lbist_router_results(net_vec_t vec, int diag_mode)
{
    __uint64_t          result[LBIST_ROUTER_TDR_DWORDS];
    __uint64_t          misr;
    int                 verbose, r;

    if (diag_mode == DIAG_MODE_MFG)
        verbose = 1;

    if ((r = bist_read(bist_router_cb, (__psunsigned_t) vec,
                       BIST_TARGET_LBIST,
                       result, LBIST_ROUTER_TDR_BITS)) < 0)
        return r;

    misr = (int) get_field(result, LBIST_ROUTER_TDR_MISR, 62);
    r = (int) get_field(result, LBIST_ROUTER_TDR_STATUS, 2);

    if (verbose)
    {
        printf("MISR:   %-062lx\n", misr);
        printf("Status: %02d\n", r);
    }

    return r;
}

/*
 * abist_router_execute
 *
 *   Executes the array BIST, which checks every storage cell throughout
 *   the router chip.  WARNING: Router resets after executing an array BIST.
 */

int abist_router_execute(net_vec_t vec)
{
    int			r;

    if ((r = bist_write(bist_router_cb, (__psunsigned_t) vec,
			BIST_TARGET_CLOCK,
			cbist_router_tdr, CBIST_ROUTER_TDR_BITS)) < 0)
	return r;

    if ((r = bist_write(bist_router_cb, (__psunsigned_t) vec,
			BIST_TARGET_ABIST,
			abist_router_tdr, ABIST_ROUTER_TDR_BITS)) < 0)
	return r;

    if ((r = bist_execute(bist_router_cb, (__psunsigned_t) vec,
			  BIST_TARGET_ABIST)) < 0)
	return r;

    return 0;
}

/*
 * abist_router_results
 *
 *   Displays the results of the last array bist (if verbose is true) and
 *   returns the BIST status.
 */

int abist_router_results(net_vec_t vec, int diag_mode)
{
    __uint64_t          result[ABIST_ROUTER_TDR_DWORDS];
    int                 gram_results;
    int                 verbose, r;

    if (diag_mode == DIAG_MODE_MFG)
        verbose = 1;

    if ((r = bist_read(bist_router_cb, (__psunsigned_t) vec,
                       BIST_TARGET_ABIST,
                       result, ABIST_ROUTER_TDR_BITS)) < 0)
        return r;

    gram_results = (int) get_field(result, ABIST_ROUTER_TDR_GRAM_RESULTS, 12);
    r = (int) get_field(result, ABIST_ROUTER_TDR_STATUS, 2);

    if (verbose) {
        printf("Gram_Results:   %012b\n", gram_results);
        printf("Status:         %02d\n", r);
    }

    return r;
}


