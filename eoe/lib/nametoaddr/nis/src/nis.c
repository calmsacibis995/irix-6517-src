/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1990 MIPS Computer Systems, Inc.            |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 52.227-7013.   |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Drive                                |
 * |         Sunnyvale, CA 94086                               |
 * |-----------------------------------------------------------|
 */
#ident	"$Revision: 1.3 $"

/*
 *	NIS-specific version of Internet name-to-address shared
 *	object.
 */

/*
 *	map the generic names to the _yp_* names used below the
 *	VIS interface 
 */

#define	_gethostbyaddr _yp_gethostbyaddr
#define	_gethostbyname _yp_gethostbyname
#define	_getservbyname _yp_getservbyname
#define	_getservbyport _yp_getservbyport

/*
 *	recompile the generic name to address mapping code to be
 *	host-table specific 
 */

#include "../../vis/vis.c"

