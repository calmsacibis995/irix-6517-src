
/*
 * @(#) $Id: tc_test.c,v 1.7 1997/11/14 07:27:24 mwang Exp $
 */

/************************ tc_test.c  *********************************
 *                                                                   *
 *  Adaptation Module: Converts RSVP's TC interface into kernel      *
 *	interface.  But this is dummy, for testing.                  *
 *                                                                   *
 *********************************************************************/


#include <stddef.h>

#include "rsvp_daemon.h"
#include "rapi_lib.h"		/* Define flowspec formats */
#include "rsvp_specs.h"		/* Flowspec descriptor format */
#include "rsvp_TCif.h"		/* Adaptation interface */

/* external declarations */
static int	udp_socket = -1;
static int	tc_clear __P((int));
int		gen_handle = 0;
Object_header	*copy_object(Object_header *);


#define TC_flags (TCF_E_POLICE | TCF_M_POLICE | TCF_B_POLICE)
	

float32_t	Fail_Level;

/************************************************************************
 *
 *	Interface routines to call the ISPS kernel functions
 * 
 ************************************************************************/

void
TC_init(int kernel_socket)
	{
	int 	i;
	FILE 	*fd;

	/*  Initialize UDP socket used to communicate with the
	 *	kernel traffic control code.
	 */
	udp_socket = kernel_socket;

	/* For testing, look for config file to define a failure level
	 *	for either parameter of token bucket.
	 */

	Fail_Level = 1.0e12;	/* Infinity */
	fd = fopen("tc.conf", "r");
	if (fd) {
		fscanf(fd, "%e", &Fail_Level);
		fprintf(stdout, "SET FAILURE LEVEL = %f\n", Fail_Level);
	}

        /* clear old reservations from the kernel and then make a
	 *	reservation for RSVP packets, on each interface.
	 */
        for (i = 0; i < if_num; i++)
                tc_clear(i);
}

/*
 * TC_AddFlowspec(): Call the kernel to make reservation for a flow.
 * 	It checks admission control, and returns a handle for the
 *	reservation, or -1 if an error.  It may also set *fwd_specpp
 * 	to point to a modified flowspec to be forwarded.
 */
u_long
TC_AddFlowspec(int OIf, FLOWSPEC *spec, SENDER_TSPEC *stspec, int flags,
							FLOWSPEC **fwd_specpp)
	{
	FLOWSPEC *s1 = spec;
	IS_serv_hdr_t	*sp1;

	sp1 = (IS_serv_hdr_t *) &s1->flow_body.spec_u;
	switch (sp1->issh_service) {

	    case GUARANTEED_SERV:
		{ Guar_flowspec_t *o1 = (Guar_flowspec_t *) sp1;

		if (o1->Gspec_r >= Fail_Level ||
		    o1->Gspec_b >= Fail_Level) {
			rsvp_errno = Set_Errno(RSVP_Err_ADMISSION,
							RSVP_Erv_Other);
			return TC_ERROR;
		}}
		break;

	    case CONTROLLED_LOAD_SERV:
		{ CL_flowspec_t *o1 = (CL_flowspec_t *) sp1;

		if (o1->CLspec_r >= Fail_Level ||
		    o1->CLspec_b >= Fail_Level) {
			rsvp_errno = Set_Errno(RSVP_Err_ADMISSION,
							RSVP_Erv_Other);
			return TC_ERROR;
		}}
		break;

	default:
		rsvp_errno = Set_Errno(RSVP_Err_TC_ERROR, RSVP_Erv_No_Serv);
		return TC_ERROR;
	}

	return (gen_handle++);
}

/*
 * TC_DelFlowspec(): This routine deletes flow for specified handle
 */
int
TC_DelFlowspec(int OIf, u_long rhandle) {
	return (TC_OK);
}

/*
 * TC_AddFilter(): Adds a filter for an existing flow.
 *
 *	Returns fhandle or TC_ERROR.
 */
u_long
TC_AddFilter(int OIf, u_long rhandle, Session *dest, FILTER_SPEC *filtp)
	{
	return (gen_handle);
}


/*
 * TC_DelFilter(): Deletes existing filter.
 */
int
TC_DelFilter(int OIf, u_long rhandle)
	{
	return (TC_OK);
}

/*
 * TC_ModFlowspec(): Modifies a flowspec of a given flow.
 *
 *	It may also set *fwd_specpp to point to a modified flowspec to
 *	be forwarded.
 */
