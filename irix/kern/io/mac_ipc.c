/*
 * Copyright 1990, Silicon Graphics, Inc. 
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
#ident	"$Revision: 1.10 $"

#include	"sys/types.h"
#include	"sys/param.h"
#include	"sys/errno.h"
#include	"sys/ipc.h"
#include	"sys/cred.h"
#include	"sys/debug.h"
#include	"sys/mac_label.h"
#include	"sys/systm.h"
#include	"sys/sysmp.h"
#include	"sys/sat.h"

static mac_label	**mac_sem_label;
static mac_label	**mac_msg_label;

void
mac_sem_init( int size )
{
	mac_sem_label = (mac_label **)kern_calloc(sizeof(mac_label *), size);
	ASSERT (mac_sem_label);
	sysmp_mpka[MPKA_SEM_MAC - 1] = (caddr_t)mac_sem_label;
}

void
mac_msg_init( int size )
{
	mac_msg_label = (mac_label **)kern_calloc(sizeof(mac_label *), size);
	ASSERT (mac_msg_label);
	sysmp_mpka[MPKA_MSG_MAC - 1] = (caddr_t)mac_msg_label;
}

static int
mac_ipc_access( mac_label *mlp, cred_t *cr )
{
        int error;

        error = _MAC_ACCESS(mlp, cr, MACWRITE);
        _SAT_SVIPC_ACCESS(mlp, 0,
	    SAT_MAC | (error ? SAT_FAILURE : SAT_SUCCESS), error);
        return (error);
}

int
mac_sem_access( int indx, cred_t *cr )
{

	return (mac_ipc_access(mac_sem_label[indx], cr));
}

int
mac_msg_access( int indx, cred_t *cr )
{

	return (mac_ipc_access(mac_msg_label[indx], cr));
}

void
mac_sem_setlabel( int indx, mac_label *lp )
{

	mac_sem_label[indx] = lp;
}

void
mac_msg_setlabel( int indx, mac_label *lp )
{

	mac_msg_label[indx] = lp;
}
