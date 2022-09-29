/*
 * dnhost.h --
 *
 * 	Support for DECnet address/name translation.
 *
 *
 * Copyright 1991, Silicon Graphics, Inc. 
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

#ifndef __DNHOST_H__
#define __DNHOST_H__

#ident "$Revision: 1.3 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) || defined(__STDC__)

/*
 * Byte order of the DECnet address.
 */
#define	ORD_HOST	0	/* big endian */
#define	ORD_NET		1	/* little endian */

/*
 * Address/Name translation routines. The <mode> agrument indicates the
 * byte order of the DECnet address. When the name/address cannot be
 * retreived for whatever reasons, NULL is returned.
 */
extern char		*dn_getnodename(unsigned short nodeaddr, char mode);
extern unsigned short	dn_getnodeaddr(char *nodename, char mode);

#endif /* C || C++ */

#endif /* !__DNHOST_H__ */
