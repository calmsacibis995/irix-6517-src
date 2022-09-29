/*
 * Copyright 1996, Silicon Graphics, Inc. 
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

#ifndef	CONFIG_BITS_H__
#define	CONFIG_BITS_H__

/* The bits for the various things configured that we need to know about */
#define ACL	0x0001
#define AUDIT	0x0002
#define CIPSO	0x0004
#define MAC	0x0008
#define CAP	0x0010

/* The variable that they are stored in */
extern long sys_config_mask;

/* The individual variables for each bit above */
extern short	acl_enabled;	/* TRUE if ACL configured */
extern short	audit_enabled;	/* TRUE if AUDIT configured */
extern short	cipso_enabled;	/* TRUE if SC_IP_SECOPTS configured */
extern short	mac_enabled;	/* TRUE if MAC configured */
extern short	cap_enabled;	/* TRUE if CAPabilities configured */

#endif /* ifndef config_bits.h */