int
TC_ModFlowspec(int OIf, u_long rhandle,
	 		FLOWSPEC *specp, SENDER_TSPEC *stspecp, int flags,
						FLOWSPEC **fwd_specpp)
	{
	FLOWSPEC *s1 = specp;
	IS_serv_hdr_t	*sp1;

	sp1 = (IS_serv_hdr_t *) &s1->flow_body.spec_u;
	switch (sp1->issh_service) {

	    case GUARANTEED_SERV:
		{ Guar_flowspec_t *o1 = (Guar_flowspec_t *) sp1;

		if (o1->Gspec_r >= Fail_Level ||
		    o1->Gspec_b >= Fail_Level) {
			rsvp_errno = Set_Errno(RSVP_Err_ADMISSION,
							RSVP_Erv_Other);
			return TC_ERROR;
		}}
		break;

	    case CONTROLLED_LOAD_SERV:
		{ CL_flowspec_t *o1 = (CL_flowspec_t *) sp1;

		if (o1->CLspec_r >= Fail_Level ||
		    o1->CLspec_b >= Fail_Level) {
			rsvp_errno = Set_Errno(RSVP_Err_ADMISSION,
							RSVP_Erv_Other);
			return TC_ERROR;
		}}
		break;

	default:
		rsvp_errno = Set_Errno(RSVP_Err_TC_ERROR, RSVP_Erv_No_Serv);
		return TC_ERROR;
	}

	return (TC_OK);
}

#define In_Obj(x, y) ((Object_header *)(x) <= Next_Object((Object_header *)(y)))

/*
 * TC_Advertise(): Given existing OPWA ADSPEC, return a new updated object.
 */
ADSPEC *
TC_Advertise(int OIf, ADSPEC * old_asp, int flags)
	{
	ADSPEC		*new_asp = copy_adspec(old_asp);
	/*	We know we will not expand the ADSPEC, so start by
	 *	just making a straight copy.
	 */
	IS_main_hdr_t	*mhp = (IS_main_hdr_t *) Obj_data(new_asp);
	genparm_parms_t *gpp = (genparm_parms_t *)(mhp +1);
	IS_serv_hdr_t	*shp, *lastshp;
	float32_t	hop_bw;
	u_int32_t	hop_latency, hop_mtu;

	if (!if_vec[OIf].if_up || (flags & ADVERTF_NonRSVP))
		Set_Break_Bit(&gpp->gen_parm_hdr);
	
	gpp->gen_parm_hopcnt++;

	hop_bw = (if_vec[OIf].if_path_bw)?if_vec[OIf].if_path_bw:
							(TC_DFLT_PATH_BW);
	gpp->gen_parm_path_bw = MIN(gpp->gen_parm_path_bw, hop_bw);

	hop_latency = (if_vec[OIf].if_min_latency)?if_vec[OIf].if_min_latency:
							TC_DFLT_MIN_LATENCY;
	gpp->gen_parm_min_latency = MIN(gpp->gen_parm_min_latency, hop_latency);
	
	hop_mtu = (if_vec[OIf].if_path_mtu)?if_vec[OIf].if_path_mtu: 
							TC_DFLT_MTU;
	gpp->gen_parm_composed_MTU = MIN(gpp->gen_parm_composed_MTU, hop_mtu);
	
	/* We KNOW that we are not capable of traffic control, so we
	 * set Break bit in each service.  We DO NOT set the Break
	 * bit in the generic parameters, because we DO understand
	 * ADSPECs, unless previous hop was not RSVP-capable.
	 */
	shp = Next_Serv_Hdr((IS_serv_hdr_t *)gpp);
	lastshp  = (IS_serv_hdr_t *) Next_Main_Hdr(mhp);
	if (!In_Obj(lastshp, new_asp)) {
		/* Internal length falls outside object...
		 *  Increment error counter
		 */
		free(new_asp);
		return(NULL);
	}
	while (shp < lastshp) {
		Set_Break_Bit(shp);
		shp = Next_Serv_Hdr(shp);
	}
	return(new_asp);
}

/*
 * tc_clear(): This routine resets an interface, cleaning up old
 * 	reservations. It is used when RSVP daemon is initialized.
 */
static int
tc_clear(int in_if) {
    	char           *if_name;
    
    	if_name = if_vec[in_if].if_name;
	if_vec[in_if].if_up = 0;	    
 
	/*
	 *	Mark Traffic Control up on interface
	 */
	if_vec[in_if].if_up = 1;
	return (1);
}

