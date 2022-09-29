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

#ident "$Revision: 1.6 $"

#include "sys/types.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/mac_label.h"
#include "sys/capability.h"
#include "sys/sat.h"

/*
 * Access check for Mandatory Access Control in a capabilities environment.
 */
int
mac_access(mac_label *lp, struct cred *cr, mode_t mode)
{
	/*
	 * If the labels are equal
	 * reading is always OKay, writing is always OKay.
	 */
	if (mac_equ(cr->cr_mac, lp))
		return (0);

	/*
	 * The labels are not equal.
	 */
	if (mode & MACWRITEUP) {
		if (mac_dom(lp, cr->cr_mac))
			return (0);
		else
			return (cap_able_cred(cr, CAP_MAC_WRITE) ? 0 : EACCES);
	}

	if ((mode & MACWRITE) && !cap_able_cred(cr, CAP_MAC_WRITE))
		return (EACCES);

	if ((mode & (MACREAD | MACEXEC)) &&
	    !mac_dom(cr->cr_mac, lp) &&
	    !cap_able_cred(cr, CAP_MAC_READ))
		return (EACCES);

	return (0);
}

/*
 * In the case where MAC and capabilities are enabled
 * the credential for init(1) has the label msenlow/minthigh (dblow).
 * The MAC label has no power over privilege, like it does if there's
 * MAC but no capabilities.
 */

void
mac_init_cred(void)
{
	extern cred_t *sys_cred;
	extern mac_label *mac_low_high_lp;

	sys_cred->cr_mac = mac_low_high_lp;
}
