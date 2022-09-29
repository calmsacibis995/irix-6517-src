/*
 * @(#) $Id: rsvp_lpm.h,v 1.4 1998/11/25 08:43:36 eddiem Exp $
 */

/***************************** rlpm.h ********************************
 *                                                                   *
 *              Local Policy Header Files                            *
 *                                                                   *
 *   We put here all stuff that the RSVP daemon needs in addition    *
 *   to the basic LPM services that are in rlpm_serv.[ch]            *
 *                                                                   *
 *********************************************************************/

#ifndef __rsvp_lpm_h__
#define __rsvp_lpm_h__

int  lpm_status_fail(destination *dest,sender *s2,int vif,int rc, int lerr);
int  lpm_resp_msg(destination *dest);
int  lpm_status_dest(destination *dest);
int *scope2fh(sender *sndp, SCOPE *scope, int *len, int ind);

#endif	/* __rsvp_lpm_h__ */




