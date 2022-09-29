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
#ident	"$Revision: 1.4 $"

/*
 *	Resolver-specific version of Internet name-to-address shared object.
 */
#define _gethostbyaddr _gethostbyaddr_named
#define _gethostbyname _gethostbyname_named

/*
 * Recompile generic name to address mapping code to be host-table specific 
 */
#include "../../vis/vis.c"
