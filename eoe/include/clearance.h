#ifndef __CLEARANCE_H__
#define __CLEARANCE_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.2 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/

/*
 * Default path to clearance database.
 */
#define CLEARANCE	"/etc/clearance"

/*
 * Return codes from mac_cleared() and related functions.
 */
#define	MAC_CLEARED		  0	/* user is cleared at requested lbl  */
#define	MAC_NULL_USER_INFO	 -1	/* uip argument == 0		     */
#define	MAC_NULL_REQD_LBL	 -2	/* label argument == 0		     */
#define	MAC_BAD_REQD_LBL	 -3	/* user's requested label is bad     */
#define	MAC_MSEN_EQUAL  	 -4	/* nobody can login at msen equal    */
#define	MAC_MINT_EQUAL  	 -5	/* only root can login at mint equal */
#define	MAC_BAD_USER_INFO	 -6	/* clearance field bad label(s)	     */
#define	MAC_NULL_CLEARANCE	 -7	/* no clearance field                */
#define	MAC_LBL_TOO_LOW 	 -8	/* lo_u_lbl > requested user label   */
#define	MAC_LBL_TOO_HIGH	 -9	/* hi_u_lbl < requested user label   */
#define	MAC_INCOMPARABLE	-10	/* reqd label incomparable to range  */
#define	MAC_NO_MEM		-11	/* no memory available               */
#define	MAC_BAD_RANGE		-12	/* Bad range in clearance field      */

#define	MAC_LAST_CLEARANCE_ERROR	MAC_INCOMPARABLE

struct clearance {
	char *cl_name;		/* Name */
	char *cl_default;	/* Default Clearance */
	char *cl_allowed;	/* Allowed Clearances */
};
struct mac_label;

int mac_cleared (const struct clearance *, const char *);
int mac_clearedlbl (const struct clearance *, struct mac_label *);

int mac_cleared_fl (const struct clearance *, struct mac_label *);
int mac_cleared_pl (const struct clearance *, struct mac_label *);
int mac_cleared_fs (const struct clearance *, const char *);
int mac_cleared_ps (const struct clearance *, const char *);

char *mac_clearance_error (int);
struct clearance *sgi_getclearancebyname (const char *);

#ifdef __cplusplus
}
#endif
#endif /* !__CLEARANCE_H__ */
