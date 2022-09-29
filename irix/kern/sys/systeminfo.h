/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SVC_SYSTEMINFO_H	/* wrapper symbol for kernel use */
#define _SVC_SYSTEMINFO_H	/* subject to change without notice */

#ident	"$Revision: 1.12 $"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#define MAX_SERIAL_SIZE 16

typedef struct module_info_s {
	__uint64_t serial_num;
	int mod_num;
	char serial_str[MAX_SERIAL_SIZE];
} module_info_t;



/*
 * Commands to sysinfo()
 */

#define SI_SYSNAME		1	/* return name of operating system */
#define SI_HOSTNAME		2	/* return name of node */
#define SI_RELEASE 		3	/* return release of operating system */
#define SI_VERSION		4	/* return version field of utsname */
#define SI_MACHINE		5	/* return kind of machine */
#define SI_ARCHITECTURE		6	/* return instruction set arch */
#define SI_HW_SERIAL		7	/* return hardware serial number */
#define SI_HW_PROVIDER		8	/* return hardware manufacturer */
#define SI_SRPC_DOMAIN		9	/* return secure RPC domain */
#define SI_INITTAB_NAME	       10	/* return name of inittab file used */

#define _MIPS_SI_VENDOR		100	/* return system provider */
#define _MIPS_SI_OS_PROVIDER	101	/* return OS manufacturer */
#define _MIPS_SI_OS_NAME	102	/* return OS name */
#define _MIPS_SI_HW_NAME	103	/* return system name */
#define _MIPS_SI_NUM_PROCESSORS	104	/* return number of processors */
#define _MIPS_SI_HOSTID		105	/* return hostid */
#define _MIPS_SI_OSREL_MAJ	106	/* return OS major release number */
#define _MIPS_SI_OSREL_MIN	107	/* return OS minor release number */
#define _MIPS_SI_OSREL_PATCH	108	/* return OS release number */
#define _MIPS_SI_PROCESSORS	109	/* return CPU revison id */
#define _MIPS_SI_AVAIL_PROCESSORS 110	/* return number of available processors */
#define	_MIPS_SI_SERIAL		111
/*
 * These commands are unpublished interfaces to sysinfo().
 */
#define SI_SET_HOSTNAME		258	/* set name of node */
					/*  -unpublished option */
#define SI_SET_SRPC_DOMAIN	265	/* set secure RPC domain */
					/* -unpublished option */

#if !defined(_KERNEL)
int sysinfo(int, char *, long);
int get_num_modules(void);
int get_module_info(int, module_info_t *, size_t);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SVC_SYSTEMINFO_H */
