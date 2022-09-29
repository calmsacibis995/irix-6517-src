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

/*
 * sat_interpret -	convert audit records from binary to human
 *			readable form
 */

#ident  "$Id: sat_interpret31.c,v 1.1 1998/06/01 18:29:16 deh Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys.s>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>			/* required for ip.h */
#include <sys/mac.h>
#include <sys/so_dac.h>
#include <sat.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <assert.h>
#include <net/if.h>
#include <net/if_sec.h>
#include <net/route.h>
#include <sys/ioctl.h>			/* also gets <net/soioctl.h> */
#include <sys/stropts.h>
#include <netinet/in_systm.h>		/* required for ip.h */
#include <netinet/ip.h>
#include <netinet/ip_var.h>		/* required for tcpip.h */
#include <netinet/tcp.h>		/* required for tcpip.h */
#include <netinet/tcpip.h>
#include <netinet/in.h>			/* for sockaddr_in */
#include <arpa/inet.h>			/* for inet_ntoa() */
#include <sys/sat_compat.h>
#include <sys/extacct.h>
#include <sys/sesmgr_if.h>
#include <sys/termios.h>
#include "fields.h"

extern char *sys_call(int,int);

#define	MAX_REC_SIZE	2408
#define ALIGN(x) ((x) + align[(int)(x) % 4])

/* #defines needed to run under Irix6.5 */

#define TIOCSIGNAL	(tIOC|31)	/*  pty: send signal to slave
					 *  Not used in Irix6.5
					 *  Needed for Irix6.4 records	*/

int align[4] = { 0, 3, 2, 1 };

#ifdef IFSEC_VECTOR
#define SEC(ifr)	((ifsec_t *)((ifr)->ifr_base))
#else /* IFSEC_VECTOR */
#define SEC(ifr)	(&(ifr)->ifr_sec)
#endif /* IFSEC_VECTOR */

/*
 * Output modes: verbose, brief, linear, and
 * SGIF binary format.
 */
#define OM_BRIEF	0
#define OM_VERBOSE	1
#define OM_LINEAR	2
#define OM_SGIF		3

int brief = OM_VERBOSE;	/* brief output? */
int debug = SAT_FALSE;	/* include debugging output? */
int fdmap = 0;		/* map file descriptors to file names? */
int mac_enabled = SAT_TRUE;	/* was mac enabled when the file was produced? */
int cap_enabled = SAT_TRUE;	/* was cap enabled when the file was produced? */
int cipso_enabled = SAT_TRUE; /* was cipso enabled when the file was produced? */
int normalize = 0;	/* normalized output? */

double fileoffset = 0.0;

struct sat_list_ent **users = NULL;	/* username <-> userid table */
struct sat_list_ent **groups = NULL;	/* groupname <-> groupid table */
struct sat_list_ent **hosts = NULL;	/* hostname <-> hostid table */
int n_users, n_groups, n_hosts;

int file_major, file_minor;		/* version number of current file */

/* prototypes */
char *	sat_str_outcome( struct sat_hdr_info * );
void	sat_debug_magic( char * );
void	sat_print_header( struct sat_hdr_info * );
char *	sat_str_domain( short );
char *	sat_str_socktype( short );
void	sat_print_sockaddr( short, char * );
char *	sat_str_mode( unsigned );
const char *sat_str_id( unsigned int, struct sat_list_ent **, int );
char *	sat_str_label( mac_t );
char *	sat_str_cap( cap_t );
char *	sat_str_umask( mode_t );
char *	sat_str_open_flags( int );
char *	sat_str_hex( unsigned char *, int );
char *	sat_str_signame( int );
char *	sat_str_dev( dev_t );
void	sat_print_ioctl( int, int, struct ifreq * );
void	sat_print_body( struct sat_hdr_info *, char * );
void	sat_print_sat_control( char * );
void    sat_print_acct_timers( struct acct_timers * );
void    sat_print_acct_counts( struct acct_counts * );

uid_t	getuid(void);

#define	sat_str_user(uid)	sat_str_id((uid), users, n_users)
#define	sat_str_group(gid)	sat_str_id((gid), groups, n_groups)
#define	sat_str_host(hostid)	sat_str_id((hostid), hosts, n_hosts)

/* SGIF */
#define NSGIF 200	/* max number of fields */
int	rlen;
char BADLABEL[] = "bad_label";
char BADCAP[] = "bad_cap";
char BADACL[] = "bad_acl";
#define ALIGN_REC   	4      /* alignment number */

/*
 * Structure to store record data before writing it to output.
 * Array index is field, as defined in fields.h.
 */
struct sgiffield {
	char *a_data;
	short a_size;
	short a_pad; /* num bytes to pad to 4-byte boundary */
	short a_free; /* set if a_data is malloc'd */
} afield[NSGIF];

struct{
	int len;
	char val[12];
} TypeSGIF= {15,"__SGIF__1|\0"};

void
assign_str(short index, char *str, short len)
/*
 * The data is a character string.
 * Assumes the address "str" will be valid when
 * dereferenced at the end of main.  This is true 
 * for addresses in the file header or record header.
 */
{
	if (str == NULL)
		return;
	afield[index].a_data = str;
	afield[index].a_size = len;
	afield[index].a_free = 0;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
} 
void
assign_maclbl(short index, mac_t lptr, short len)
/*
 * The data is a mac label.
 * Assumes the address "lptr" will be valid when
 * dereferenced at the end of main.  This is true 
 * for addresses in the file header or record header.
 */
{
	if (lptr == NULL)
		return;
	afield[index].a_data = (char *)lptr;
	afield[index].a_size = len;
	afield[index].a_free = 0;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
} 
void
assign_maclbldup(short index, mac_t lptr, short len)
/*
 * The data is a mac label.
 * Makes a copy of lptr, which must be freed after use.

 */
{
	if (lptr == NULL)
		return;
	afield[index].a_data = malloc(len);
	memcpy((void *) afield[index].a_data, (void *) lptr, (size_t) len);
	afield[index].a_size = len;
	afield[index].a_free = 1;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
} 
void
assign_capdup(short index, cap_t cptr, short len)
{
	if (cptr == NULL)
		return;
	afield[index].a_data = malloc(len);
	memcpy((void *) afield[index].a_data, (void *) cptr, (size_t) len);
	afield[index].a_size = len;
	afield[index].a_free = 1;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}
void
assign_cap(short index, cap_t lptr, short len)
/*
 * The data is a capability set.
 * Assumes the address "lptr" will be valid when
 * dereferenced at the end of main.  This is true 
 * for addresses in the file header or record header.
 */
{
	if (lptr == NULL)
		return;
	afield[index].a_data = (char *)lptr;
	afield[index].a_size = len;
	afield[index].a_free = 0;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}
void
assign_strdup(short index, char *str, short len)
/*
 * The data is a character string.
 * Makes a copy of str, which must be freed after use.
 */
{
	if (str == NULL)
		return;
	afield[index].a_data = strdup(str);
	afield[index].a_size = len;
	afield[index].a_free = 1;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}

void
assign_I16Adup(short index, short *val, short nshorts)
/*
 * The data is an array of shorts.
 * Makes a copy of val, which must be freed after use.
 */
{
	afield[index].a_data = malloc(sizeof(short) * nshorts);
	memcpy((void *) afield[index].a_data, (void *) val, sizeof(short) * nshorts);
	afield[index].a_size = (sizeof(u_short) * nshorts);
	afield[index].a_free = 1;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}

void
assign_I32A(short index, __int32_t *val, short nints)
/*
 * The data is an array of 32-bit ints.
 * Assumes the address "val" will be valid when
 * dereferenced at the end of main.  This is true 
 * for addresses in the file header or record header.
 */
{
	afield[index].a_data = (char *)(val);
	afield[index].a_size = (sizeof(__int32_t) * nints);
	afield[index].a_free = 0;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}

void
assign_I32Adup(short index, __int32_t *val, short nints)
/*
 * The data is an array of 32-bit ints.
 * Makes a copy of val, which must be freed after use.
 */
{
	afield[index].a_data = malloc(sizeof(__int32_t) * nints);
	memcpy((void *) afield[index].a_data, (void *) val, sizeof(__int32_t) * nints);
	afield[index].a_size = (sizeof(__int32_t) * nints);
	afield[index].a_free = 0;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}

/*
 * The data is a 32-bit int. 
 * Assumes the address "val" will be valid when
 * dereferenced at the end of main.  This is true 
 * for addresses in the file header or record header.
 */
void
assign_I32(short index, __int32_t *val)
{
	afield[index].a_data = (char *)(val);
	afield[index].a_size = sizeof(__int32_t);
	afield[index].a_free = 0;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}

/*
 * Makes a copy of val, which must be freed after use.
 * This routine is used when the address of the data will
 * not be valid when dereferenced, or when the data type
 * is not an __int32_t so it can't be passed by address
 * to the routine above.
 */
void
assign_I32dup(short index, __int32_t val)
{
	__int32_t *intptr;
	intptr = malloc(sizeof(__int32_t));
	*intptr = val;
	afield[index].a_data = (char *)(intptr);
	afield[index].a_size = sizeof(val);
	afield[index].a_free = 1;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}
/*
 * The data is a 64-bit int.
 * Assumes the address "val" will be valid when
 * dereferenced at the end of main.  This is true 
 * for addresses in the file header or record header.
 */
void
assign_I64(short index, __int64_t *val)
{
	afield[index].a_data = (char *)(val);
	afield[index].a_size = sizeof(__int64_t);
	afield[index].a_free = 0;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}
/*
 * The data is a 64-bit int.
 * Makes a copy of val, which must be freed after use.
 */
void
assign_I64dup(short index, __int64_t val)
{
	__int64_t *intptr;
	intptr = malloc(sizeof(__int64_t));
	*intptr = val;
	afield[index].a_data = (char *)(intptr);
	afield[index].a_size = sizeof(__int64_t);
	afield[index].a_free = 1;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}

#ifdef MAYBE
void
assign_IA(short index, int *val, short nints)
{
	afield[index].a_data = (char *)(val);
	afield[index].a_size = (sizeof(int) * nints);
	afield[index].a_free = 0;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}

void
assign_IAdup(short index, int *val, short nints)
{
	afield[index].a_data = malloc(sizeof(int) * nints);
	memcpy((void *) afield[index].a_data, (void *) val, sizeof(int) * nints);
	afield[index].a_size = (sizeof(int) * nints); afield[index].a_free = 1;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += (2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad;
}

void
assign_I16(short index, short val)
{
	afield[index].a_data = (char *)&(val);
	afield[index].a_size = sizeof(val);
	afield[index].a_free = 0;
	afield[index].a_pad = ((afield[index].a_size * (ALIGN_REC - 1)) % ALIGN_REC);
	rlen += ((2 * sizeof(short)) + afield[index].a_size + afield[index].a_pad);
}
#endif

/*
 *  More detailed info for sys_call routine.
 */

char *
sat_sys_call(int major, int minor)
{
	static char buffer[256];

	/* let sys_call handle everything but ioctl() */
	if ((major+SYSVoffset) != SYS_ioctl)
		return sys_call(major,minor);

	/* Convert common ioctls to strings */
	switch (minor & 0xFFFF) {
	default:
                sprintf( buffer, "ioctl('%c'<<8|%d)",
                        (minor&0xff00)>>8, minor&0xff);
		break;
	case (FIOASYNC & 0xFFFF):
		strcpy(buffer,"ioctl(FIOASYNC)");
		break;
	case (FIOGETOWN & 0xFFFF):
		strcpy(buffer,"ioctl(FIOGETOWN)");
		break;
	case (FIONBIO & 0xFFFF):
		strcpy(buffer,"ioctl(FIONBIO)");
		break;
	case (FIONREAD & 0xFFFF):
		strcpy(buffer,"ioctl(FIONREAD)");
		break;
	case (FIOSETOWN & 0xFFFF):
		strcpy(buffer,"ioctl(FIOSETOWN)");
		break;
	case (I_STR & 0xFFFF):
		strcpy(buffer,"ioctl(I_STR)");
		break;
	case (SIOCADDMULTI & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCADDMULTI)");
		break;
	case (SIOCATMARK & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCATMARK)");
		break;
	case (SIOCDARP & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCDARP)");
		break;
	case (SIOCDELMULTI & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCDELMULTI)");
		break;
	case (SIOCGARP & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGARP)");
		break;
	case (SIOCGENADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGENADDR)");
		break;
	case (SIOCGENPSTATS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGENPSTATS)");
		break;
	case (SIOCGETACL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETACL)");
		break;
	case (SIOCGETLABEL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETLABEL)");
		break;
	case (SIOCGETNAME & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETNAME)");
		break;
	case (SIOCGETPEER & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETPEER)");
		break;
	case (SIOCGETRCVUID & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETRCVUID)");
		break;
	case (SIOCGETSYNC & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETSYNC)");
		break;
	case (SIOCGETUID & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGETUID)");
		break;
	case (SIOCGHIWAT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGHIWAT)");
		break;
	case (SIOCGIFADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFADDR)");
		break;
	case (SIOCGIFBRDADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFBRDADDR)");
		break;
	case (SIOCGIFCONF & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFCONF)");
		break;
	case (SIOCGIFDSTADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFDSTADDR)");
		break;
	case (SIOCGIFFLAGS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFFLAGS)");
		break;
	case (SIOCGIFLABEL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFLABEL)");
		break;
	case (SIOCGIFMEM & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFMEM)");
		break;
	case (SIOCGIFMETRIC & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFMETRIC)");
		break;
	case (SIOCGIFMTU & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFMTU)");
		break;
	case (SIOCGIFNETMASK & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFNETMASK)");
		break;
	case (SIOCGIFSTATS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFSTATS)");
		break;
	case (SIOCGIFUID & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGIFUID)");
		break;
	case (SIOCGLOWAT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGLOWAT)");
		break;
	case (SIOCGPGRP & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCGPGRP)");
		break;
	case (SIOCIFDETACH & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCIFDETACH)");
		break;
	case (SIOCLOWER & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCLOWER)");
		break;
	case (SIOCNREAD & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCNREAD)");
		break;
	case (SIOCPROTO & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCPROTO)");
		break;
	case (SIOCSARP & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSARP)");
		break;
	case (SIOCSETACL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSETACL)");
		break;
	case (SIOCSETLABEL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSETLABEL)");
		break;
	case (SIOCSETSYNC & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSETSYNC)");
		break;
	case (SIOCSETUID & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSETUID)");
		break;
	case (SIOCSHIWAT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSHIWAT)");
		break;
	case (SIOCSIFADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFADDR)");
		break;
	case (SIOCSIFBRDADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFBRDADDR)");
		break;
	case (SIOCSIFDSTADDR & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFDSTADDR)");
		break;
	case (SIOCSIFFLAGS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFFLAGS)");
		break;
	case (SIOCSIFHEAD & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFHEAD)");
		break;
	case (SIOCSIFLABEL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFLABEL)");
		break;
	case (SIOCSIFMEM & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFMEM)");
		break;
	case (SIOCSIFMETRIC & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFMETRIC)");
		break;
	case (SIOCSIFMTU & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFMTU)");
		break;
	case (SIOCSIFNAME & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFNAME)");
		break;
	case (SIOCSIFNETMASK & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFNETMASK)");
		break;
	case (SIOCSIFSTATS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFSTATS)");
		break;
	case (SIOCSIFUID & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSIFUID)");
		break;
	case (SIOCSLGETREQ & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSLGETREQ)");
		break;
	case (SIOCSLOWAT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSLOWAT)");
		break;
	case (SIOCSLSTAT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSLSTAT)");
		break;
	case (SIOCSOCKSYS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSOCKSYS)");
		break;
	case (SIOCSPGRP & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSPGRP)");
		break;
	case (SIOCSPROMISC & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSPROMISC)");
		break;
	case (SIOCSSDSTATS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSSDSTATS)");
		break;
	case (SIOCSSESTATS & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCSSESTATS)");
		break;
	case (SIOCTOSTREAM & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCTOSTREAM)");
		break;
	case (SIOCUPPER & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCUPPER)");
		break;
	case (SIOCX25RCV & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCX25RCV)");
		break;
	case (SIOCX25TBL & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCX25TBL)");
		break;
	case (SIOCX25XMT & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCX25XMT)");
		break;
	case (SIOCXPROTO & 0xFFFF):
		strcpy(buffer,"ioctl(SIOCXPROTO)");
		break;
	case (TCBLKMD & 0xFFFF):
		strcpy(buffer,"ioctl(TCBLKMD)");
		break;
	case (TCDSET & 0xFFFF):
		strcpy(buffer,"ioctl(TCDSET)");
		break;
	case (TCFLSH & 0xFFFF):
		strcpy(buffer,"ioctl(TCFLSH)");
		break;
	case (TCGETA & 0xFFFF):
		strcpy(buffer,"ioctl(TCGETA)");
		break;
	case (TCGETS & 0xFFFF):
		strcpy(buffer,"ioctl(TCGETS)");
		break;
	case (TCIOFF & 0xFFFF):
		strcpy(buffer,"ioctl(TCIOFF)");
		break;
	case (TCION & 0xFFFF):
		strcpy(buffer,"ioctl(TCION)");
		break;
	case (TCOOFF & 0xFFFF):
		strcpy(buffer,"ioctl(TCOOFF)");
		break;
	case (TCOON & 0xFFFF):
		strcpy(buffer,"ioctl(TCOON)");
		break;
	case (TCSADRAIN & 0xFFFF):
		strcpy(buffer,"ioctl(TCSADRAIN)");
		break;
	case (TCSAFLUSH & 0xFFFF):
		strcpy(buffer,"ioctl(TCSAFLUSH)");
		break;
	case (TCSANOW & 0xFFFF):
		strcpy(buffer,"ioctl(TCSANOW)");
		break;
	case (TCSBRK & 0xFFFF):
		strcpy(buffer,"ioctl(TCSBRK)");
		break;
	case (TCSETA & 0xFFFF):
		strcpy(buffer,"ioctl(TCSETA)");
		break;
	case (TCSETAF & 0xFFFF):
		strcpy(buffer,"ioctl(TCSETAF)");
		break;
	case (TCSETAW & 0xFFFF):
		strcpy(buffer,"ioctl(TCSETAW)");
		break;
	case (TCSETLABEL & 0xFFFF):
		strcpy(buffer,"ioctl(TCSETLABEL)");
		break;
	case (TCXONC & 0xFFFF):
		strcpy(buffer,"ioctl(TCXONC)");
		break;
	case (TFIOC & 0xFFFF):
		strcpy(buffer,"ioctl(TFIOC)");
		break;
	case (TIOCCBRK & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCCBRK)");
		break;
	case (TIOCCDTR & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCCDTR)");
		break;
	case (TIOCEXCL & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCEXCL)");
		break;
	case (TIOCFLUSH & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCFLUSH)");
		break;
	case (TIOCGETC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGETC)");
		break;
	case (TIOCGETD & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGETD)");
		break;
	case (TIOCGETP & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGETP)");
		break;
	case (TIOCGLTC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGLTC)");
		break;
	case (TIOCGPGRP & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGPGRP)");
		break;
	case (TIOCGSID & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGSID)");
		break;
	case (TIOCGWINSZ & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCGWINSZ)");
		break;
	case (TIOCHPCL & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCHPCL)");
		break;
	case (TIOCLBIC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCLBIC)");
		break;
	case (TIOCLBIS & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCLBIS)");
		break;
	case (TIOCLGET & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCLGET)");
		break;
	case (TIOCLSET & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCLSET)");
		break;
	case (TIOCMBIC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCMBIC)");
		break;
	case (TIOCMBIS & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCMBIS)");
		break;
	case (TIOCMGET & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCMGET)");
		break;
	case (TIOCMSET & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCMSET)");
		break;
	case (TIOCNXCL & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCNXCL)");
		break;
	case (TIOCOUTQ & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCOUTQ)");
		break;
	case (TIOCPKT & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCPKT)");
		break;
	case (TIOCREMOTE & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCREMOTE)");
		break;
	case (TIOCSBRK & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSBRK)");
		break;
	case (TIOCSDTR & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSDTR)");
		break;
	case (TIOCSETC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSETC)");
		break;
	case (TIOCSETD & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSETD)");
		break;
	case (TIOCSETN & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSETN)");
		break;
	case (TIOCSETP & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSETP)");
		break;
	case (TIOCSIGNAL & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSIGNAL)");
		break;
	case (TIOCSLTC & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSLTC)");
		break;
	case (TIOCSPGRP & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSPGRP)");
		break;
	case (TIOCSSID & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSSID)");
		break;
	case (TIOCSTART & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSTART)");
		break;
	case (TIOCSTI & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSTI)");
		break;
	case (TIOCSTOP & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSTOP)");
		break;
	case (TIOCSWINSZ & 0xFFFF):
		strcpy(buffer,"ioctl(TIOCSWINSZ)");
		break;
	}
	return buffer;
}

/*
 *  outcome (success or failure, and the reason why or why not) is displayed
 */
char *
sat_str_outcome (struct sat_hdr_info *h) {
	static	char	outstring [72];
	int outcome = h->sat_outcome;
	cap_value_t sat_cap = h->sat_cap;

	switch (brief) {
	case OM_BRIEF:
	case OM_VERBOSE:
		if (SAT_SUCCESS & outcome)
			strcpy (outstring, "Success");
		else
			strcpy (outstring, "Failure");

		if (SAT_CHK & outcome) {
			strcat (outstring, "/privilege=");
			if (SAT_CAPABILITY & outcome)
				strcat (outstring, "capability");
			else {
				if (SAT_SUSER & outcome)
					strcat (outstring, "superuser");
				else
					strcat (outstring, "none");
			}
		}

		if (cap_enabled && sat_cap != 0) {
			strcat (outstring, "/capability=");
			strcat (outstring, cap_value_to_text (sat_cap));
		}

		if (SAT_DAC & outcome)
			strcat (outstring, " (DAC checked)");

		if (SAT_MAC & outcome)
			strcat (outstring, " (MAC checked)");
		break;
	case OM_LINEAR:
	case OM_SGIF:
		if (SAT_SUCCESS & outcome)
			strcpy (outstring, "+");
		else
			strcpy (outstring, "-");

		if (SAT_CHK & outcome) {
			strcat (outstring, "/p=");
			if (SAT_CAPABILITY & outcome)
				strcat (outstring, "c");
			else {
				if (SAT_SUSER & outcome)
					strcat (outstring, "s");
				else
					strcat (outstring, "n");
			}
		}

		if (cap_enabled && sat_cap != 0) {
			strcat (outstring, "/c=");
			strcat (outstring, cap_value_to_text (sat_cap));
		}

		if (SAT_DAC & outcome)
			strcat (outstring, " DAC");

		if (SAT_MAC & outcome)
			strcat (outstring, " MAC");
		break;
	}
	if (brief == OM_SGIF)
		assign_strdup(OUTSTRING, outstring, strlen(outstring));
	return (outstring);
}


/*
 *  sat_debug_magic - display header in hex if misaligned - a debugging aid
 */
void
sat_debug_magic (char *h)
{
	int     l;		/* indexes bytes of header		*/
	int     n;		/* indexes bytes of hex header buffer 	*/
	int     nibl;		/* nibble of a header byte		*/
	char    hhb[72];	/* hex header buffer			*/

	printf("INVALID MAGIC NUMBER!!\n");

	for (n=0,l=0; l < sizeof(struct sat_hdr) && n < sizeof(hhb); l++) {
		if ((!(l%4))&&l)
			hhb[n++] = ' ';	/* insert blanks after each word */
		nibl = 0xf & (h[l] >> 4);
		hhb[n++] = (0xa > nibl) ? '0' + nibl : 'a' + nibl - 0xa;
		nibl = 0xf &  h[l];
		hhb[n++] = (0xa > nibl) ? '0' + nibl : 'a' + nibl - 0xa;
	}
	hhb[n] = '\0';
	printf ("Header           = %s\n",  hhb );
}

/*
 * descriptor/name mapping.
 */
struct fdnamemap {
	struct fdnamemap	*fn_next;
	int			fn_pid;		/* Process ID */
	int			fn_fd;		/* File Descriptor */
	char			*fn_path;	/* Associated Object Name */
};

struct fdnamemap *fdnamemap;

void
sat_set_fdmap_path(int pid, int fd, char *path)
{
	struct fdnamemap *fp;
	struct fdnamemap *unused = NULL;

	if (!fdmap)
		return;

	for (fp = fdnamemap; fp; fp = fp->fn_next) {
		/*
		 * Remember the first unused entry for later.
		 */
		if (unused == NULL && fp->fn_pid == -1)
			unused = fp;

		if (fp->fn_pid != pid)
			continue;
		if (fd != -1 && fp->fn_fd != fd)
			continue;

		if (fp->fn_path) {
			free(fp->fn_path);
			fp->fn_path = NULL;
		}

		/*
		 * If a path is given set the entry, otherwise mark it
		 * as available for other use.
		 */
		if (path)
			fp->fn_path = strdup(path);
		else
			fp->fn_pid = -1;
		return;
	}
	/*
	 * Don't add entries for the clear cases.
	 */
	if (fd == -1 || path == NULL)
		return;
	/*
	 * Mapping not found. Add to the list, using existing list
	 * entry if available.
	 */
	if (unused) {
		fp = unused;
	}
	else {
		fp = (struct fdnamemap *)malloc(sizeof(struct fdnamemap));
		fp->fn_next = fdnamemap;
		fdnamemap = fp;
	}
	fp->fn_pid = pid;
	fp->fn_fd = fd;
	fp->fn_path = strdup(path);
}

/*
 * If path is NULL clear the entry.
 * If fd is -1 set (clear) all entries for this pid.
 */
void
sat_set_fdmap(int pid, int fd, char *body)
{
	mac_t label;
	char *reqname = NULL;
	char *actname;
	char *buf = body;
	struct sat_pathname pn;

	if (body && sat_intrp_pathname(&buf, &pn, &reqname, &actname, &label,
		file_major, file_minor) < 0) {
		exit(1);
	}
	sat_set_fdmap_path(pid, fd, reqname);
}

char *
sat_get_fdmap(int pid, int fd)
{
	static char result[MAXPATHLEN];
	struct fdnamemap *fp;

	if (!fdmap)
		return (NULL);

	for (fp = fdnamemap; fp; fp = fp->fn_next) {
		if (fp->fn_pid != pid || fp->fn_fd != fd)
			continue;
		if (fp->fn_path == NULL)
			break;
		sprintf(result, "%s", fp->fn_path);
		return (result);
	}
	return (NULL);
}

void
sat_copy_fdmap(int pid, int newpid)
{
	struct fdnamemap *fp;

	if (!fdmap)
		return;

	for (fp = fdnamemap; fp; fp = fp->fn_next)
		if (fp->fn_pid == pid)
			sat_set_fdmap_path(newpid, fp->fn_fd, fp->fn_path);
}

/*
 * sat_normalize_pid keeps a list even if normalization is not being done.
 */
int
sat_normalize_pid(int pid)
{
	static int normalizer;
	static struct pids {
		struct pids	*next;
		int		pid;
		int		npid;
	} *pidlist;
	struct pids *pp;

	/*
	 * Look up the real pid in the list and return the normalized pid.
	 */
	for (pp = pidlist; pp; pp = pp->next)
		if (pp->pid == pid)
			return (pp->npid);
	/*
	 * Create a new entry in the normalized pid list.
	 */
	pp = (struct pids *)malloc(sizeof(struct pids));
	pp->pid = pid;

	if (normalize)
		pp->npid = ++normalizer;
	else
		pp->npid = pid;

	pp->next = pidlist;
	pidlist = pp;

	/*
	 * While we're here, add mappings for std{in,out,err}
	 */
	sat_set_fdmap_path(pid, 0, "<stdin>");
	sat_set_fdmap_path(pid, 1, "<stdout>");
	sat_set_fdmap_path(pid, 2, "<stderr>");

	return (pp->npid);
}

void
sat_print_pid(int pid)
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("Process ID       = %d\n", sat_normalize_pid(pid));
		break;
	case OM_LINEAR:
		printf("pid:%d ", sat_normalize_pid(pid));
		break;
	case OM_SGIF:
		assign_I32dup(OBJPID, sat_normalize_pid(pid));
		break;
	}
}

/*
 *  headers of audit records are displayed readably or normalized.
 */
void
sat_print_header(struct sat_hdr_info *h)
{
	char time_b[36];	/* time of day string buffer */
	char csec_b[3];		/* centiseconds buffer       */
	char *eventname;
	char *labelname;
	char *capname;
	char *cp;
	int i, len;

	cftime (time_b, "%a %b %e %T.?? %Z %Y", &(h->sat_time));
	sprintf (csec_b, "%02d", h->sat_ticks);
        time_b[20]=csec_b[0]; time_b[21]=csec_b[1];     /* put csecs in str  */

	/*
	 * print it
	 */
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf ("\n.....................................................\n\n");
		break;
	case OM_LINEAR:
	case OM_SGIF:
		break;
	}

	if (debug) {
		printf ("Record offset    = %.0f\n", fileoffset);
		printf ("Header size      = %d\n", h->sat_hdrsize);
		printf ("Record size      = %d\n", h->sat_recsize);
		printf ("Magic number     = %#x\n", h->sat_magic);
	}
	if (h->sat_magic != 0x12345678)
		sat_debug_magic ((char *)h);

	/*
	 * (contrived) example of a brief header:
	 *
	 * Fri Aug 30 16:18:29.44 PDT 1991  orange.wpd.sgi.com
	 * -sat_pipe (pipe), Failure (No permission match)
	 * Process 37 (syslogd), ppid 1, tty NODEV, cwd /
	 * SAT ID root, uid root, gid sys, label dbadmin, groups=adm,daemon
	 */
	switch (brief) {
	case OM_BRIEF:
	{
		int cwd_len = (h->sat_cwd)? strlen(h->sat_cwd): 0;
		int root_len = (h->sat_rootdir)? strlen(h->sat_rootdir): 0;

		/* line 1 */
		printf("%s", time_b);
		if (n_hosts > 1 || h->sat_host_id != 0) {
			printf("  %s", sat_str_host(h->sat_host_id));
		}
		putchar('\n');

		/* line 2 */
		if (eventname = sat_eventtostr( h->sat_rectype ))
			printf ("%c%s ", (h->sat_outcome)? '+': '-',
			    eventname);
		else
			printf ("* invalid event #%d, ", h->sat_rectype );
		if (h->sat_syscall == SAT_SYSCALL_KERNEL) {
			printf("(direct kernel action), %s\n",
			    sat_str_outcome (h));
			return;
		}
		printf("(%s), %s", 
		    sat_sys_call (h->sat_syscall, h->sat_subsyscall),
		    sat_str_outcome (h));
		if (h->sat_errno)
			printf (" (%s)", strerror (h->sat_errno));
		putchar('\n');

		/* line 3 */
		len = printf("Process %d (%s), ppid %d, tty %s",
		    sat_normalize_pid(h->sat_pid),
		    (h->sat_pname)? h->sat_pname: "no_name",
		    sat_normalize_pid(h->sat_ppid), sat_str_dev(h->sat_tty));

		/* put root/cwd on the same line if it'll fit */
		if ((len + root_len + cwd_len) < 70)
			printf(", ");
		else
			putchar('\n');
		if (root_len)
			printf ("root %s", h->sat_rootdir);
		if (root_len && cwd_len)
			printf(", ");
		if (cwd_len)
			printf ("cwd %s", h->sat_cwd);
		putchar('\n');

		/* line 4 */
		printf("SAT ID %s, uid %s", sat_str_user(h->sat_id),
		    sat_str_user(h->sat_ruid));
		if (h->sat_euid != h->sat_ruid)
			printf(" (euid %s)", sat_str_user(h->sat_euid));
		printf(", gid %s", sat_str_group(h->sat_rgid));
		if (h->sat_egid != h->sat_rgid)
			printf(" (egid %s)", sat_str_group(h->sat_egid));
		if (mac_enabled) {
			labelname = mac_to_text(h->sat_plabel, (size_t *) NULL);
			if (labelname) {
				printf(", label %s", labelname);
				free(labelname);
			} else
				printf(", label <invalid>");
		}
		if (cap_enabled) {
			capname = cap_to_text(h->sat_pcap, (size_t *) NULL);

			if (capname) {
				printf(", cap %s", capname);
				cap_free(capname);
			}
			else
				printf(", cap <invalid>");
		}
		for (i=0; i < h->sat_ngroups; i++) {
			if (i == 0)
				printf(", groups=");
			else
				putchar(',');
			printf ("%s", sat_str_group(h->sat_groups[i]));
		}
		putchar('\n');
		break;
	}
	case OM_VERBOSE:
		eventname = sat_eventtostr( h->sat_rectype );
		if (eventname)
			printf ("Event type       = %s\n", eventname);
		else
			printf ("Event type       = invalid event #%d\n",
				h->sat_rectype );
		printf ("Outcome          = %s\n",
			sat_str_outcome (h));
		printf ("Sequence number  = %d\n", h->sat_sequence);
		printf ("Time of event    = %s\n", time_b);
		printf ("System call      = %s\n",
			sat_sys_call (h->sat_syscall, h->sat_subsyscall));
		if (h->sat_syscall == SAT_SYSCALL_KERNEL)
			return;

		if (h->sat_errno)
			printf ("Error status     = %d (%s)\n", h->sat_errno,
				strerror (h->sat_errno));
		else
			printf ("Error status     = 0 (No error)\n");

		if (n_hosts > 1 || h->sat_host_id != 0) {
			printf ("Hostname         = %s\n",
				sat_str_host(h->sat_host_id));
		}
		if (h->sat_pname)
			printf ("Process ID       = %d (%s)\n",
			    sat_normalize_pid(h->sat_pid), h->sat_pname);
		else
			printf ("Process ID       = %d\n",
			    sat_normalize_pid(h->sat_pid));
		printf ("Parent process   = %d\n",
		    sat_normalize_pid(h->sat_ppid));

		if (h->sat_rootdir)
			printf ("Curr root dir.   = %s\n", h->sat_rootdir);
		if (h->sat_cwd)
			printf ("Curr working dir = %s\n", h->sat_cwd);
		if (mac_enabled)
			printf ("Process label    = %s\n",
				sat_str_label(h->sat_plabel));
		if (cap_enabled)
			printf ("Capability Set   = %s\n",
				sat_str_cap(h->sat_pcap));

		printf ("SAT ID           = %s\n", sat_str_user(h->sat_id));
		if (h->sat_euid != h->sat_ruid) {
			printf ("Effective uid    = %s\n",
				sat_str_user(h->sat_euid));
		}
		printf ("User id          = %s\n", sat_str_user(h->sat_ruid));

		if (h->sat_egid != h->sat_rgid) {
			printf ("Effective gid    = %s\n",
				sat_str_group(h->sat_egid));
		}
		printf ("Group id         = %s\n", sat_str_group(h->sat_rgid));
		for (i=0; i < h->sat_ngroups; i++) {
			printf ("Group list ent.  = %s\n",
				sat_str_group(h->sat_groups[i]));
		}

		if (h->sat_tty) {
			printf ("Terminal dev.    = %s\n",
				sat_str_dev(h->sat_tty));
		}
		break;
	case OM_LINEAR:
		/*
		 * Fit just the most important stuff on one line.
		 */
		if (eventname = sat_eventtostr( h->sat_rectype ))
			printf ("%s ", eventname);
		else
			printf ("#%d ", h->sat_rectype );
		if (h->sat_syscall == SAT_SYSCALL_KERNEL) {
			printf("KERNEL %s ",
			    sat_str_outcome (h));
			return;
		}
		/*
		 * Because sys_call(3) returns "syscall #%d\n"
		 */
		cp = sat_sys_call(h->sat_syscall, h->sat_subsyscall);
		{
			char *nl;
			if (nl = strchr(cp, '\n'))
				*nl = '\0';
		}
		printf("(%s) %s (%s) ", cp, sat_str_outcome (h),
		    h->sat_errno ? strerror(h->sat_errno) : "ok");

		if (n_hosts > 1 || h->sat_host_id != 0) {
			printf("%s ", sat_str_host(h->sat_host_id));
		}
		sat_print_pid(h->sat_pid);
		printf("%s ", (h->sat_pname)? h->sat_pname: "no_name");

		printf("sreuid:%s,%s,%s ", sat_str_user(h->sat_id),
		    sat_str_user(h->sat_ruid), sat_str_user(h->sat_euid));
		printf("regid:%s,%s", sat_str_group(h->sat_rgid),
		    sat_str_group(h->sat_egid));
		for (i = 0; i < h->sat_ngroups; i++) {
			printf (",%s", sat_str_group(h->sat_groups[i]));
		}
		if (mac_enabled) {
			labelname = mac_to_text(h->sat_plabel, (size_t *) NULL);
			if (labelname) {
				printf(" label:%s", labelname);
				free(labelname);
			} else
				printf(" label:<invalid>");
		}
		if (cap_enabled) {
			capname = cap_to_text(h->sat_pcap, (size_t *) NULL);

			if (capname) {
				printf(" cap:%s", capname);
				cap_free(capname);
			}
			else
				printf(" cap:<invalid>");
		}
		printf(" ");
		break;
        case OM_SGIF: {
		rlen = 0;
		assign_I32(MAGIC, &(h->sat_magic));
		assign_I32(RECTYPE, &(h->sat_rectype));
		/*
		 *  sat_str_outcome() assigns OUTSTRING
		 */
		sat_str_outcome (h); 
		assign_I64(CAP, (__int64_t *)&(h->sat_cap));
		assign_I32(SEQUENCE,  &(h->sat_sequence));
		assign_I32(ERRNO,  &(h->sat_errno));
		assign_I32dup(TIME,  h->sat_time);
 		assign_I32(TICKS,  &(h->sat_ticks));
		assign_I32(SYSCALL,  &(h->sat_syscall));
		assign_I32(SUBSYSCALL,  &(h->sat_subsyscall));
		assign_I32dup(SATID,  h->sat_id);
		if (h->sat_host_id)
			assign_I32dup(HOST_ID,  h->sat_host_id);
		if (h->sat_tty && h->sat_tty != NODEV)
			assign_I32dup(TTY, (__int32_t)h->sat_tty); /* dev_t */
		assign_I32dup(PPID, h->sat_ppid);		       
		assign_I32dup(PID, h->sat_pid);

		if (mac_enabled) {
			assign_maclbl(PLABEL, h->sat_plabel, mac_size(h->sat_plabel));
		}
		if (cap_enabled) {
			assign_cap(PCAP, h->sat_pcap, cap_size(h->sat_pcap));
		}
		assign_I32dup(EUID,  h->sat_euid);	
		assign_I32dup(RUID,  h->sat_ruid);	
		assign_I32dup(EGID,  h->sat_egid);	
		assign_I32dup(RGID,  h->sat_rgid);	
		assign_I32(NGROUP,  &(h->sat_ngroups));	
  		assign_I32A(GROUPS,  (__int32_t *)h->sat_groups, h->sat_ngroups);
		if (h->sat_rootdir)
			assign_str(ROOTDIR, h->sat_rootdir, strlen(h->sat_rootdir));
		if (h->sat_pname)
			assign_str(PNAME, h->sat_pname, strlen(h->sat_pname));
		if (h->sat_cwd)
			assign_str(CWD, h->sat_cwd, strlen(h->sat_cwd));
		break;
	}
	}
}


/*
 * print user events with proper spacing
 */
void
sat_print_ae( int ae, const char *message )
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		switch (ae) {
		case SAT_AE_AUDIT:
			printf("Audit adm event  =");
			break;
		case SAT_AE_IDENTITY:
			printf("Identity event   =");
			break;
		case SAT_AE_CUSTOM:
			printf("Custom event     =");
			break;
		case SAT_AE_MOUNT:
			printf("Mount event      =");
			break;
		case SAT_AE_DBEDIT:
			printf("DB edit event    =");
			break;
		case SAT_AE_LP:
			printf("LP event         =");
			break;
		case SAT_AE_X_ALLOWED:
			printf("X access allowed =");
			break;
		case SAT_AE_X_DENIED:
			printf("X access denied  =");
			break;
		}
		/*
		 * Width of description is constant (18).
		 */
		if (strlen(message) < 52)
			printf(" %s\n", message);
		else
			printf("\n\t%s\n", message);
		break;
	case OM_LINEAR:
		switch (ae) {
		case SAT_AE_AUDIT:
			printf("ae_audit:\"%s\" ", message);
			break;
		case SAT_AE_IDENTITY:
			printf("ae_identity:\"%s\" ", message);
			break;
		case SAT_AE_CUSTOM:
			printf("ae_custom:\"%s\" ", message);
			break;
		case SAT_AE_MOUNT:
			printf("ae_mount:\"%s\" ", message);
			break;
		case SAT_AE_DBEDIT:
			printf("ae_dbedit:\"%s\" ", message);
			break;
		case SAT_AE_LP:
			printf("ae_lp:\"%s\" ", message);
			break;
		case SAT_AE_X_ALLOWED:
			printf("ae_x_allowed:\"%s\" ", message);
			break;
		case SAT_AE_X_DENIED:
			printf("ae_x_denied:\"%s\" ", message);
			break;
		}
		break;
	case OM_SGIF:
		assign_strdup(STRING, (char *)message, strlen(message));
		break;
	}
}


/*
 *  communications domains are translated from binary to ascii
 */
char *
sat_str_domain (short doma)
{
#define	SAT_NUM_DOMA	19
	static struct doma_names {
		char *  name;
	} doma_name [] = {
		"unspecified",			/* 00 */
		"unix same host",		/* 01 */
		"internet",			/* 02 */
		"IMP link",			/* 03 */
		"PUP",				/* 04 */
		"MIT CHAOS",			/* 05 */
		"XEROX NS",			/* 06 */
		"NBS",				/* 07 */
		"ECMA",				/* 08 */
		"ATT datakit",			/* 09 */
		"CCITT wide area",		/* 10 */
		"IBM SNA",			/* 11 */
		"DEC net",			/* 12 */
		"direct link interface",	/* 13 */
		"LAT",				/* 14 */
		"NSC hyperchannel",		/* 15 */
		"Apple Talk",			/* 16 */
		"CS net serial",		/* 17 */
		"raw",				/* 18 */
		"eXpress Transfer Protocol",	/* 19 */
	};

	static  char    doma_name_str[72];

	if ((SAT_NUM_DOMA <= doma) || (0 > doma))
		sprintf (doma_name_str, "unknown");
	else
		strcpy  (doma_name_str, doma_name[doma].name);
	return (doma_name_str);
}


/*
 *  network socket types are translated from binary to ascii
 */
char *
sat_str_socktype (short styp)
{
#define	SAT_NUM_STYP	5
	static struct styp_names {
		char *  name;
	} styp_name [] = {
		"unspecified",
		"stream",
		"datagram",
		"raw",
		"reliably-delivered",
		"sequenced-packet",
	};

	static  char    styp_name_str[72];

	if ((SAT_NUM_STYP <= styp) || (0 > styp))
		sprintf (styp_name_str, "unknown");
	else
		strcpy  (styp_name_str, styp_name[styp].name);
	return (styp_name_str);
}


/*
 *  socket addresses are displayed readably
 */
void
sat_print_sockaddr (short lnth, char *addr)
{
	int	fam;		/* address family			     */
	int	port;		/* TCP/UDP port for inet sockets	     */
	struct servent *serv;	/* service associated with port number	     */
	struct hostent *host;	/* host at named internet address	     */

	int	l;		/* indexes bytes of address string	     */
	char	rab[2*MAXPATHLEN+257];/* readable address buffer	     */
	int	nibl;		/* nibble of an address byte		     */
	int     n = 0;		/* indexes bytes of hex header buffer 	     */
	struct sockaddr_in * sin;

	sin = (struct sockaddr_in *)addr;
	fam = sin->sin_family;

	switch (fam) {

	case AF_UNIX:
#ifdef KLUGE
af_unix:
#endif /* KLUGE */
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Un*x rendezvous  = %s\n",  
				(addr[2] ? addr + 2 : "(null)"));
			break;
		case OM_LINEAR:
			printf ("UDS:\"%s\" ", (addr[2] ? addr + 2 : ""));
			break;
		case OM_SGIF:
			if (addr[2])
			assign_strdup(RENDEZVOUS, addr + 2, strlen(addr + 2));
			break;
		}
		break;

	case AF_INET:
#ifdef KLUGE
af_inet:
#endif /* KLUGE */
		port = sin->sin_port;
		serv = getservbyport (port, NULL);
		host = gethostbyaddr (&sin->sin_addr, sizeof(sin->sin_addr),
		    AF_INET);

		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			if (serv)
				printf ("TCP/UDP port     = %d (%s)\n",
				    port, serv->s_name);
			else
				printf ("TCP/UDP port     = %d\n", port);

			if (host)
				printf ("Host Name        = %s (%s)\n",
				    host->h_name, inet_ntoa(sin->sin_addr));
			else
				printf ("IP Address       = %s\n", 
				    inet_ntoa(sin->sin_addr));
			break;
		case OM_LINEAR:
			if (serv)
				printf ("port:%d(%s) ", port, serv->s_name);
			else
				printf ("port:%d ", port);

			if (host)
				printf ("hostname:%s(%s) ",
				    host->h_name, inet_ntoa(sin->sin_addr));
			else
				printf ("IPaddr:%s ", 
				    inet_ntoa(sin->sin_addr));
			break;
		case OM_SGIF:
			if (serv)
				assign_strdup(SERV, serv->s_name, strlen(serv->s_name));
			assign_I32dup(PORT, port);
			if (host)
				assign_strdup(HOST, host->h_name, strlen(host->h_name));
			assign_strdup(ADDR, inet_ntoa(sin->sin_addr), strlen(inet_ntoa(sin->sin_addr)));
			break;		
		}
		break;

	default:
#ifdef KLUGE
		/* the Kernel doesn't check for legal values of sin_family */
		if (lnth >= sizeof(struct sockaddr_in)) { /* KLUGE */
			if (lnth == sizeof(struct sockaddr_in)) 
				goto af_inet;
			else
				goto af_unix;
		}
#endif
		for (l=0; (l<lnth) && (l<MAXPATHLEN); l++) {
			if ((!(l%4))&&l) /* insert blanks after each word    */
				rab[n++] = ' ';
			nibl = 0xf & (addr[l] >> 4);
			rab[n++] = (0xa>nibl) ? '0' + nibl : 'a' + nibl - 0xa;
			nibl = 0xf &  addr[l];
			rab[n++] = (0xa>nibl) ? '0' + nibl : 'a' + nibl - 0xa;
			}
		rab[n] = '\0';
#ifdef NOTDEF
		for (l=0; (l<lnth) && (l<128); l++)
			for (n=0; n<2; n++) {
				nibl = 0xf & (n ? addr[l] : (addr[l] >> 4));
				if (0xa > nibl)
					rab[2*l+n] = '0' + nibl;
				else
					rab[2*l+n] = 'a' + nibl - 0xa;
			}
		rab[2*l+2] = '\0';
#endif
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Address len      = %d\n",  lnth);
			printf ("Socket addr      = %s\n",  rab );
			break;
		case OM_LINEAR:
			printf ("addrlen,socaddr:%d,%s ", lnth, rab);
			break;
		case OM_SGIF:
			assign_I32dup(ADDRLNTH, lnth);
			assign_strdup(SOCADDR, rab, strlen(rab));
			break;
		}
		break;
	}
}


/*
 *  Pathname modules are displayed readably.  Return the size.
 */
int
sat_print_pathname (char *message, char *line_message, char *buffer, short field)
{
	mac_t label;
	char *labelname;
	char *reqname;
	char *actname;
	char *buf = buffer;
	struct sat_pathname pn;

	if (sat_intrp_pathname(&buf, &pn, &reqname, &actname, &label,
		file_major, file_minor) < 0) {
		exit(1);
	}

	if (! mac_enabled && label) {
		free(label);
		label = NULL;
	}

	switch (brief) {
	case OM_BRIEF:
	case OM_VERBOSE:
		if (message)
			printf ("%s:\n", message);
		else
			printf ("Pathname information:\n");
		break;
	case OM_LINEAR:
		printf ("%s:", line_message ? line_message : "path");
		break;
	case OM_SGIF:
		break;
	}

	switch (brief) {
	case OM_BRIEF:
		/* pseudo-ls format: inode -mode- owner group name [label] */
		if (pn.sat_device != 0) {
			if (normalize)
				printf("  -     ");
			else
				printf("  %-lld ", pn.sat_inode);
			printf("%s  %-6s %-6s %s ",
			    sat_str_mode (pn.sat_filemode),
			    sat_str_user (pn.sat_fileown),
			    sat_str_group (pn.sat_filegrp),
			    reqname);
			if (mac_enabled) {
				labelname = mac_to_text(label, (size_t *) NULL);
				printf("[%s]\n",
				    (labelname) ? labelname: "<invalid>");
				if (labelname)
					mac_free(labelname);
			}
		} else
			printf( "  Requested name: %s\n", reqname );
		printf("  Actual name: %s\n", actname);
		break;
	case OM_VERBOSE:
		if (pn.sat_device != 0) {
			if (normalize)
				printf ("  Device         = (%s)\n",
				    sat_str_dev(pn.sat_device));
			else
				printf ("  Device/Inode   = (%s)/%lld\n",
				    sat_str_dev(pn.sat_device), pn.sat_inode);
			printf ("  Owner          = %s\n",
				sat_str_user (pn.sat_fileown));
			printf ("  Group          = %s\n",
				sat_str_group (pn.sat_filegrp));
			printf ("  Mode bits      = %#o (%s)\n",
				pn.sat_filemode,
				sat_str_mode (pn.sat_filemode));
			if (mac_enabled)
				printf ("  Label          = %s\n",
					sat_str_label (label));
		} else {
			printf ("  Device/Inode   = N/A\n");
			printf ("  Owner          = N/A\n");
			printf ("  Group          = N/A\n");
			printf ("  Mode bits      = N/A\n");
			if (mac_enabled)
				printf ("  Label          = N/A\n");
		}
		if (debug) {
			printf ("  Req. name len  = %d\n",  pn.sat_reqname_len);
			printf ("  Actual namelen = %d\n",  pn.sat_actname_len);
		}
		printf("  Requested name = \"%s\"\n", reqname);
		printf("  Actual name    = \"%s\"\n", actname);
		break;
	case OM_LINEAR:
		if (pn.sat_device != 0) {
			if (normalize)
				printf("-,");
			else
				printf("%lld,", pn.sat_inode);
			printf("%s,%s,%s,",
			    sat_str_mode (pn.sat_filemode),
			    sat_str_user (pn.sat_fileown),
			    sat_str_group (pn.sat_filegrp));
			if (mac_enabled) {
				labelname = mac_to_text(label, (size_t *) NULL);
				printf("%s,", (labelname) ? labelname: "-");
				if (labelname)
					mac_free(labelname);
			}
		} else {
			if (mac_enabled)
				printf("-,-,-,-,-,");
			else
				printf("-,-,-,-,");
		}
		printf("\"%s\",\"%s\" ", reqname, actname);
		break;
	case OM_SGIF:
	if (pn.sat_device != 0) {
		assign_I64dup(FILE0INODE + field,  pn.sat_inode);	 /* __int64_t */
		if (pn.sat_device != NODEV)
			assign_I32dup(FILE0DEVICE + field,
				(__int32_t)pn.sat_device); /* dev_t (unsigned int) */
		assign_I32dup(FILE0OWN + field, pn.sat_fileown); /* uid_t */
		assign_I32dup(FILE0GRP + field, pn.sat_filegrp); /* gid_t */
		assign_I32dup(FILE0MODE + field,
				(__int32_t)pn.sat_filemode); /* mode_t (unsigned int) */
	}	
	if (reqname)
		assign_strdup(FILE0REQNAME + field, reqname, (short)pn.sat_reqname_len);
	if (actname)
		assign_strdup(FILE0ACTNAME + field, actname, (short)pn.sat_actname_len);
		if (mac_enabled) {
			labelname = mac_to_text(label, (size_t *) NULL);
			if (labelname) {
				assign_strdup(FILE0LABEL, labelname, strlen(labelname));	
				mac_free(labelname);
			} else
				assign_str(FILE0LABEL, BADLABEL, strlen(BADLABEL));		
		}
		break;
	}

	free(reqname);
	free(actname);
	if (label) free(label);

	return ALIGN(buf - buffer);
}


/*
 * Display mode bits like ls -l does
 */
char *
sat_str_mode (unsigned mode )
{
	static char modestr[11];

	strcpy( modestr, "----------" );

	if (S_ISDIR(mode))
		modestr[0] = 'd';
	else if (S_ISCHR(mode))
		modestr[0] = 'c';
	else if (S_ISBLK(mode))
		modestr[0] = 'b';
	else if (S_ISREG(mode))
		modestr[0] = '-';
	else if (S_ISFIFO(mode))
		modestr[0] = 'p';
	else if (S_ISLNK(mode))
		modestr[0] = 'l';
	else if (S_ISSOCK(mode))
		modestr[0] = 's';
	else
		modestr[0] = ' ';

	if (mode & S_ISUID)
		modestr[3] = 'S';
	if (mode & S_ISGID)
		modestr[6] = 'l';
	if (mode & S_ISVTX)
		modestr[9] = 'T';

	if (mode & S_IRUSR)
		modestr[1] = 'r';
	if (mode & S_IWUSR)
		modestr[2] = 'w';
	if (mode & S_IXUSR)
		modestr[3] = 'x';
	if (mode & S_IRGRP)
		modestr[4] = 'r';
	if (mode & S_IWGRP)
		modestr[5] = 'w';
	if (mode & S_IXGRP)
		modestr[6] = 'x';
	if (mode & S_IROTH)
		modestr[7] = 'r';
	if (mode & S_IWOTH)
		modestr[8] = 'w';
	if (mode & S_IXOTH)
		modestr[9] = 'x';

	if ((mode & S_ISUID) && (mode & S_IXUSR))
		modestr[3] = 's';
	if ((mode & S_ISGID) && (mode & S_IXGRP))
		modestr[6] = 's';
	if ((mode & S_ISVTX) && (mode & S_IXOTH))
		modestr[9] = 't';

	return (modestr);
}

void
sat_print_new_mode_bits(int filemode)
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf ("New mode bits    = %#o (%s)\n",
		    filemode, sat_str_mode(filemode));
		break;
	case OM_LINEAR:
		printf ("newmode:%s ", sat_str_mode(filemode));
		break;
	}
}


/*
 * sat_list_ent decoder.  Given an id in the list of entries, return
 * the text field associated with that id.  Since the list is sorted
 * by id, use a binary search.
 */
const char *
sat_str_id(unsigned int id, struct sat_list_ent **list, int n_entries)
{
	static char mystery_id[80];
	static char *lastdata;
	static struct sat_list_ent **lastlist;
	static int lastid;
	int first, last, curr;

	if (id == (unsigned short)-1 || id == (unsigned)-1 || id == -1)
		return "N/A";

	if (id == lastid && list == lastlist)
		return lastdata;

	/* binary search */

	first = 0;			/* index of first entry */
	last = n_entries - 1;		/* index of last entry */

	while (last >= first) {
		curr = first + (last - first)/2;

		if (list[curr]->sat_id == id) {
			lastlist = list;
			lastid = id;
			lastdata = list[curr]->sat_name.data;
			return list[curr]->sat_name.data;
		}

		if (list[curr]->sat_id < id)
			first = curr + 1;
		else if (first == last)
			break;
		else
			last = curr;
	}

	/* it wasn't found */

	sprintf (mystery_id, "No name (#%d)", id);
	return mystery_id;
}


void
sat_print_new_file_owner(uid_t fileown)
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf ("New file owner   = %s\n", sat_str_user(fileown));
		break;
	case OM_LINEAR:
		printf ("newowner:%s ", sat_str_user(fileown));
		break;
	}
}


void
sat_print_new_file_group(gid_t filegrp)
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf ("New file group   = %s\n", sat_str_group(filegrp));
		break;
	case OM_LINEAR:
		printf ("newgroup:%s ", sat_str_user(filegrp));
		break;
	}
}

/*
 * Return the label name for this label.
 */
char *
sat_str_label (mac_t label)
{
	static char buf[MAX_REC_SIZE];
	size_t maclen;
	char *name;

	if (!mac_enabled) {
		return "mac_not_enabled";
	}

	if (label) {
		name = mac_to_text(label, &maclen);
		if (name == NULL)
			return "<BAD LABEL>";
		if (maclen > sizeof (buf) - 1) {
			mac_free(name);
			return "<MAC TOO BIG>";
		}
		strcpy(buf, name);
		mac_free(name);
	} else
		strcpy(buf, "(null)");

	return buf;
}


/*
 * Return the capability set for this capability
 */
char *
sat_str_cap (cap_t pcap)
{
	static char buf[MAX_REC_SIZE];
	size_t caplen;
	char *name;

	if (!cap_enabled) {
		return "cap_not_enabled";
	}

	if (pcap) {
		name = cap_to_text (pcap, &caplen);
		if (name == NULL)
			return "<BAD CAP>";
		if (caplen > sizeof (buf) - 1) {
			cap_free(name);
			return "<CAP TOO BIG>";
		}
		strcpy(buf, name);
		cap_free(name);
	} else
		strcpy(buf, "(null)");

	return buf;
}

char *
sat_str_acl (acl_t acl)
{
	static char buf[MAX_REC_SIZE];
	char *name;
	ssize_t acllen;
	
	if (acl) {
		name = acl_to_text (acl, &acllen);
		if (name == NULL)
			return "<BAD ACL>";
		if (acllen > sizeof (buf) - 1) {
			acl_free(name);
			return "<CAP TOO BIG>";
		}
		strcpy(buf, name);
		acl_free(name);
	}
	else
		strcpy (buf, "(null)");

	return buf;
}

char *
sat_str_umask (mode_t mode)
{
	static char buf[MAX_REC_SIZE];
	sprintf (buf, "%#lo", (unsigned long) mode);
	return (buf);
}

void
sat_print_new_file_label(mac_t lp)
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("New file label   = %s\n", sat_str_label(lp));
		break;
	case OM_LINEAR:
		printf("newlabel:%s ", sat_str_label(lp));
		break;
        case OM_SGIF: {
		char *tmpstr;
		tmpstr = mac_to_text(lp, (size_t *) NULL);
		if (tmpstr) {
			assign_strdup(FILE0LABEL, tmpstr, strlen(tmpstr));
			mac_free(tmpstr);
		} else
			assign_str(FILE0LABEL, BADLABEL, strlen(BADLABEL));
		break;
	}
	}
}

void
sat_print_new_file_cap(cap_t cp)
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("New file cap     = %s\n", sat_str_cap(cp));
		break;
	case OM_LINEAR:
		printf("newcap:%s ", sat_str_cap(cp));
		break;
        case OM_SGIF: {
		char *tmpstr;
		tmpstr = cap_to_text(cp, (size_t *) NULL);
		if (tmpstr) {
			assign_strdup(FILE0CAP, tmpstr, strlen(tmpstr));
			cap_free(tmpstr);
		} else
			assign_str(FILE0CAP, BADCAP, strlen(BADCAP));
		break;
	}
	}
}

void
sat_print_new_file_acl (acl_t acl)
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("New file acl     = %s\n", sat_str_acl(acl));
		break;
	case OM_LINEAR:
		printf("newacl:%s ", sat_str_acl(acl));
		break;
        case OM_SGIF: {
		char *tmpstr;
		tmpstr = acl_to_text(acl, (ssize_t *) NULL);
		if (tmpstr) {
			assign_strdup(FILE0ACL, tmpstr, strlen(tmpstr));
			acl_free(tmpstr);
		} else
			assign_str(FILE0ACL, BADACL, strlen(BADACL));
		break;
	}
	}
}

/*
 * Return the open flags in textual form.
 */
char *
sat_str_open_flags (int flags)
{
	static char buf[256];

	flags += FOPEN;		/* reverse the -= FOPEN done in the kernel */

	if ((flags & O_ACCMODE) == O_RDONLY)
		strcpy( buf, "O_RDONLY" );
	if ((flags & O_ACCMODE) == O_WRONLY)
		strcpy( buf, "O_WRONLY" );
	if ((flags & O_ACCMODE) == O_RDWR)
		strcpy( buf, "O_RDWR" );

	if (flags & O_APPEND)
		strcat( buf, " | O_APPEND" );
	if (flags & O_CREAT)
		strcat( buf, " | O_CREAT" );
	if (flags & O_EXCL)
		strcat( buf, " | O_EXCL" );
	if (flags & O_NDELAY)
		strcat( buf, " | O_NDELAY" );
	if (flags & O_NOCTTY)
		strcat( buf, " | O_NOCTTY" );
	if (flags & O_NONBLOCK)
		strcat( buf, " | O_NONBLOCK" );
	if (flags & O_SYNC)
		strcat( buf, " | O_SYNC" );
	if (flags & O_TRUNC)
		strcat( buf, " | O_TRUNC" );

	return buf;
}

char *
sat_str_hex( unsigned char * bytes, int len )
{
	static char hex[] = "0123456789ABCDEF";
	static char buf[50];
	char * bufp;
	int tmp;

	buf[0] = 0;
	if ( len <= 0 )
		return buf;
	len = ((len - 1) & 15) + 1;
	bufp = buf;
	do {
		tmp = *bytes++;
		bufp[0] = hex[ tmp >> 4 ];
		bufp[1] = hex[ tmp & 15 ];
		bufp[2] = ' ';
		bufp += 3;
	} while ( --len );
	*--bufp = 0;
	return buf;
}

char *
sat_str_signame( int signal )
{
	static char *signame[] = {
		"SIGNULL",
		"SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT",
		"SIGEMT", "SIGFPE", "SIGKILL", "SIGBUS", "SIGSEGV", "SIGSYS",
		"SIGPIPE", "SIGALRM", "SIGTERM", "SIGUSR1", "SIGUSR2",
		"SIGCHLD", "SIGPWR", "SIGWINCH", "SIGURG", "SIGIO", "SIGSTOP",
		"SIGTSTP", "SIGCONT", "SIGTTIN", "SIGTTOU", "SIGVTALRM",
		"SIGPROF", "SIGXCPU", "SIGXFSZ", "SIG32", "SIGCKPT",
	};

	/* contiguous signals */
	if (signal >= 0 && signal < (sizeof (signame) / sizeof (signame[0])))
		return signame[signal];

	/* non-contiguous signals */
	switch (signal) {
		case SIGRTMIN:
			return "SIGRTMIN";
		case SIGRTMAX:
			return "SIGRTMAX";
		case SIGPTINTR:
			return "SIGPTINTR";
		case SIGPTRESCHED:
			return "SIGPTRESCHED";
	}
	return "<Bad signal number>";
}


/*
 * print device info
 */
char *
sat_str_dev( dev_t dev )
{
	static char buf[80];

	if (dev == (o_dev_t)-1 || dev == (dev_t)-1)
		return "NODEV";

	if (file_major == 1)
		sprintf(buf, "%d,%d", ((unsigned)dev>>8) & 0x7f, dev & 0xff);
	else
		sprintf(buf, "%d,%d", major(dev), minor(dev));

	return buf;
}


void
sat_print_iphdr( char * cp, int len )
{
	struct ip *ip = (struct ip *)cp;
	char buf[20];

	(void)strcpy(buf, inet_ntoa(ip->ip_src));
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("IP Src/Dest Addr = %s/%s\n",buf,inet_ntoa(ip->ip_dst));
		break;
	case OM_LINEAR:
		printf("src/dest:%s/%s ", buf, inet_ntoa(ip->ip_dst));
		break;	
	case OM_SGIF:
		assign_strdup(ADDR, buf, strlen(buf));
		assign_strdup(DESTADDR, inet_ntoa(ip->ip_dst), strlen(inet_ntoa(ip->ip_dst)));
		break;
	}

	if ( len <= sizeof(*ip) )
		return;
	len -= sizeof(*ip);
	cp  += sizeof(*ip);
	while ( len > 0 ) {
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("IP Options       = %s\n", 
				sat_str_hex( (unsigned char *)cp, len ));
			break;
		case OM_LINEAR:
			printf ("IPopt:%s ", 
				sat_str_hex( (unsigned char *)cp, len ));
			break;
		case OM_SGIF: {
			char *hexstring;
			hexstring = sat_str_hex( (unsigned char *)cp, len );
			assign_strdup(IPOPTS, hexstring, strlen(hexstring));
			break;
		}
		}
		len -= 16;
		cp  += 16;
	}
}

void
sat_print_tcpiphdr( void * cp, int len )
{
	struct tcpiphdr *ti = (struct tcpiphdr *)cp;
	char buf[20];
	(void)strcpy(buf, inet_ntoa(ti->ti_src));
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("IP src/dest addr = %s/%s\n",buf,inet_ntoa(ti->ti_dst));
		break;
	case OM_LINEAR:
		printf("src/dest:%s/%s ", buf, inet_ntoa(ti->ti_dst));
		break;
	case OM_SGIF:
		assign_strdup(ADDR, buf, strlen(buf));
		assign_strdup(DESTADDR, inet_ntoa(ti->ti_dst), strlen(inet_ntoa(ti->ti_dst)));
		break;
	}

	if ( len <= sizeof(struct ip) )
		return;
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("   Src/Dest Port = %d/%d\n",ti->ti_sport,ti->ti_dport);
		break;
	case OM_LINEAR:
		printf("ports:%d/%d ",ti->ti_sport,ti->ti_dport);
		break;
	case OM_SGIF:
		assign_I32dup(PORT, ti->ti_sport);
		assign_I32dup(DESTPORT, ti->ti_dport);
		break;
	}
}

void
sat_print_ifname( char * ifname )
{
	char local[SATIFNAMSIZ+1];

	local[SATIFNAMSIZ] = 0; /* ensure null termination */
	strncpy(local, ifname, SATIFNAMSIZ);

	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf ("Interface Name   = %s\n", local);
		break;
	case OM_LINEAR:
		printf ("IFname:%s ", local);
		break;
	case OM_SGIF:
		assign_strdup(IFNAME, local, strlen(local));
		break;
	}
}

void
sat_print_idiom( int idiom )
{
	/* Known interface idioms */
	static struct idiomswtch {
		char *	idiom_name;
		int 	idiom_value;
	} idioms[] = {
		{ "MONO",	IDIOM_MONO	},
		{ "BSO",	IDIOM_BSO_REQ	},
		{ "BSO_TX",	IDIOM_BSO_TX	},
		{ "BSO_RX",	IDIOM_BSO_RX	},
		{ "CIPSO",	IDIOM_CIPSO	},
		{ "CIPSO1",	IDIOM_TT1	},
		{ "SGIPSO",	IDIOM_SGIPSO	},
 		{ "CIPSOT1",	IDIOM_TT1	},
 		{ "CIPSO2",	IDIOM_CIPSO2	},
		{ "SGIPSO2",	IDIOM_SGIPSO2	},
		{ "SGIPSOD",	IDIOM_SGIPSOD	},
		{ "SGIPSO2_NO_UID",	IDIOM_SGIPSO2_NO_UID  },
		{ "CIPSO2T1",	IDIOM_TT1_CIPSO2      }
	};
	struct idiomswtch * ids;

	ids = idioms; 

	while ( ids->idiom_value != idiom && ids->idiom_value != IDIOM_MAX ) 
		++ids;
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf ("Interface Idiom  = %s\n", ids->idiom_name );
		break;
	case OM_LINEAR:
		printf ("idiom:%s ", ids->idiom_name );
		break;
	}
}

void
sat_print_ioctl( int ioctl, int len, struct ifreq * ifr)
{
	char *ioctl_fmt;
	char *cp;

	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		ioctl_fmt = "Interface IOCTL  = %s\n";
		break;
	case OM_LINEAR:
		ioctl_fmt = "ioctl(%s)";
		break;
	case OM_SGIF:
		assign_I32dup(IOCTLCMD, ioctl);
		break;
	}

	sat_print_ifname( ifr->ifr_name );	
	switch ( ioctl ) {
        case SIOCSIFFLAGS:
		if (brief != OM_SGIF)
			printf( ioctl_fmt, "SIOCSIFFLAGS");
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("Interface Flags  = 0x%04x\n", ifr->ifr_flags );
			break;
		case OM_LINEAR:
			printf(",flags:0x%04x ", ifr->ifr_flags );
			break;
		case OM_SGIF:
			assign_I32dup(IFFLAGS, ifr->ifr_flags);
			break;
		}
		break;
        case SIOCSIFMETRIC:
		if (brief != OM_SGIF)
			printf( ioctl_fmt, "SIOCSIFMETRIC");
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("Interface Metric = %d\n", ifr->ifr_metric );
			break;
		case OM_LINEAR:
			printf(",metric:%d ", ifr->ifr_metric );
			break;
		case OM_SGIF:
			assign_I32dup(IFMETRIC, ifr->ifr_metric);
			break;
		}
		break;
        case SIOCADDMULTI:
		if (brief != OM_SGIF)
			printf( ioctl_fmt, "SIOCADDMULTI");
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			break;
		case OM_LINEAR:
			printf(",");
			break;
		case OM_SGIF:
			break;
		}
		sat_print_sockaddr(sizeof(ifr->ifr_addr), 
				   (char *)&ifr->ifr_addr);
		break;
        case SIOCDELMULTI:
		if (brief != OM_SGIF)
			printf( ioctl_fmt, "SIOCDELMULTI");
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			break;
		case OM_LINEAR:
			printf(",");
			break;
		case OM_SGIF:
			break;
		}
		sat_print_sockaddr(sizeof(ifr->ifr_addr), 
				   (char *)&ifr->ifr_addr);
		break;
        case SIOCSIFSTATS:
		if (brief != OM_SGIF)
			printf( ioctl_fmt, "SIOCSIFSTATS");
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Input Packets    = %d\n",
			    ifr->ifr_stats.ifs_ipackets);
			printf ("Input Errors     = %d\n",
			    ifr->ifr_stats.ifs_ierrors);
			printf ("Output Packets   = %d\n",
			    ifr->ifr_stats.ifs_opackets);
			printf ("Output Errors    = %d\n",
			    ifr->ifr_stats.ifs_oerrors);
			printf ("Collisions       = %d\n",
			    ifr->ifr_stats.ifs_collisions);
			break;
		case OM_LINEAR:
			printf ("inpac:%d,", ifr->ifr_stats.ifs_ipackets);
			printf ("inerr:%d,", ifr->ifr_stats.ifs_ierrors);
			printf ("outpac:%d,", ifr->ifr_stats.ifs_opackets);
			printf ("outerr:%d,", ifr->ifr_stats.ifs_oerrors);
			printf ("collisions:%d ",ifr->ifr_stats.ifs_collisions);
			break;
		case OM_SGIF:
			assign_I32dup(IPACKETS, ifr->ifr_stats.ifs_ipackets);
			assign_I32dup(IERRORS, ifr->ifr_stats.ifs_ierrors);
			assign_I32dup(OPACKETS, ifr->ifr_stats.ifs_opackets);
			assign_I32dup(OERRORS, ifr->ifr_stats.ifs_oerrors);
			assign_I32dup(COLLISIONS, ifr->ifr_stats.ifs_collisions);
			break;
		}
		break;
        case SIOCSIFHEAD:
		if (brief != OM_SGIF)
			printf( ioctl_fmt, "SIOCSIFHEAD (New Head Interface)");
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			break;
		case OM_LINEAR:
			printf(" ");
			break;
		case OM_SGIF:
			break;
		}
		break;
        case SIOCSIFLABEL:
		if (brief != OM_SGIF)
			printf( ioctl_fmt, "SIOCSIFLABEL");
#ifdef IFSEC_VECTOR
		    if (brief != OM_SGIF)
			printf("IF SEC = 0x%08x/%d\n",
			ifr->ifr_base, ifr->ifr_len);
#else /* IFSEC_VECTOR */
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
		    printf("Max/Min IF label = 0x%08x/0x%08x\n",
			SEC(ifr)->ifs_label_max, 
			SEC(ifr)->ifs_label_min);
		    printf("DOI, Max/Min auth= %d (0x%08x), %02x/%02x\n",
			SEC(ifr)->ifs_doi,
			SEC(ifr)->ifs_doi, 
			SEC(ifr)->ifs_authority_max,
			SEC(ifr)->ifs_authority_min);
			break;
		case OM_LINEAR:
		    printf("IFlabels:0x%08x/0x%08x ",
			SEC(ifr)->ifs_label_max, 
			SEC(ifr)->ifs_label_min);
		    printf("DOI,Max,Min:%d(0x%08x),%02x,%02x ",
			SEC(ifr)->ifs_doi,
			SEC(ifr)->ifs_doi, 
			SEC(ifr)->ifs_authority_max,
			SEC(ifr)->ifs_authority_min);
			break;
		case OM_SGIF:
			assign_maclbldup(LABEL_MAX, (mac_t)(ifs_label_max), mac_size(ifs_label_max));
			assign_maclbldup(LABEL_MIN, (mac_t)(ifs_label_min), mac_size(ifs_label_min));
			assign_I32dup(DOI, ifr->ifs_doi);
			assign_I32dup(AUTHORITY_MAX, ifr->ifs_authority_max);
			assign_I32dup(AUTHORITY_MIN, ifr->ifs_authority_min);
			assign_I32dup(IDIOM, ifr->ifs_idiom);
			break;
		}
		if (brief != OM_SGIF)
			sat_print_idiom( SEC(ifr)->ifs_idiom );
#endif /* IFSEC_VECTOR */
		break;
	default:
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Unknown IOCTL    = 0x%08x\n", ioctl );
			cp = (char *)ifr;
			while ( len > 0 ) {
				printf ("                 = %s\n", 
					sat_str_hex((unsigned char *)cp, len ));
				len -= 16;
				cp += 16;
			}
			break;
		case OM_LINEAR:
			printf ("ioctl(0x%08x):", ioctl );
			cp = (char *)ifr;
			while ( len > 0 ) {
				printf ("%s",
					 sat_str_hex((unsigned char *)cp, len ));
				len -= 16;
				cp += 16;
			}
			printf(" ");
			break;
		case OM_SGIF:{
			char *hexstring;
			hexstring = sat_str_hex( (unsigned char *)cp, len );
			assign_strdup(IOC_UNKNOWN, hexstring, strlen(hexstring));
			break;
		}
		}
	}
}

void
sat_print_new_time(char *kind, time_t when)
{
	char timeb[80];

	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("New %s time  = %s",kind, (when)? ctime(&when): "Now\n");
		break;
	case OM_LINEAR:
		if (when) {
			strcpy(timeb, ctime(&when));
			timeb[strlen(timeb) - 1] = '\0';
			printf("new%ctime:%s", *kind, timeb);
		}
		else
			printf("new%ctime:now", *kind);
		break;
	}
}

void
sat_print_socket_id_desc(u_int sid, int des)
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
        	printf("Socket id, desc. = %08X, %d\n", (u_int) sid, des);
		break;
	case OM_LINEAR:
        	printf("socket,fd:%08X,%d ", (u_int) sid, des);
		break;
	case OM_SGIF:
		break;
	}
}
void
sat_print_socket_dac(uid_t uid, uid_t rcvuid, short uidcount, int *list, int count)
{
    	uid_t acluid;			/* To change int to uid_t */

	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
        	printf("Socket uid       = %s\n", sat_str_user(uid) );
		if (rcvuid != uid)
        		printf("Socket rcvuid    = %s\n", 
			       sat_str_user(rcvuid));
		if (!cipso_enabled)
			return;
        	if (uidcount != 0)
			printf("Socket acl       = ");
		if (uidcount > SOACL_MAX || 
		    (uidcount < 0 && uidcount != WILDACL)) {
		    	printf("BADACL\n");
		} 
		else
			if (uidcount == WILDACL)
				printf("WILDACL\n");
		else if (uidcount > 0) {
		    	acluid = *list++;
		    	printf("%s", sat_str_user(acluid));
			--uidcount;
		    	while (uidcount-- > 0) {
				acluid = *list++;
			    	printf(",%s", sat_str_user(acluid));
			}
			printf("\n");
		}
		break;
	case OM_LINEAR:
        	printf("souids:%s,%s ",sat_str_user(uid),sat_str_user(rcvuid));
		if (!cipso_enabled)
			return;
		if (uidcount != 0)
			printf("soacl:");
		if (uidcount > SOACL_MAX || 
		    (uidcount < 0 && uidcount != WILDACL)) {
		    	printf("BADACL");
		} 
		else
			if (uidcount == WILDACL)
				printf("WILDACL ");
		else if (uidcount > 0) {
		    	acluid = *list++;
		    	printf("%s", sat_str_user(acluid));
			--uidcount;
		    	while (uidcount-- > 0) {
				acluid = *list++;
			    	printf(",%s", sat_str_user(acluid));
			}
		}
		break;
	case OM_SGIF:
		assign_I32dup(SOUID + count, uid);
		assign_I32dup(SORCVUID + count, rcvuid);
		if (!cipso_enabled)
			return;
		if (uidcount > 0) {
			assign_I32dup(SOACLUIDCOUNT + count, uidcount);
			assign_I32Adup(SOACLUIDLIST + count, list, uidcount);
		}
		break;
	}
}

void
sat_print_domain(int domain)
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
        	printf("Domain           = %d (%s)\n", domain,
		    sat_str_domain(domain));
		break;
	case OM_LINEAR:
        	printf("domain:%s ", sat_str_domain(domain));
		break;
	case OM_SGIF:
		break;
	}
}

void
sat_print_datagram_label(mac_t lp)
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf ("Datagram label   = %s\n", sat_str_label(lp));
		break;
	case OM_LINEAR:
		printf ("paclabel:%s ", sat_str_label(lp));
		break;
	}
}

void
sat_print_datagram_uid(uid_t uid)
{
	switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
		printf ("Datagram uid     = %s\n", 
			sat_str_user(uid) );
		break;
		case OM_LINEAR:
		printf ("pacuid:%s ", 
			sat_str_user(uid));
		break;
	}
}


void
sat_print_audit_event(int event)
{
	char *eventname = sat_eventtostr(event);

	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		if (eventname)
			printf("Audit event      = %s\n", eventname);
		else
			printf("Audit event      = invalid event #%d\n", event);
		break;
	case OM_LINEAR:
		if (eventname)
			printf("satevent:%s ", eventname);
		else
			printf("satevent:%d ", event);
		break;
	}
}

void
sat_print_file_descriptor(int pid, int fd, short field)
{
	char *mapped = sat_get_fdmap(pid, fd);

	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		if (fdmap && mapped)
			printf("File descriptor  = %d(\"%s\")\n", fd, mapped);
		else
			printf("File descriptor  = %d\n", fd);
		break;
	case OM_LINEAR:
		if (fdmap && mapped)
			printf("file:\"%s\" ", mapped);
		else
			printf("fd:%d ", fd);
		break;
	case OM_SGIF:
		if (fdmap && mapped) 
			assign_strdup(MAPPEDNAME0 + field, mapped, strlen(mapped));
		assign_I32dup(FILEDES0 + field, fd); /* short */
		break;

	}
}


void
sat_print_sat_control( char *body )
{

	struct sat_control *ctl;

	ctl = (struct sat_control *) body;

	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf ("Audit control    = ");
		break;
	case OM_LINEAR:
	case OM_SGIF:
		assign_I32dup(CTL_SAT_CMD, ctl->sat_cmd);
		break;
	}

	switch (ctl->sat_cmd) {
	    case SATCTL_AUDIT_ON:
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("set audit event\n");
			sat_print_audit_event(ctl->sat_arg);
			break;
		case OM_LINEAR:
			printf("on:");
			sat_print_audit_event(ctl->sat_arg);
			break;
		case OM_SGIF:
			assign_I32dup(CTL_SAT_ARG, ctl->sat_arg);
			break;
		}
		break;
	    case SATCTL_AUDIT_OFF:
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("clear audit event\n");
			sat_print_audit_event(ctl->sat_arg);
			break;
		case OM_LINEAR:
			sat_print_audit_event(ctl->sat_arg);
			printf("off:");
			break;
		case OM_SGIF:
			assign_I32dup(CTL_SAT_ARG, ctl->sat_arg);
			break;
		}

		break;
	    case SATCTL_AUDIT_QUERY:
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("query audit event\n");
			sat_print_audit_event(ctl->sat_arg);
			break;
		case OM_LINEAR:
			printf("read:");
			sat_print_audit_event(ctl->sat_arg);
			break;
		case OM_SGIF:
			assign_I32dup(CTL_SAT_ARG, ctl->sat_arg);
			break;
		}
		break;
	    case SATCTL_LOCALAUDIT_ON:
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("set process-local event\n");
			sat_print_pid(ctl->sat_pid);
			sat_print_audit_event(ctl->sat_arg);
			break;
		case OM_LINEAR:
			printf("localon:");
			sat_print_pid(ctl->sat_pid);
			sat_print_audit_event(ctl->sat_arg);
			break;
		case OM_SGIF:
			assign_I32dup(CTL_SAT_ARG, ctl->sat_arg);
			assign_I32dup(CTL_SAT_PID, ctl->sat_pid);
			break;
		}
		break;
	    case SATCTL_LOCALAUDIT_OFF:
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("clear process-local event\n");
			sat_print_pid(ctl->sat_pid);
			sat_print_audit_event(ctl->sat_arg);
			break;
		case OM_LINEAR:
			printf("localoff:");
			sat_print_pid(ctl->sat_pid);
			sat_print_audit_event(ctl->sat_arg);
			break;
		case OM_SGIF:
			assign_I32dup(CTL_SAT_ARG, ctl->sat_arg);
			assign_I32dup(CTL_SAT_PID, ctl->sat_pid);
			break;
		}
		break;
	    case SATCTL_LOCALAUDIT_QUERY:
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("query process-local event\n");
			sat_print_pid(ctl->sat_pid);
			sat_print_audit_event(ctl->sat_arg);
			break;
		case OM_LINEAR:
			printf("localread:");
			sat_print_pid(ctl->sat_pid);
			sat_print_audit_event(ctl->sat_arg);
			break;
		case OM_SGIF:
			assign_I32dup(CTL_SAT_ARG, ctl->sat_arg);
			assign_I32dup(CTL_SAT_PID, ctl->sat_pid);
			break;
		}
		break;
	    case SATCTL_SET_SAT_ID:
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("set Audit ID\n");
			printf("Audit ID         = %s\n",
			    sat_str_user((uid_t)ctl->sat_arg));
			break;
		case OM_LINEAR:
			printf("suid:%s ",
			    sat_str_user((uid_t)ctl->sat_arg));
			break;
		case OM_SGIF:
			assign_I32dup(CTL_SAT_ARG, ctl->sat_arg);
			break;
		}
		break;
	    case SATCTL_GET_SAT_ID:
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("get Audit ID\n");
			break;
		case OM_LINEAR:
			printf("getsuid ");
			break;
		case OM_SGIF:
			assign_I32dup(CTL_SAT_ARG, ctl->sat_arg);
			break;
		}
		break;
	} /* end switch */
}

void
sat_print_acct_timers( struct acct_timers *timers )
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("Times (ns):\n");
		printf("  User time      = %lld\n", timers->ac_utime);
		printf("  System time    = %lld\n", timers->ac_stime);
		printf("  Block I/O wait = %lld\n", timers->ac_bwtime);
		printf("  Raw I/O wait   = %lld\n", timers->ac_rwtime);
		printf("  Run queue wait = %lld\n", timers->ac_qwtime);
		break;
	case OM_LINEAR:
		printf("user:%lld system:%lld ",
		       timers->ac_utime, timers->ac_stime);
		printf("blocki/o:%lld rawi/o:%lld runqwait:%lld ",
		       timers->ac_bwtime, timers->ac_rwtime,
		       timers->ac_qwtime);
		break;
	}
}

void
sat_print_acct_counts( struct acct_counts *counts )
{
	switch (brief) {
	case OM_VERBOSE:
	case OM_BRIEF:
		printf("Counts:\n");
		printf("  Memory usage   = %lld\n", counts->ac_mem);
		printf("  swaps          = %lld\n", counts->ac_swaps);
		printf("  bytes read     = %lld\n", counts->ac_chr);
		printf("  bytes written  = %lld\n", counts->ac_chw);
		printf("  blocks read    = %lld\n", counts->ac_br);
		printf("  blocks written = %lld\n", counts->ac_bw);
		printf("  read syscalls  = %lld\n", counts->ac_syscr);
		printf("  write syscalls = %lld\n", counts->ac_syscw);
		break;
	case OM_LINEAR:
		printf("mem:%lld swaps:%lld chr:%lld chw:%lld ",
		       counts->ac_mem, counts->ac_swaps,
		       counts->ac_chr, counts->ac_chw);
		printf("br:%lld bw:%lld syscr:%lld syscw:%lld ",
		       counts->ac_br, counts->ac_bw,
		       counts->ac_syscr, counts->ac_syscw);
		break;
	}
}


/*
 *  bodies of audit records are displayed readably, for all record types
 */
void
sat_print_body (struct sat_hdr_info *hdr, char *body)
{
	int i;
	int pnskip;

	switch (hdr->sat_rectype) {

	case SAT_ACCESS_DENIED:
	case SAT_ACCESS_FAILED:
	case SAT_CHDIR:
	case SAT_CHROOT:
	case SAT_READ_SYMLINK:
	case SAT_FILE_CRT_DEL:
	case SAT_FILE_WRITE:
	case SAT_FILE_ATTR_READ:
		(void) sat_print_pathname (NULL, NULL, body, 0);
		break;

	case SAT_FILE_ATTR_WRITE: {
		int	filemode;
		uid_t	fileown;
		uid_t	filegrp;
		int	label_size;
		int	cap_size;
		int	acl_size;
		int	dacl_size;
		time_t	atime;
		time_t	mtime;

		if (file_major == 1) {
			struct sat_file_attr_write_1_0 *faw_1_0;
			faw_1_0 = (struct sat_file_attr_write_1_0 *) body;
			body += ALIGN(sizeof(struct sat_file_attr_write_1_0));
			filemode = faw_1_0->newattr.sat_filemode;
			fileown = faw_1_0->newattr.chown.sat_fileown;
			filegrp = faw_1_0->newattr.chown.sat_filegrp;
			label_size = faw_1_0->newattr.sat_label_size;
			atime = faw_1_0->newattr.utime.sat_atime;
			mtime = faw_1_0->newattr.utime.sat_mtime;

		} else {
			struct sat_file_attr_write *	faw;
			faw = (struct sat_file_attr_write *) body;
			body += ALIGN(sizeof(struct sat_file_attr_write));
			filemode = (int)faw->newattr.sat_filemode;
			fileown = faw->newattr.chown.sat_fileown;
			filegrp = faw->newattr.chown.sat_filegrp;
			label_size = faw->newattr.sat_label_size;
			cap_size = faw->newattr.sat_cap_size;
			acl_size = faw->newattr.acl_set.sat_acl_size;
			dacl_size = faw->newattr.acl_set.sat_dacl_size;
			atime = faw->newattr.utime.sat_atime;
			mtime = faw->newattr.utime.sat_mtime;
		}

		switch (hdr->sat_syscall) {
		case SAT_SYSCALL_CHMOD:
			if (brief == OM_SGIF)
				assign_I32dup(FILE0MODE, filemode);
			else
				sat_print_new_mode_bits(filemode);
			break;
		case SAT_SYSCALL_CHOWN:
			if (brief == OM_SGIF) {
				assign_I32dup(FILE0OWN, fileown); /* uid_t */
				assign_I32dup(FILE0GRP, filegrp); /* gid_t */
			} else {
				sat_print_new_file_owner(fileown);
				sat_print_new_file_group(filegrp);
			}
			break;
		case SAT_SYSCALL_SYSSGI:
			switch (hdr->sat_subsyscall) {
				case SGI_SETLABEL:
				case SGI_MAC_SET:
					sat_print_new_file_label((mac_t)body);
					body += label_size;
					break;
				case SGI_CAP_SET:
					sat_print_new_file_cap((cap_t)body);
					body += cap_size;
					break;
				case SGI_ACL_SET:
					if (acl_size) {
						sat_print_new_file_acl((acl_t)body);
						body += acl_size;
					}
					if (dacl_size) {
						sat_print_new_file_acl((acl_t)body);
						body += dacl_size;
					}
					break;
			}
			break;
		case SAT_SYSCALL_UTIME:
			if (brief == OM_SGIF) {
				assign_I32dup(ATIME, atime); /* time_t */
				assign_I32dup(MTIME, mtime); /* time_t */
			} else {
				sat_print_new_time("access", atime);
				sat_print_new_time("modify", mtime);
			}
			break;
		}
		}
		(void) sat_print_pathname (NULL, NULL, body, 0);
		break;

	case SAT_SYSACCT: {
		struct sat_sysacct *		acct;
		acct = (struct sat_sysacct *) body;
		switch (brief) {
		case OM_BRIEF:
		case OM_VERBOSE:
			printf ("Accounting state = %s\n",
				(acct->sat_acct_state)? "On":"Off");
			break;
		case OM_LINEAR:
			printf ("acct:%s ", (acct->sat_acct_state)? "on":"off");
			break;
                case OM_SGIF:
			assign_I32dup(ACCTSTATE, acct->sat_acct_state);
			break;			
		}
		body += ALIGN(sizeof(struct sat_sysacct));
		if (acct->sat_acct_state)
			(void) sat_print_pathname (NULL, NULL, body, 0);
		}
		break;

	case SAT_FILE_CRT_DEL2:
		pnskip = sat_print_pathname ("File from", "from", body, 0);

		body += pnskip;
		(void) sat_print_pathname ("File to", "to", body, 1);
		break;

	case SAT_OPEN:
	case SAT_OPEN_RO: {
		struct sat_open *		open;
		open = (struct sat_open *) body;
		switch (brief) {
		case OM_BRIEF:
			printf("file descriptor %d, %s, %screated\n",
			    open->sat_filedes,
			    sat_str_open_flags(open->sat_open_flags),
			    (open->sat_file_created)? "":"not ");
			break;
		case OM_VERBOSE:
			sat_print_file_descriptor(hdr->sat_pid,
			    open->sat_filedes, 0);
			printf ("Open flags       = %s\n",
				sat_str_open_flags (open->sat_open_flags));
			printf ("Created          = %s\n",
				(open->sat_file_created)? "Yes":"No");
			break;
		case OM_LINEAR:
			printf("fd:%d,(%s),%s ",
			    open->sat_filedes,
			    sat_str_open_flags(open->sat_open_flags),
			    (open->sat_file_created)? "created":"exists");
			break;
		case OM_SGIF:
			sat_print_file_descriptor(hdr->sat_pid, open->sat_filedes, 0);
			assign_I32dup(OPENCREATE, (__int32_t)(open->sat_file_created)); /* short */
			assign_I32dup(OPENFLAGS, open->sat_open_flags); /* int */
			break;
		}
		body += ALIGN(sizeof(struct sat_open));
		(void) sat_print_pathname (NULL, NULL, body, 0);
		sat_set_fdmap(hdr->sat_pid, open->sat_filedes, body);
		}
		break;

	case SAT_MOUNT:

		if (file_major == 1) {
			struct sat_mount_1_0 *	mnt_1_0;
			mnt_1_0 = (struct sat_mount_1_0 *) body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf ("Device           = %s\n",
				    sat_str_dev(mnt_1_0->sat_fs_dev));
				break;
			case OM_LINEAR:
				printf ("dev:%s ",
				    sat_str_dev(mnt_1_0->sat_fs_dev));
				break;
			case OM_SGIF:
				assign_I32dup(FILE2DEVICE, mnt_1_0->sat_fs_dev); /* o_dev_t */
				break;
			}
			body += ALIGN(sizeof(struct sat_mount_1_0));
			pnskip = sat_print_pathname ("Mounted device",
			    "mntdev", body, 1);
			if (mnt_1_0->sat_npaths == 2) {
				body += pnskip;
				(void) sat_print_pathname ("Mount point",
				    "mntpt", body, 0);
			}
		} else
			(void) sat_print_pathname ("Mount point",
			    "mntpt", body, 0);
		break;

	case SAT_EXEC: {
		int	npaths;
		int	interpreter;
		uid_t	euid;
		gid_t	egid;

		if (file_major == 1) {
			struct sat_exec_1_0 *sxec_1_0;
			sxec_1_0 = (struct sat_exec_1_0 *) body;
			body += ALIGN(sizeof(struct sat_exec_1_0));
			npaths = sxec_1_0->sat_npaths;
			interpreter = sxec_1_0->sat_interpreter;
			euid = sxec_1_0->sat_euid;
			egid = sxec_1_0->sat_egid;
		} else {
			struct sat_exec *sxec;
			sxec = (struct sat_exec *) body;
			body += ALIGN(sizeof(struct sat_exec));
			npaths = sxec->sat_npaths;
			interpreter = sxec->sat_interpreter;
			euid = sxec->sat_euid;
			egid = sxec->sat_egid;
		}

		/*
		 * Only print the things that changed (the header info
		 * is from before the exec; the record info is from after).
		 * This only happens when set{u,g}id programs are involved.
		 */
		if (hdr->sat_euid != euid) {
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf("New eff. uid     = %s\n",
				    sat_str_user(euid));
				break;
			case OM_LINEAR:
				printf("neweuid:%s ", sat_str_user(euid));
				break;
			case OM_SGIF:
				assign_I32dup(NEWEUID, euid);	
				break;
			}
		}
		if (hdr->sat_egid != egid) {
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf("New eff. gid     = %s\n",
				    sat_str_group(egid));
				break;
			case OM_LINEAR:
				printf("newegid:%s ", sat_str_group(egid));
				break;
		        case OM_SGIF:
				assign_I32dup(NEWEGID, egid);	
				break;
			}
		}


		pnskip = sat_print_pathname ("File exec'ed", "program", body, 0);
		body += pnskip;
		npaths--;
		/*
		 * These fields are currently not created on exec.
		 */
		if (interpreter) {
			pnskip = sat_print_pathname ("Interpreter", "shell",
			    body, 1);
			body += pnskip;
			npaths--;
		}

		while (npaths--) {
			pnskip = sat_print_pathname ("Shared library", "slib",
			    body, 2 + npaths);
			body += pnskip;
		}
		break;

		}

	case SAT_FCHDIR: {
		struct sat_fchdir *		fchd;
		fchd = (struct sat_fchdir *) body;
		sat_print_file_descriptor(hdr->sat_pid, fchd->sat_filedes, 0);
		}
		break;

	case SAT_FD_READ: {
		struct sat_fd_read *		fdrd;
		fdrd = (struct sat_fd_read *) body;
		sat_print_file_descriptor(hdr->sat_pid, fdrd->sat_filedes, 0);
		}
		break;

	case SAT_FD_READ2:
		if (file_major == 1) {
			struct sat_fd_read2_1_0 *fdrd2_1_0;
			fdrd2_1_0 = (struct sat_fd_read2_1_0 *) body;
			if (brief != OM_SGIF) {
				for (i=0; i < sizeof(fdrd2_1_0->sat_fdset)*8; i++)
					if (FD_ISSET(i, &(fdrd2_1_0->sat_fdset)))
						sat_print_file_descriptor(hdr->sat_pid,
								  i, i);
			} else	
				assign_I32Adup(FDSET, (__int32_t *)(body), 8);
		} else {
#if SAT_VERSION_MAJOR_3 >= 2
			struct sat_fd_read2 *	fdrd2;
			unsigned short *fdlist;
			fdrd2 = (struct sat_fd_read2 *) body;
			fdlist = (unsigned short *)(fdrd2+1);
			if (brief != OM_SGIF) {
				for (i=0; i < fdrd2->sat_nfds; i++)
					sat_print_file_descriptor(hdr->sat_pid,
							  (int) fdlist[i], i);
			} else {
				assign_I32dup(NFDS, fdrd2->sat_nfds);
				assign_I16Adup(FDLIST, (short *)fdlist, fdrd2->sat_nfds);
			}
#endif
		}
		break;

	case SAT_TTY_SETLABEL: {
		struct sat_tty_setlabel *	ttylbl;
		ttylbl = (struct sat_tty_setlabel *) body;
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Stream descriptr = %d\n", ttylbl->sat_filedes);
			printf ("Stream new label = %s\n",
			    sat_str_label((mac_t)(ttylbl+1)));
			break;
		case OM_LINEAR:
			printf ("sd:%d newlabel:%s ", ttylbl->sat_filedes,
			    sat_str_label((mac_t)(ttylbl+1)));
			break;
		case OM_SGIF: {
			char *tmpstr;
			assign_I32dup(FILEDES0, (__int32_t)(ttylbl->sat_filedes));
			tmpstr = mac_to_text((mac_t)(ttylbl+1), (size_t *) NULL);
			if (tmpstr) {
				assign_strdup(FILE0LABEL, tmpstr, strlen(tmpstr));
				mac_free(tmpstr);
			} else
				assign_strdup(FILE0LABEL, BADLABEL, strlen(BADLABEL));
			break;
		}
		}
	}
		break;

	case SAT_FD_WRITE: {
		struct sat_fd_write *		fdwr;
		fdwr = (struct sat_fd_write *) body;
		sat_print_file_descriptor(hdr->sat_pid, fdwr->sat_filedes, 0);
		}
		break;

	case SAT_FD_ATTR_WRITE: {
		int filedes;
		int filemode;
		uid_t fileown;
		gid_t filegrp;
		if (file_major == 1) {
			struct sat_fd_attr_write_1_0 *	fdaw_1_0;
			fdaw_1_0 = (struct sat_fd_attr_write_1_0 *) body;
			filedes = fdaw_1_0->sat_filedes;
			filemode = fdaw_1_0->newattr.sat_filemode;
			fileown = fdaw_1_0->newattr.fchown.sat_fileown;
			filegrp = fdaw_1_0->newattr.fchown.sat_filegrp;
		} else {
			struct sat_fd_attr_write *	fdaw;
			fdaw = (struct sat_fd_attr_write *) body;
			filedes = fdaw->sat_filedes;
			filemode = (int)fdaw->newattr.sat_filemode;
			fileown = fdaw->newattr.fchown.sat_fileown;
			filegrp = fdaw->newattr.fchown.sat_filegrp;
		}
		sat_print_file_descriptor(hdr->sat_pid, filedes, 0);
		switch (hdr->sat_syscall) {
		case SAT_SYSCALL_FCHMOD:
			if (brief == OM_SGIF) 
				assign_I32dup(FILE0MODE, filemode); /* mode_t */
			
			else
				sat_print_new_mode_bits(filemode);
			break;
		case SAT_SYSCALL_FCHOWN:
			if (brief == OM_SGIF) {
				assign_I32dup(FILE0OWN, fileown); /* uid_t */
				assign_I32dup(FILE0GRP, filegrp); /* gid_t */
			} else {
				sat_print_new_file_owner(fileown);
				sat_print_new_file_group(filegrp);
			}
			break;
		}
		}
		break;

	case SAT_PIPE: {
		struct sat_pipe *		spipe;
		spipe = (struct sat_pipe *) body;
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Read file des.   = %d\n",
			    spipe->sat_read_filedes);
			printf ("Write file des.  = %d\n",
			    spipe->sat_write_filedes);
			break;
		case OM_LINEAR:
			printf ("read");
			sat_print_file_descriptor(hdr->sat_pid,
			    spipe->sat_read_filedes, 0);
			printf ("write");
			sat_print_file_descriptor(hdr->sat_pid,
			    spipe->sat_write_filedes, 1);
			sat_set_fdmap_path(hdr->sat_pid,
			    spipe->sat_read_filedes, "readpipe");
			sat_set_fdmap_path(hdr->sat_pid,
			    spipe->sat_write_filedes, "writepipe");
			break;
		case OM_SGIF:
			sat_print_file_descriptor(hdr->sat_pid,
			    spipe->sat_read_filedes, 0);
			sat_print_file_descriptor(hdr->sat_pid,
			    spipe->sat_write_filedes, 1);
			sat_set_fdmap_path(hdr->sat_pid,
			    spipe->sat_read_filedes, "readpipe");
			sat_set_fdmap_path(hdr->sat_pid,
			    spipe->sat_write_filedes, "writepipe");
			break;
		}
		}
		break;

	case SAT_DUP: {
		struct sat_dup *		sdup;
		char *cp;
		sdup = (struct sat_dup *) body;
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("Copied file des. = %d\n",sdup->sat_old_filedes);
			printf("New file des.    = %d\n",sdup->sat_new_filedes);
			break;
		case OM_LINEAR:
			printf ("old");
			sat_print_file_descriptor(hdr->sat_pid,
			    sdup->sat_old_filedes, 0);
			printf ("new");
			sat_print_file_descriptor(hdr->sat_pid,
			    sdup->sat_new_filedes, 1);
			break;
		case OM_SGIF:
			sat_print_file_descriptor(hdr->sat_pid,
			    sdup->sat_old_filedes, 0);
			sat_print_file_descriptor(hdr->sat_pid,
			    sdup->sat_new_filedes, 1);
			break;
		}
		if (cp = sat_get_fdmap(hdr->sat_pid, sdup->sat_old_filedes))
			sat_set_fdmap_path(hdr->sat_pid,
			    sdup->sat_new_filedes, cp);
		}
		break;

	case SAT_CLOSE: {
		struct sat_close *		sclose;
		sclose = (struct sat_close *) body;
		sat_print_file_descriptor(hdr->sat_pid, sclose->sat_filedes, 0);
		sat_set_fdmap_path(hdr->sat_pid, sclose->sat_filedes, NULL);
		}
		break;

	case SAT_FORK: {
		struct sat_fork *		sfork;
		sfork = (struct sat_fork *) body;
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("New process ID   = %d\n",
			    sat_normalize_pid(sfork->sat_newpid));
			break;
		case OM_LINEAR:
			printf ("newpid:%d ",
			    sat_normalize_pid(sfork->sat_newpid));
			break;
                case OM_SGIF: {
			int normpid;
			normpid = sat_normalize_pid(sfork->sat_newpid);
			assign_I32dup(NEWPID, normpid);
			break;
		   }
		}
		sat_copy_fdmap(hdr->sat_pid, sfork->sat_newpid);
		}
		break;

	case SAT_EXIT: {
		struct sat_exit *		sexit;
		sexit = (struct sat_exit *) body;
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Exit status      = %d\n",
			    sexit->sat_exit_status);
			break;
		case OM_LINEAR:
			printf ("status:%d ", sexit->sat_exit_status);
			break;
		case OM_SGIF:
			assign_I32dup(EXITSTATUS,  sexit->sat_exit_status);
			break;
		}
		sat_set_fdmap_path(hdr->sat_pid, -1, NULL);
		}
		break;

	case SAT_PROC_READ:
	case SAT_PROC_WRITE:
	case SAT_PROC_ATTR_READ:
	case SAT_PROC_ATTR_WRITE: {
		int	signo;
		pid_t	pid;
		uid_t	ruid;
		uid_t	euid;

		if (file_major == 1) {
			struct sat_proc_access_1_0 *	pacc_1_0;
			pacc_1_0 = (struct sat_proc_access_1_0 *) body;
			body += ALIGN(sizeof(struct sat_proc_access_1_0));
			signo = pacc_1_0->sat_signal;
			pid = pacc_1_0->sat_pid;
			ruid = pacc_1_0->sat_ruid;
			euid = pacc_1_0->sat_euid;
		} else {
			struct sat_proc_access *	pacc;
			pacc = (struct sat_proc_access *) body;
			body += ALIGN(sizeof(struct sat_proc_access));
			signo = pacc->sat_signal;
			pid = pacc->sat_pid;
			ruid = pacc->sat_ruid;
			euid = pacc->sat_euid;
		}
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			switch (hdr->sat_syscall) {
			case SAT_SYSCALL_KILL:
				printf ("Signal name      = %s (%d)\n", 
				    sat_str_signame(signo), signo);
				break;
			case SAT_SYSCALL_PROCBLK:
				printf ("Command           = %d\n", signo);
				break;
			}
			break;
		case OM_LINEAR:
			switch (hdr->sat_syscall) {
			case SAT_SYSCALL_KILL:
				printf ("sig:%s ", sat_str_signame(signo));
				break;
			case SAT_SYSCALL_PROCBLK:
				printf ("cmd:%d ", signo);
				break;
			}
			break;
		case OM_SGIF:
			switch (hdr->sat_syscall) {
			case SAT_SYSCALL_KILL:
				assign_I32dup(SIGNAL, signo);
				break;
			case SAT_SYSCALL_PROCBLK:
				assign_I32dup(PROCBLKCMD, signo);
				break;
			}
			break;
		}

		sat_print_pid(pid);

		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Real process uid = %s\n", sat_str_user (ruid));
			printf ("Eff. process uid = %s\n", sat_str_user (euid));
			if (mac_enabled)
				printf ("Process label    = %s\n",
					sat_str_label((mac_t)body));
			break;
		case OM_LINEAR:
			if (mac_enabled)
				printf ("reuid,label:%s,%s,%s ",
				    sat_str_user(ruid),
				    sat_str_user(euid),
				    sat_str_label((mac_t)body));
			else
				printf ("reuid:%s,%s ", sat_str_user(ruid),
				    sat_str_user(euid));
			break;
		case OM_SGIF:
			assign_I32dup(OBJRUID, ruid);
			assign_I32dup(OBJEUID, euid);
			if (mac_enabled)
				assign_maclbldup(FILE0LABEL, (mac_t)body, mac_size((mac_t)body));
		}
		}
		break;

	case SAT_PROC_OWN_ATTR_WRITE: {
		uid_t	euid;
		uid_t	ruid;
		gid_t	egid;
		gid_t	rgid;
		mode_t	mask;
		int	glist_len;

		if (file_major == 1) {
			struct sat_proc_own_attr_write_1_0 *	paw_1_0;
			paw_1_0 = (struct sat_proc_own_attr_write_1_0 *) body;
			body+=ALIGN(sizeof(struct sat_proc_own_attr_write_1_0));
			euid = paw_1_0->newattr.uid.sat_euid;
			ruid = paw_1_0->newattr.uid.sat_ruid;
			egid = paw_1_0->newattr.gid.sat_egid;
			rgid = paw_1_0->newattr.gid.sat_rgid;
			glist_len = paw_1_0->newattr.sat_glist_len;
		} else {
			struct sat_proc_own_attr_write *	paw;
			paw = (struct sat_proc_own_attr_write *) body;
			body += ALIGN(sizeof(struct sat_proc_own_attr_write));
			euid = paw->newattr.uid.sat_euid;
			ruid = paw->newattr.uid.sat_ruid;
			egid = paw->newattr.gid.sat_egid;
			rgid = paw->newattr.gid.sat_rgid;
			mask = paw->newattr.sat_mask;
			glist_len = paw->newattr.sat_glist_len;
		}
		switch (hdr->sat_syscall) {
		case SAT_SYSCALL_SETUID:
		case SAT_SYSCALL_SETREUID:
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf("New eff. uid     = %s\n",
				    sat_str_user(euid));
				printf("New real uid     = %s\n",
				    sat_str_user(ruid));
				break;
			case OM_LINEAR:
				printf("newreuid:%s,%s ",
				    sat_str_user(ruid), sat_str_user(euid));
				break;
			case OM_SGIF:
				assign_I32dup(OBJRUID, ruid);
				assign_I32dup(OBJEUID, euid);
				break;
			}
			break;
		case SAT_SYSCALL_SETGID:
		case SAT_SYSCALL_SETREGID:
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf("New eff. gid     = %s\n",
				    sat_str_group(egid));
				printf("New real gid     = %s\n",
				    sat_str_group(rgid));
				break;
			case OM_LINEAR:
				printf("newregid:%s,%s ",
				    sat_str_group(rgid), sat_str_group(egid));
				break;
			case OM_SGIF:
				assign_I32dup(OBJRGID, rgid);
				assign_I32dup(OBJEGID, egid);
				break;
			}
			break;
		case SAT_SYSCALL_UMASK:
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf("New umask        = %s\n",
				       sat_str_umask(mask));
				break;
			case OM_LINEAR:
				printf("newumask:%s ", sat_str_umask(mask));
				break;
			case OM_SGIF:
				assign_I32dup(OBJUMASK, (__int32_t)mask);
				break;
			}
			break;
		case SAT_SYSCALL_SYSSGI:
			switch (hdr->sat_subsyscall) {
			case SGI_SETPLABEL:
				switch (brief) {
				case OM_VERBOSE:
				case OM_BRIEF:
					printf ("New process lbl. = %s\n",
					    sat_str_label((mac_t)body));
					break;
				case OM_LINEAR:
					printf ("newplabel:%s ",
					    sat_str_label((mac_t)body));
					break;
				case OM_SGIF:
					if (mac_enabled)
						assign_maclbldup(FILE0LABEL, (mac_t)body, mac_size((mac_t)body));
					break;
				}
			 case SGI_SETGROUPS:
				switch (brief) {
				case OM_VERBOSE:
				case OM_BRIEF:
					if (file_major == 1) {
						o_gid_t *glist = (o_gid_t *)body;
						for (i=0; i < glist_len; i++) {
							printf ("New grp list ent = %s\n",
							    sat_str_group(glist[i]));
						}
					} else {
						gid_t *glist = (gid_t *)body;
						for (i=0; i < glist_len; i++) {
							printf ("New grp list ent = %s\n",
							    sat_str_group(glist[i]));
						}
					}
					break;
				case OM_LINEAR:
					if (file_major == 1) {
						o_gid_t *glist = (o_gid_t *)body;
						if (glist_len > 0)
							printf("ngrouple:%s",
							    sat_str_group(glist[0]));
						for (i=1; i < glist_len; i++) {
							printf (",%s",
							    sat_str_group(glist[i]));
						}
						if (glist_len > 0)
							printf(" ");
					} else {
						gid_t *glist = (gid_t *)body;
						if (glist_len > 0)
							printf("ngrouple:%s",
							    sat_str_group(glist[0]));
						for (i=1; i < glist_len; i++) {
							printf (",%s",
							    sat_str_group(glist[i]));
						}
						if (glist_len > 0)
							printf(" ");
					}
					break;
				case OM_SGIF:
					if (file_major == 1) {
						o_gid_t *glist = (o_gid_t *)body;
						if (glist_len > 0) {
							assign_I32dup(OBJGLIST_LEN, glist_len);
							assign_I16Adup(OBJOGLIST, (short *)glist, glist_len);

						}
					} else {
						gid_t *glist = (gid_t *)body;
						if (glist_len > 0){
							assign_I32dup(OBJGLIST_LEN, glist_len);
							assign_I32Adup(OBJGLIST, (__int32_t *)glist, glist_len);
						}
					}
					break;
				}
				break;
			}
			break;
		}
		}
		break;

	case SAT_PROC_OWN_EXT_ATTR_WRITE: {
		int which;
		if (file_major == 1) {
			break;
		} else {
			struct sat_proc_own_ext_attr_write *paw;
			paw = (struct sat_proc_own_ext_attr_write *) body;
			body += ALIGN(sizeof(struct sat_proc_own_ext_attr_write));
			which = paw->which;
		}
		if (hdr->sat_syscall == SAT_SYSCALL_SYSSGI) {
		    switch (hdr->sat_subsyscall) {
			case SGI_PROC_ATTR_SET:
			    switch (which) {
				case SAT_PROC_EXT_ATTR_LABEL:
				    switch (brief) {
					case OM_VERBOSE:
					case OM_BRIEF:
					    printf ("New process lbl. = %s\n",
						    sat_str_label((mac_t)body));
					    break;
					case OM_LINEAR:
					    printf ("newplabel:%s ",
						    sat_str_label((mac_t)body));
					    break;
					case OM_SGIF:
					    if (mac_enabled)
						assign_maclbldup(FILE0LABEL, (mac_t)body, mac_size((mac_t)body));
					    break;
				    }
				    break;
				case SAT_PROC_EXT_ATTR_CAPABILITY:
				    switch (brief) {
					case OM_VERBOSE:
					case OM_BRIEF:
					    printf ("New process cap  = %s\n",
						    sat_str_cap((cap_t)body));
					    break;
					case OM_LINEAR:
					    printf ("newpcap:%s ",
						    sat_str_cap((cap_t)body));
					    break;
					case OM_SGIF:
					    if (cap_enabled)
						assign_capdup(FILE0CAP, (cap_t)body, cap_size((cap_t)body));
					    break;
				    }
				    break;
			    }
			}
		}
	        }
		break;
	    case SAT_SVIPC_ACCESS: {
		struct sat_svipc_access *	svac;
		svac = (struct sat_svipc_access *) body;

		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("IPC identifier   = %d\n",  svac->sat_svipc_id);
			if (svac->sat_label_size)
				printf ("Label            = %s\n",
					sat_str_label((mac_t)(svac+1)));
			else
				printf ("Permissions      = %#o\n",
					svac->sat_svipc_perm);
			break;
		case OM_LINEAR:
			printf ("ipcid:%d,", svac->sat_svipc_id);
			if (svac->sat_label_size)
				printf ("label:%s ",
					sat_str_label((mac_t)(svac+1)));
			else
				printf ("mode:%#o ", svac->sat_svipc_perm);
			break;
		case OM_SGIF:
			assign_I32dup(SVIPCID, svac->sat_svipc_id);
			assign_I32dup(FILE0MODE, svac->sat_svipc_perm);
			if (svac->sat_label_size && mac_enabled)
				assign_maclbldup(FILE0LABEL, (mac_t)(svac+1), svac->sat_label_size);
			break;
		}
		}
		break;

	case SAT_SVIPC_CREATE: {
		struct sat_svipc_create *	svcr;
		svcr = (struct sat_svipc_create *) body;
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("IPC identifier   = %d\n", svcr->sat_svipc_id);
			printf("IPC key          = %#x\n",svcr->sat_svipc_key);
			printf("Permissions      = %#o\n",svcr->sat_svipc_perm);
			break;
		case OM_SGIF:
			assign_I32dup(SVIPCID, svcr->sat_svipc_id);
			assign_I32dup(SVIPCKEY, svcr->sat_svipc_key);
			assign_I32dup(FILE0MODE, svcr->sat_svipc_perm);
			break;
		case OM_LINEAR:
			printf("ipcid:%d ",  svcr->sat_svipc_id);
			printf("key:%#x ", svcr->sat_svipc_key);
			printf("mode:%#o ", svcr->sat_svipc_perm);
			break;
		}

		}
		break;

	case SAT_SVIPC_REMOVE: {
		struct sat_svipc_remove *	svrm;
		svrm = (struct sat_svipc_remove *) body;
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("IPC identifier   = %d\n",  svrm->sat_svipc_id);
			break;
		case OM_SGIF:
			assign_I32dup(SVIPCID, svrm->sat_svipc_id);
			break;
		case OM_LINEAR:
			printf("ipcid:%d ",  svrm->sat_svipc_id);
			break;
		}
		}
		break;

	case SAT_SVIPC_CHANGE: {
		int	id;
		uid_t	oldown;
		gid_t	oldgrp;
		int	oldperm;
		uid_t	newown;
		gid_t	newgrp;
		int	newperm;

		if (file_major == 1) {
			struct sat_svipc_change_1_0 *	svch_1_0;
			svch_1_0 = (struct sat_svipc_change_1_0 *) body;
			id = svch_1_0->sat_svipc_id;
			oldown = svch_1_0->sat_svipc_oldown;
			oldgrp = svch_1_0->sat_svipc_oldgrp;
			oldperm = svch_1_0->sat_svipc_oldperm;
			newown = svch_1_0->sat_svipc_newown;
			newgrp = svch_1_0->sat_svipc_newgrp;
			newperm = svch_1_0->sat_svipc_newperm;
		} else {
			struct sat_svipc_change *	svch;
			svch = (struct sat_svipc_change *) body;
			id = svch->sat_svipc_id;
			oldown = svch->sat_svipc_oldown;
			oldgrp = svch->sat_svipc_oldgrp;
			oldperm = svch->sat_svipc_oldperm;
			newown = svch->sat_svipc_newown;
			newgrp = svch->sat_svipc_newgrp;
			newperm = svch->sat_svipc_newperm;
		}
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("IPC identifier   = %d\n",id);
			printf("Old owner        = %s\n",sat_str_user(oldown));
			printf("Old group        = %s\n",sat_str_group(oldgrp));
			printf("Old permissions  = %#o\n",oldperm);
			printf("New owner        = %s\n",sat_str_user(newown));
			printf("New group        = %s\n",sat_str_group(newgrp));
			printf("New permissions  = %#o\n",newperm);
			break;
		case OM_SGIF:
			assign_I32dup(SVIPCID, id);
			assign_I32dup(FILE0OWN, oldown);
			assign_I32dup(FILE0GRP, oldgrp);
			assign_I32dup(FILE0MODE, oldperm);
			assign_I32dup(FILE1OWN, newown);
			assign_I32dup(FILE1GRP, newgrp);
			assign_I32dup(FILE1MODE, newperm);
			break;
		case OM_LINEAR:
			printf("ipcid:%d ",id);
			printf("oldogp:%s,",sat_str_user(oldown));
			printf("%s,",sat_str_group(oldgrp));
			printf("%#o ",oldperm);
			printf("newogp:%s,",sat_str_user(newown));
			printf("%s,",sat_str_group(newgrp));
			printf("%#o ",newperm);
			break;
		}
		}
		break;

	case SAT_BSDIPC_CREATE:
		if (file_major == 1 && file_minor <= 1) {
			struct sat_bsdipc_create_1_1 * bscr;
			bscr = (struct sat_bsdipc_create_1_1 *)body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				sat_print_socket_id_desc(bscr->sat_socket,
						 bscr->sat_socket_dscr);
				sat_print_domain(bscr->sat_comm_domain);
			    	printf ("Socket type      = %d (%s)\n", 
					bscr->sat_protocol,
					sat_str_socktype (bscr->sat_protocol));
				break;
			case OM_LINEAR:
				sat_print_socket_id_desc(bscr->sat_socket,
						 bscr->sat_socket_dscr);
				sat_print_domain(bscr->sat_comm_domain);				
				printf ("soctype:%s ", 
					sat_str_socktype (bscr->sat_protocol));
				break;
			case OM_SGIF:
				assign_I32dup(SOCKTYPE, bscr->sat_protocol);
				assign_I32dup(SOCKID, (int)bscr->sat_socket);
				assign_I32dup(SOCKDSCR, bscr->sat_socket_dscr);
				assign_I32dup(COMMDOMAIN, bscr->sat_comm_domain);
				break;
			}				

		} else if (file_major >= 2 || (file_major == 1 && file_minor == 2)) {
			struct sat_bsdipc_create * bscr;
			bscr = (struct sat_bsdipc_create *) body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				sat_print_socket_id_desc(bscr->sat_socket,
						 bscr->sat_socket_dscr);
				sat_print_domain(bscr->sat_comm_domain);
			    	printf ("Socket type      = %d (%s)\n", 
					bscr->sat_protocol,
					sat_str_socktype (bscr->sat_protocol));
				break;
			case OM_LINEAR:
				sat_print_socket_id_desc(bscr->sat_socket,
						 bscr->sat_socket_dscr);
				sat_print_domain(bscr->sat_comm_domain);
				printf ("soctype:%s ", 
					sat_str_socktype (bscr->sat_protocol));
				break;
			case OM_SGIF:
				assign_I32dup(SOCKTYPE, bscr->sat_protocol);
				assign_I32dup(SOCKID, (int)bscr->sat_socket);
				assign_I32dup(SOCKDSCR, bscr->sat_socket_dscr);
				assign_I32dup(COMMDOMAIN, bscr->sat_comm_domain);
				break;
			}
			body += sizeof(struct sat_bsdipc_create); 
			sat_print_socket_dac(bscr->sat_so_uid,
				bscr->sat_so_rcvuid, 
				bscr->sat_so_uidcount, (int *)body, 0);

		}
		break;

	case SAT_BSDIPC_CREATE_PAIR: 

		if (file_major == 1 && file_minor <= 1) {
			struct sat_bsdipc_create_pair_1_1 *	bscp;
			bscp = (struct sat_bsdipc_create_pair_1_1 *) body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				sat_print_socket_id_desc(bscp->sat_socket,
						 bscp->sat_socket_dscr);
			    	printf ("Second id, desc. = %08X, %d\n", 
					bscp->sat_second_socket, 
					bscp->sat_second_dscr);
				sat_print_domain(bscp->sat_comm_domain);
			    	printf ("Protocol         = %d (%s)\n", 
					bscp->sat_protocol,
					sat_str_socktype (bscp->sat_protocol));
				break;
			case OM_LINEAR:
				sat_print_socket_id_desc(bscp->sat_socket,
						 bscp->sat_socket_dscr);
			    	printf("2nd");
				sat_print_socket_id_desc(
					 bscp->sat_second_socket,
					 bscp->sat_second_dscr);
				sat_print_domain(bscp->sat_comm_domain);
				printf ("protocol:%s ", 
					sat_str_socktype (bscp->sat_protocol));
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)bscp->sat_socket);
				assign_I32dup(SOCKDSCR, bscp->sat_socket_dscr);
				assign_I32dup(SOCKID1, (int)bscp->sat_second_socket);
				assign_I32dup(SOCKDSCR1, bscp->sat_second_dscr);
				assign_I32dup(COMMDOMAIN, bscp->sat_comm_domain);
				assign_I32dup(SOCKTYPE, bscp->sat_protocol);
				break;
			}
		} else if (file_major >= 2 || (file_major == 1 && file_minor == 2)) {
		    	struct sat_bsdipc_create_pair *	bscp;
			bscp = (struct sat_bsdipc_create_pair *) body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				sat_print_socket_id_desc(bscp->sat_socket,
						 bscp->sat_socket_dscr);
			    	printf ("Second id, desc. = %08X, %d\n", 
					bscp->sat_second_socket, 
					bscp->sat_second_dscr);
				sat_print_domain(bscp->sat_comm_domain);
			    	printf ("Protocol         = %d (%s)\n", 
					bscp->sat_protocol,
					sat_str_socktype (bscp->sat_protocol));
				break;
			case OM_LINEAR:
				sat_print_socket_id_desc(bscp->sat_socket,
						 bscp->sat_socket_dscr);
			    	printf("2nd");
				sat_print_socket_id_desc(
					 bscp->sat_second_socket,
					 bscp->sat_second_dscr);
				sat_print_domain(bscp->sat_comm_domain);
				printf ("protocol:%s ", 
					sat_str_socktype (bscp->sat_protocol));
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)bscp->sat_socket);
				assign_I32dup(SOCKDSCR, bscp->sat_socket_dscr);
				assign_I32dup(SOCKID1, (int)bscp->sat_second_socket);
				assign_I32dup(SOCKDSCR1, bscp->sat_second_dscr);
				assign_I32dup(COMMDOMAIN, bscp->sat_comm_domain);
				assign_I32dup(SOCKTYPE, bscp->sat_protocol);
				break;
			}
			body += sizeof(struct sat_bsdipc_create_pair); 
			sat_print_socket_dac(bscp->sat_so_uid,
				bscp->sat_so_rcvuid, 
			     	bscp->sat_so_uidcount, (int *)body, 0);
			if (bscp->sat_so_uidcount > 0 &&
			    bscp->sat_so_uidcount != WILDACL)
				body += (sizeof(int) * bscp->sat_so_uidcount); 
			sat_print_socket_dac(bscp->sat_second_so_uid,
				bscp->sat_second_so_rcvuid, 
				bscp->sat_second_so_uidcount, (int *)body, 1);

		}	
		break;

	case SAT_BSDIPC_SHUTDOWN:

		if (file_major == 1 && file_minor <= 1) {
			struct sat_bsdipc_shutdown_1_1 *bsde;
			bsde = (struct sat_bsdipc_shutdown_1_1 *) body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				sat_print_socket_id_desc(bsde->sat_socket,
						 bsde->sat_socket_dscr);
				printf ("Shutdown how     = %d\n",  
					bsde->sat_how);
				break;
			case OM_LINEAR:
				sat_print_socket_id_desc(bsde->sat_socket,
						 bsde->sat_socket_dscr);
				printf ("shutdown:%d ", bsde->sat_how);
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)bsde->sat_socket);
				assign_I32dup(SOCKDSCR, bsde->sat_socket_dscr);
				break;
			}

		} else if (file_major >= 2 || (file_major == 1 && file_minor == 2)) {
			struct sat_bsdipc_shutdown *	bsde;
			bsde = (struct sat_bsdipc_shutdown *) body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				sat_print_socket_id_desc(bsde->sat_socket,
						 bsde->sat_socket_dscr);
				printf ("Shutdown how     = %d\n",  
					bsde->sat_how);
				break;
			case OM_LINEAR:
				sat_print_socket_id_desc(bsde->sat_socket,
						 bsde->sat_socket_dscr);
				printf ("shutdown:%d ", bsde->sat_how);
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)bsde->sat_socket);
				assign_I32dup(SOCKDSCR, bsde->sat_socket_dscr);
				break;
			}
			body += sizeof(struct sat_bsdipc_shutdown); 
			sat_print_socket_dac(bsde->sat_so_uid,
				bsde->sat_so_rcvuid, 
				bsde->sat_so_uidcount, (int *)body, 0);

		}
		break;

	case SAT_BSDIPC_MAC_CHANGE: 

		if (file_major == 1 && file_minor <= 1) {
			struct sat_bsdipc_mac_change_1_1 *bsmc;
			bsmc = (struct sat_bsdipc_mac_change_1_1 *) body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				sat_print_socket_id_desc(bsmc->sat_socket,
						 bsmc->sat_socket_dscr);
				printf ("Socket label     = %s\n",
					sat_str_label((mac_t)(bsmc+1)));
				break;
			case OM_LINEAR:
				sat_print_socket_id_desc(bsmc->sat_socket,
						 bsmc->sat_socket_dscr);
				printf ("solabel:%s ",
					sat_str_label((mac_t)(bsmc+1)));
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)bsmc->sat_socket);
				assign_I32dup(SOCKDSCR, bsmc->sat_socket_dscr);				
				if (mac_enabled)
					assign_maclbldup(FILE0LABEL, (mac_t)(bsmc+1), mac_size((mac_t)(bsmc+1)));
				break;
			}

		} else if (file_major >= 2 || (file_major == 1 && file_minor == 2)) {
			struct sat_bsdipc_mac_change *bsmc;
			bsmc = (struct sat_bsdipc_mac_change *) body;
			body += sizeof(struct sat_bsdipc_mac_change);
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				sat_print_socket_id_desc(bsmc->sat_socket,
						 bsmc->sat_socket_dscr);
				printf ("Socket label     = %s\n",
					sat_str_label((mac_t)(body)));
				break;
			case OM_LINEAR:
				sat_print_socket_id_desc(bsmc->sat_socket,
						 bsmc->sat_socket_dscr);
				printf ("solabel:%s ",
					sat_str_label((mac_t)(body)));
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)bsmc->sat_socket);
				assign_I32dup(SOCKDSCR, bsmc->sat_socket_dscr);				
				if (mac_enabled)
					assign_maclbldup(FILE0LABEL, (mac_t)(bsmc+1), mac_size((mac_t)(bsmc+1)));
				break;
			}
			body += bsmc->sat_label_size;
			sat_print_socket_dac(bsmc->sat_so_uid,
				bsmc->sat_so_rcvuid, 
				bsmc->sat_so_uidcount, (int *)(body), 0);

		}
		break;

	case SAT_BSDIPC_DAC_CHANGE: {

		struct sat_bsdipc_dac_change *bsdc;
		bsdc = (struct sat_bsdipc_dac_change *) body;
		if (brief != OM_SGIF)
			sat_print_socket_id_desc(bsdc->sat_socket,
						 bsdc->sat_socket_dscr);
		else {
			assign_I32dup(SOCKID, (int)bsdc->sat_socket);
			assign_I32dup(SOCKDSCR, bsdc->sat_socket_dscr);
		}
		body += sizeof(struct sat_bsdipc_dac_change); 
		sat_print_socket_dac(bsdc->sat_so_uid,
			bsdc->sat_so_rcvuid, bsdc->sat_so_uidcount, (int *)body, 0);
		}
		break;

	case SAT_BSDIPC_ADDRESS: 

		if (file_major == 1 && file_minor <= 1) {
			struct sat_bsdipc_address_1_1 *bsea;
			bsea = (struct sat_bsdipc_address_1_1 *)body;
			if (brief != OM_SGIF)
				sat_print_socket_id_desc(bsea->sat_socket,
						 bsea->sat_socket_dscr);
			else {
				assign_I32dup(SOCKID, (int)bsea->sat_socket);
				assign_I32dup(SOCKDSCR, bsea->sat_socket_dscr);
			}
			body += sizeof (struct sat_bsdipc_address_1_1);
			sat_print_sockaddr (bsea->sat_addr_len, body);

		} else if (file_major >= 2 || (file_major == 1 && file_minor == 2)) {
			struct sat_bsdipc_address *bsea;
			bsea = (struct sat_bsdipc_address *)body;
			if (brief != OM_SGIF)
				sat_print_socket_id_desc(bsea->sat_socket,
						 bsea->sat_socket_dscr);
			else {
				assign_I32dup(SOCKID, (int)bsea->sat_socket);
				assign_I32dup(SOCKDSCR, bsea->sat_socket_dscr);
			}
			body += sizeof (struct sat_bsdipc_address);
			sat_print_sockaddr (bsea->sat_addr_len, body);
			body += bsea->sat_addr_len;
			sat_print_socket_dac(bsea->sat_so_uid,
				bsea->sat_so_rcvuid, 
				bsea->sat_so_uidcount, (int *)body, 0);

		}
		break;

	case SAT_BSDIPC_RESVPORT: 

		if (file_major == 1 && file_minor <= 1) {
			struct sat_bsdipc_resvport_1_1 *bsrp;
			bsrp = (struct sat_bsdipc_resvport_1_1 *) body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				sat_print_socket_id_desc(bsrp->sat_socket,
						 bsrp->sat_socket_dscr);
				printf ("Reserved port    = %d\n",  
					bsrp->sat_port);
				break;
			case OM_LINEAR:
				sat_print_socket_id_desc(bsrp->sat_socket,
						 bsrp->sat_socket_dscr);
				printf ("resport:%d ", bsrp->sat_port);
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)bsrp->sat_socket);
				assign_I32dup(SOCKDSCR, bsrp->sat_socket_dscr);
				assign_I32dup(PORT, bsrp->sat_port);
				break;
			}

		} else if (file_major >= 2 || (file_major == 1 && file_minor == 2)) {
			struct sat_bsdipc_resvport *bsrp;
			bsrp = (struct sat_bsdipc_resvport *) body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				sat_print_socket_id_desc(bsrp->sat_socket,
						 bsrp->sat_socket_dscr);
				printf ("Reserved port    = %d\n",  
					bsrp->sat_port);
				break;
			case OM_LINEAR:
				sat_print_socket_id_desc(bsrp->sat_socket,
						 bsrp->sat_socket_dscr);
				printf ("resport:%d ", bsrp->sat_port);
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)bsrp->sat_socket);
				assign_I32dup(SOCKDSCR, bsrp->sat_socket_dscr);
				assign_I32dup(PORT, bsrp->sat_port);
				break;
			}
			body += sizeof(struct sat_bsdipc_resvport); 
			sat_print_socket_dac(bsrp->sat_so_uid,
				bsrp->sat_so_rcvuid, 
				bsrp->sat_so_uidcount, (int *)body, 0);

		}
		break;

	case SAT_BSDIPC_DELIVER:
	case SAT_BSDIPC_CANTFIND: 
	case SAT_BSDIPC_DAC_DENIED: 

		if (file_major == 1 && file_minor <= 1) {
			struct sat_bsdipc_match_1_1 *smtch;
			smtch = (struct sat_bsdipc_match_1_1 *)body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf ("Socket id        = %08X\n", 
					smtch->sat_socket);
				break;
			case OM_LINEAR:
				printf ("sd:%08X ", smtch->sat_socket);
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)smtch->sat_socket);
				break;
			}
			body += sizeof(*smtch);
			sat_print_tcpiphdr( body, smtch->sat_ip_len );
			body += smtch->sat_ip_len;
			sat_print_datagram_label((mac_t)(body));

		} else if (file_major >= 2 || (file_major == 1 && file_minor == 2)) {
			struct sat_bsdipc_match *smtch;
			smtch = (struct sat_bsdipc_match *)body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf ("Socket id        = %08X\n", 
					smtch->sat_socket);
				break;
			case OM_LINEAR:
				printf ("sd:%08X ", smtch->sat_socket);
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)smtch->sat_socket);
				break;
			}
			body += sizeof(*smtch);
			sat_print_tcpiphdr( body, smtch->sat_ip_len );
			body += smtch->sat_ip_len;
			if (brief == OM_SGIF) {
				if (mac_enabled)
					assign_maclbldup(DGLABEL, (mac_t)(body), mac_size((mac_t)body));
				assign_I32dup(DGUID,smtch->sat_uid);
	
			} else {
				sat_print_datagram_label((mac_t)(body));
				sat_print_datagram_uid(smtch->sat_uid);
			}
			if (hdr->sat_rectype == SAT_BSDIPC_DELIVER ||
			    hdr->sat_rectype == SAT_BSDIPC_DAC_DENIED) {
				body += smtch->sat_label_len;
				sat_print_socket_dac(smtch->sat_so_uid,
				     smtch->sat_so_rcvuid, 
				     smtch->sat_so_uidcount, (int *)body, 0); 
			}

		}
		break;

	case SAT_BSDIPC_SNOOP_OK:
	case SAT_BSDIPC_SNOOP_FAIL: 

		if (file_major == 1 && file_minor <= 1) {
			struct sat_bsdipc_snoop_1_1 *snoop;
			snoop = (struct sat_bsdipc_snoop_1_1 *)body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf ("Socket id        = %08X\n", 
					snoop->sat_socket);
				break;
			case OM_LINEAR:
				printf ("sd:%08X ", snoop->sat_socket);
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)snoop->sat_socket);
				break;
			}
			body += sizeof (*snoop);
			if (brief == OM_SGIF) {
				if (mac_enabled)
					assign_maclbldup(DGLABEL, (mac_t)(body), mac_size((mac_t)body));
			} else 
				sat_print_datagram_label((mac_t)(body));

		} else if (file_major >= 2 || (file_major == 1 && file_minor == 2)) {
			struct sat_bsdipc_snoop *snoop;
			snoop = (struct sat_bsdipc_snoop *)body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf ("Socket id        = %08X\n", 
					snoop->sat_socket);
				break;
			case OM_LINEAR:
				printf ("sd:%08X ", snoop->sat_socket);
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)snoop->sat_socket);
				break;
			}
			body += sizeof (*snoop);
			if (brief == OM_SGIF) {
				if (mac_enabled)
					assign_maclbldup(DGLABEL, (mac_t)(body), mac_size((mac_t)body));
			} else 
				sat_print_datagram_label((mac_t)(body));
			body += snoop->sat_label_len;
			sat_print_socket_dac(snoop->sat_so_uid,
				snoop->sat_so_rcvuid, 
				snoop->sat_so_uidcount, (int *)body, 0);

		}
		break;

	case SAT_BSDIPC_RX_MISSING: {

		struct sat_bsdipc_missing * miss;
		miss = (struct sat_bsdipc_missing *)body;
		sat_print_ifname( miss->sat_ifname );
		body += sizeof(*miss) + sizeof(u_short);
		sat_print_iphdr( body, miss->sat_ip_len );
		}
		break;

	case SAT_BSDIPC_RX_OK:
	case SAT_BSDIPC_RX_RANGE:
	case SAT_BSDIPC_TX_OK:
	case SAT_BSDIPC_TX_RANGE:
	case SAT_BSDIPC_TX_TOOBIG: 

		if (file_major == 1 && file_minor <= 1) {
			struct sat_bsdipc_range_1_1 *range;
			range = (struct sat_bsdipc_range_1_1 *)body;
			sat_print_ifname( range->sat_ifname );
			body += sizeof(struct sat_bsdipc_range_1_1);
			sat_print_tcpiphdr( body, range->sat_ip_len );
			body += range->sat_ip_len;
			if (brief == OM_SGIF) {
				if (mac_enabled)
					assign_maclbldup(DGLABEL, (mac_t)(body), mac_size((mac_t)body));
			} else 
				sat_print_datagram_label((mac_t)(body));

		} else if (file_major >= 2 || (file_major == 1 && file_minor == 2)) {
			struct sat_bsdipc_range *range;
			range = (struct sat_bsdipc_range *)body;
			sat_print_ifname( range->sat_ifname );
			body += ALIGN(sizeof(struct sat_bsdipc_range));
			sat_print_tcpiphdr( body, range->sat_ip_len );
			body += range->sat_ip_len;
			if (brief == OM_SGIF) {
				if (mac_enabled)
					assign_maclbldup(DGLABEL, (mac_t)(body), mac_size((mac_t)body));
				assign_I32dup(DGUID, range->sat_uid);
			} else {
				sat_print_datagram_label((mac_t)(body));
				sat_print_datagram_uid(range->sat_uid);
			} 
		}
		break;

	case SAT_BSDIPC_IF_CONFIG: {

		struct sat_bsdipc_if_config * cfg;
		cfg = (struct sat_bsdipc_if_config *)body;
		if (brief == OM_SGIF) {
			assign_I32dup(SOCKID, (int)cfg->sat_socket);
			assign_I32dup(SOCKDSCR, cfg->sat_socket_dscr);
		} else
			sat_print_socket_id_desc(cfg->sat_socket,cfg->sat_socket_dscr);
		body += sizeof(*cfg);
		sat_print_ioctl( cfg->sat_ioctl_cmd, 
				 cfg->sat_ifreq_len,
				(struct ifreq *) body);
		}
		break;

	case SAT_BSDIPC_IF_INVALID:
	case SAT_BSDIPC_IF_SETLABEL: {

		struct sat_bsdipc_if_setlabel * iflbl;
		iflbl = (struct sat_bsdipc_if_setlabel *)body;
		if (brief != OM_SGIF) {
			sat_print_socket_id_desc(iflbl->sat_socket,
						 iflbl->sat_socket_dscr);
			sat_print_ifname( iflbl->sat_ifname );
		}
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("DOI, Max/Min auth= %d (0x%08x), %02x/%02x\n",
				iflbl->sat_doi, iflbl->sat_doi, 
				iflbl->sat_authority_max,
				iflbl->sat_authority_min);
			break;
		case OM_LINEAR:
			printf ("DOI/max/min-auth:%d(0x%08x)/%02x/%02x ",
				iflbl->sat_doi, iflbl->sat_doi, 
				iflbl->sat_authority_max,
				iflbl->sat_authority_min);
			break;
		case OM_SGIF:
			assign_I32dup(SOCKID, (int)(iflbl->sat_socket));
			assign_I32dup(SOCKDSCR, iflbl->sat_socket_dscr);
			assign_I32dup(DOI, iflbl->sat_doi);
			assign_I32dup(AUTHORITY_MAX, iflbl->sat_authority_max);
			assign_I32dup(AUTHORITY_MIN, iflbl->sat_authority_min);
			assign_I32dup(IDIOM, iflbl->sat_idiom);
			break;
		}
		if (brief != OM_SGIF)
			sat_print_idiom( iflbl->sat_idiom );
		body += sizeof(*iflbl);
					
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Maximum IF label = %s\n",
			    sat_str_label((mac_t)(body)));
			break;
		case OM_LINEAR:
			printf ("maxiflabel:%s ",
			    sat_str_label((mac_t)(body)));
			break;
		case OM_SGIF:
			if (mac_enabled)
				assign_maclbldup(LABEL_MAX, (mac_t)(body), mac_size((mac_t)body));
			break;
		}
		body += iflbl->sat_maxlabel_len;
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Minimum IF label = %s\n",
			    sat_str_label((mac_t)(body)));
			break;
		case OM_LINEAR:
			printf ("miniflabel:%s ",
			    sat_str_label((mac_t)(body)));
			break;
		case OM_SGIF:
			if (mac_enabled)
				assign_maclbldup(LABEL_MIN, (mac_t)(body), mac_size((mac_t)body));
			break;
		}
		}
		break;

	case SAT_BSDIPC_IF_SETUID: {

		struct sat_bsdipc_if_setuid *ifuid;
		ifuid = (struct sat_bsdipc_if_setuid *)body;
		sat_print_socket_id_desc(ifuid->sat_socket,
		    ifuid->sat_socket_dscr);
		sat_print_ifname( ifuid->sat_ifname );
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Ifuid            = %s\n", 
				sat_str_user(ifuid->sat_newuid) );
			break;
		case OM_LINEAR:
			printf ("Ifuid:%s ",
				sat_str_user(ifuid->sat_newuid) );
			break;
		case OM_SGIF:
			assign_I32dup(SOCKID, (int)(ifuid->sat_socket));
			assign_I32dup(SOCKDSCR, ifuid->sat_socket_dscr);
			assign_I32dup(NEWEUID, ifuid->sat_newuid);
			break;
		}
		}
		break;

	case SAT_CLOCK_SET: {

		struct sat_clock_set *		clck;
		clck = (struct sat_clock_set *) body;
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Time set to      = %s",
			    ctime(&(clck->sat_newtime)));
			break;
		case OM_LINEAR:
			{
			char timeb[80];
			strcpy(timeb, ctime(&(clck->sat_newtime)));
			timeb[strlen(timeb) - 1] = '\0';
			printf ("time:%s ", timeb);
			}
			break;
		case OM_SGIF:
			assign_I32dup(ATIME, clck->sat_newtime); /* time_t */
			break;
		}
		}
		break;

	case SAT_HOSTNAME_SET:
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Hostname set     = %s\n", body);
			break;
		case OM_LINEAR:
			printf ("hostname:%s ", body);
			break;
		case OM_SGIF:
			assign_strdup(HOSTNAME_SET, body, strlen(body));
			break;
		}
		break;

	case SAT_DOMAINNAME_SET:
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Domainname set   = %s\n", body);
			break;
		case OM_LINEAR:
			printf ("domain:%s ", body);
			break;
		case OM_SGIF:
			assign_strdup(DOMAINNAME_SET, body, strlen(body));
			break;
		}
		break;

	case SAT_HOSTID_SET: {

		struct sat_hostid_set *		hostid;
		hostid = (struct sat_hostid_set *) body;
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Hostid set       = %#x\n",
			    hostid->sat_newhostid);
			break;
		case OM_LINEAR:
			printf ("hostid:%#x ", hostid->sat_newhostid);
			break;
		case OM_SGIF:
			assign_strdup(HOSTID_SET, body, strlen(body));
			break;
		}
		}
		break;

	case SAT_CHECK_PRIV: {
		struct sat_check_priv *		priv;
		priv = (struct sat_check_priv *) body;
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf ("Privilege state  = %s\n",
			    (priv->sat_priv_state) ?
			    "Superuser": "Not superuser");
			break;
		case OM_LINEAR:
			break;
		case OM_SGIF:
			assign_I32dup(PRIVSTATE, priv->sat_priv_state);
			break;
		}
		}
		break;

	case SAT_CONTROL:
		sat_print_sat_control (body);
		break;
	case SAT_PROC_ACCT: {
		struct sat_proc_acct *pa;
		struct acct_timers *timers;
		struct acct_counts *counts;
		char timebuf[26];
		pa = (struct sat_proc_acct *) body;
		timers = (struct acct_timers *) (pa + 1);
		counts = (struct acct_counts *) (timers + 1);
		ctime_r(&pa->sat_btime, timebuf);
		timebuf[24] = '\0';	/* remove trailing newline */
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("Acct version     = %d\n", pa->sat_version);
			printf("Flags            = ");
			if (pa->sat_flag == 0)
			    printf("<none>\n");
			else {
				if (pa->sat_flag & SPASF_FORK)
				    printf("FORK ");
				if (pa->sat_flag & SPASF_SU)
				    printf("SU ");
				if (pa->sat_flag & SPASF_SESSEND)
				    printf("SESSEND ");
				printf("\n");
			}
			printf("Nice value       = %d\n", pa->sat_nice);
			printf("Scheduling parm  = %d\n", pa->sat_sched);
			printf("Arsess handle    = %lld\n", pa->sat_ash);
			printf("Project ID       = %lld\n", pa->sat_prid);
			printf("Start time       = %s\n", timebuf);
			printf("Elapsed hz       = %d\n", pa->sat_etime);
			break;
		case OM_LINEAR:
			printf("acct vers:%d flags:%x nice:%d sched:%d ",
			       pa->sat_version, pa->sat_flag,
			       pa->sat_nice, pa->sat_sched);
			printf("ash:%lld prid:%lld start:%s elapsed:%d",
			       pa->sat_ash, pa->sat_prid, timebuf,
			       pa->sat_etime);

			break;
		}
#ifdef LATER
		SGIF stuff
#endif
		sat_print_acct_timers(timers);
		sat_print_acct_counts(counts);
		}
		break;
	case SAT_SESSION_ACCT: {
		struct sat_session_acct *sa;
		struct acct_spi *spi;
		struct acct_timers *timers;
		struct acct_counts *counts;
		char timebuf[26];
		sa = (struct sat_session_acct *) body;
		spi = (struct acct_spi *) (sa + 1);
		timers = (struct acct_timers *) (spi + 1);
		counts = (struct acct_counts *) (timers + 1);
		ctime_r(&sa->sat_btime, timebuf);
		timebuf[24] = '\0';	/* remove trailing newline */
		switch (brief) {
		case OM_VERBOSE:
		case OM_BRIEF:
			printf("Acct version     = %d\n", sa->sat_version);
			printf("Init nice value  = %d\n", sa->sat_nice);
			printf("Arsess handle    = %lld\n", sa->sat_ash);
			printf("Project ID       = %lld\n", sa->sat_prid);
			printf("Start time       = %s\n", timebuf);
			printf("Elapsed hz       = %d\n", sa->sat_etime);
			printf("S.P. Info:\n");
			printf("  Company        = %s\n", spi->spi_company);
			printf("  Initiator      = %s\n", spi->spi_initiator);
			printf("  Origin         = %s\n", spi->spi_origin);
			printf("  Other info     = %s\n", spi->spi_spi);
			break;
		case OM_LINEAR:
			printf("acct vers:%d nice:%d ",
			       sa->sat_version, sa->sat_nice);
			printf("ash:%lld prid:%lld start:%s elapsed:%d ",
			       sa->sat_ash, sa->sat_prid, timebuf,
			       sa->sat_etime);
			printf("SPI:%s,%s,%s,%s ",
			       spi->spi_company, spi->spi_initiator,
			       spi->spi_origin, spi->spi_spi);
			break;
#ifdef LATER
		SGIF stuff
#endif
		}
		sat_print_acct_timers(timers);
		sat_print_acct_counts(counts);
		}
		break;
	case SAT_AE_LP:
	case SAT_AE_AUDIT:
	case SAT_AE_IDENTITY:
	case SAT_AE_CUSTOM:
	case SAT_AE_MOUNT:
	case SAT_AE_DBEDIT:
	case SAT_AE_X_ALLOWED:
	case SAT_AE_X_DENIED:
		sat_print_ae(hdr->sat_rectype, body);
		break;

	/**** New in version 2.2 ****/

	case SAT_SVR4NET_CREATE: 

		if (file_major >= 2) {
			struct sat_svr4net_create * bscr;
			bscr = (struct sat_svr4net_create *) body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
			    	sat_print_socket_id_desc(bscr->sat_socket,
						 bscr->sat_socket_dscr);
				sat_print_domain(bscr->sat_comm_domain);
				printf ("Socket type      = %d (%s)\n", 
					bscr->sat_protocol,
					sat_str_socktype (bscr->sat_protocol));
				break;
			case OM_LINEAR:
			    	sat_print_socket_id_desc(bscr->sat_socket,
						 bscr->sat_socket_dscr);
				sat_print_domain(bscr->sat_comm_domain);
				printf ("soctype:%s ", 
					sat_str_socktype (bscr->sat_protocol));
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)(bscr->sat_socket));
				assign_I32dup(SOCKDSCR, bscr->sat_socket_dscr);
				assign_I32dup(COMMDOMAIN, bscr->sat_comm_domain);
				assign_I32dup(SOCKTYPE, bscr->sat_protocol);
				break;
			}
			body += sizeof(struct sat_svr4net_create); 
			sat_print_socket_dac(bscr->sat_so_uid,
				bscr->sat_so_rcvuid, 
				bscr->sat_so_uidcount, (int *)body, 0);
    		}
		break;

	case SAT_SVR4NET_ADDRESS: 

		if (file_major >= 2) {
			struct sat_svr4net_address *bsea;
			bsea = (struct sat_svr4net_address *)body;
			if (brief == OM_SGIF) {
				assign_I32dup(SOCKID, (int)(bsea->sat_socket));
				assign_I32dup(SOCKDSCR, bsea->sat_socket_dscr);				
			} else 
				sat_print_socket_id_desc(bsea->sat_socket,
							 bsea->sat_socket_dscr);			
			body += sizeof (struct sat_svr4net_address);
			sat_print_sockaddr (bsea->sat_addr_len, body);
			body += bsea->sat_addr_len;
			sat_print_socket_dac(bsea->sat_so_uid,
				bsea->sat_so_rcvuid, 
				bsea->sat_so_uidcount, (int *)body, 0);
    		}
		break;

	case SAT_SVR4NET_SHUTDOWN:

		if (file_major >= 2) {
			struct sat_svr4net_shutdown * bsde;
			bsde = (struct sat_svr4net_shutdown *) body;
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				sat_print_socket_id_desc(bsde->sat_socket,
						 bsde->sat_socket_dscr);
				printf ("Shutdown how     = %d\n",  
					bsde->sat_how);
				break;
			case OM_LINEAR:
				sat_print_socket_id_desc(bsde->sat_socket,
						 bsde->sat_socket_dscr);
				printf ("shutdown:%d ", bsde->sat_how);
				break;
			case OM_SGIF:
				assign_I32dup(SOCKID, (int)(bsde->sat_socket));
				assign_I32dup(SOCKDSCR, bsde->sat_socket_dscr);					assign_I32dup(SHUTDOWN, bsde->sat_how);						break;
			}
			body += sizeof(struct sat_svr4net_shutdown); 
			sat_print_socket_dac(bsde->sat_so_uid,
				bsde->sat_so_rcvuid, 
				bsde->sat_so_uidcount, (int *)body, 0);
		}
		break;

	case SAT_SYS_NOTE:

		/* There is a string length in the record, but for 
		   the time being it is not used here. */
		if (file_major >= 2) {
			/* struct sat_sys_note * note;
			 * note = (struct sat_sys_note *) body;		
			 */
			body += ALIGN(sizeof(struct sat_sys_note));
			switch (brief) {
			case OM_VERBOSE:
			case OM_BRIEF:
				printf ("Note             = %s\n", body);
				break;
			case OM_LINEAR:
				printf ("Note:%s ", body);
				break;
			case OM_SGIF:
				assign_strdup(STRING, body, strlen(body));	
				break;
			}
		}
		break;

	default:
		/* already printed by eventname	*/
		if (brief != OM_SGIF)
			printf ("-- body not done --\n");
		break;
	}
	/*
	 * OM_LINEAR wants the newline here.
	 */
	if (brief == OM_LINEAR)
		printf("\n");
}

#ifndef REAL
	struct sat_file_info	finfo;	/* audit file header */
#endif /* REAL  */
int
main(int argc, char *argv[])
{
	int	size;
	int	rec_count = 0;
	int	i;
	FILE *	in;
	char *	filename;
	char *	timezone;	/* override audit file timezone */
	int	c;		/* getopt */
	extern	int optind;	/* getopt */
	char 	*sgifbuf;	/* to hold SGIF format record */

	static char		body[MAX_REC_SIZE];
#ifdef REAL
	struct sat_file_info	finfo;	/* audit file header */
#endif /* REAL  */
	struct sat_hdr_info	hinfo;	/* record header */
	int			hdr_mask; /* what we want from the hdr */

	timezone = NULL;

	while ((c = getopt(argc, argv, "bdflanz:")) != -1) {
		switch (c) {
		case 'b':
			brief = OM_BRIEF;
			break;
		case 'd':
			debug = SAT_TRUE;
			setlinebuf(stderr);
			setlinebuf(stdout);
			break;
		case 'f':
			fdmap = 1;
			break;
		case 'l':
			brief = OM_LINEAR;
			break;
		case 'a':
			/*
			 * Output SGIF binary format
			 * (SGIF) records, not ASCII.
			 */
			brief = OM_SGIF;
			break;
		case 'n':
			normalize = 1;
			break;
		case 'z':
			timezone = optarg;
			break;
		default:
			fprintf(stderr,
			    "usage: %s [-bdl] [-z timezone] [filename]\n",
			    argv[0]);
			exit(1);
		}
	}

	/* check for an optional input file in the command line */
	if (optind < argc) {
		filename = argv[optind];
		in = fopen( filename, "r" );
		if (in == NULL) {
			fprintf (stderr, "Cannot open %s for input: ",
			    filename);
			perror(NULL);
			exit(1);
		}
	} else {
		in = stdin;
		filename = "<stdin>";
	}

	/*
	 * If invoked with "-a", by root, and in == stdin, turn
	 * off most audit events, as we are probably filtering satd output.
	 */
	if (brief == OM_SGIF && in == stdin && getuid() == 0) {
		cap_t ocap;
		cap_value_t cap_audit_control[] = {CAP_AUDIT_CONTROL};

		ocap = cap_acquire (1, cap_audit_control);
		for (i=0; i<SAT_NTYPES; i++) {
			if (i == SAT_EXIT || i == SAT_EXEC || i == SAT_FORK)
				continue;
			(void)syssgi(SGI_SATCTL, SATCTL_LOCALAUDIT_OFF, i, 0);
		}
		cap_surrender (ocap);
	}
	 
	
	/* clear users and groups lists, if necessary */

	if (users) {
		for (i=0; users[i]; i++)
			free(users[i]);
		free(users);
	}
	if (groups) {
		for (i=0; groups[i]; i++)
			free(groups[i]);
		free(groups);
	}

	/* read file header */

	if (sat_read_file_info(in, NULL, &finfo, SFI_ALL) == SFI_ERROR) {
		exit(1);
	}

	n_users = finfo.sat_user_entries;
	users = finfo.sat_users;
	n_groups = finfo.sat_group_entries;
	groups = finfo.sat_groups;
	n_hosts = finfo.sat_host_entries;
	hosts = finfo.sat_hosts;

	file_major = finfo.sat_major;
	file_minor = finfo.sat_minor;
	mac_enabled = finfo.sat_mac_enabled;
	cap_enabled = finfo.sat_cap_enabled;
	cipso_enabled = finfo.sat_cipso_enabled;

	if (debug) {
		printf("debug: file header:\n");
		printf("  sat_major = %d\n", finfo.sat_major);
		printf("  sat_minor = %d\n", finfo.sat_minor);
		printf("  sat_start_time = %d\n", finfo.sat_start_time);
		printf("  sat_stop_time = %d\n", finfo.sat_stop_time);
		printf("  sat_host_id = %#x\n", finfo.sat_host_id);
		printf("  sat_mac_enabled = %d\n", finfo.sat_mac_enabled);
		printf("  sat_cap_enabled = %d\n", finfo.sat_cap_enabled);
		printf("  sat_cipso_enabled = %d\n", finfo.sat_cipso_enabled);
		printf("  sat_user_entries = %d\n", finfo.sat_user_entries);
		printf("  sat_group_entries = %d\n", finfo.sat_group_entries);
		printf("  sat_host_entries = %d\n", finfo.sat_host_entries);
	}

	if (file_major > SAT_VERSION_MAJOR_3 ||
	    (file_major == SAT_VERSION_MAJOR_3 &&
	     file_minor > SAT_VERSION_MINOR_1)) {
		fprintf(stderr,
"Error: this file is version %d.%d; this version of sat_interpret can only\n\
interpret file versions up to and including %d.%d\n",
			file_major, file_minor,
			SAT_VERSION_MAJOR_3, SAT_VERSION_MINOR_1);
		exit(1);
	}

	/* set our timezone to that of the file */
	/* don't free finfo.sat_timezone, putenv keeps it! */
	if (timezone) {
		char *tz;
		tz = malloc(strlen(timezone) + 4);
		strcpy(tz, "TZ=");
		strcat(tz, timezone);
		putenv(tz);
	} else {
		putenv(finfo.sat_timezone);
	}
	tzset();

	if (brief != OM_SGIF || debug) {
		/*
		 * Don't output ASCII strings in SGIF mode unless debugging.
		 */
		printf("File version %d.%d\n", finfo.sat_major, finfo.sat_minor);
		cftime(body, "%a %b %e %T %Z %Y", &finfo.sat_start_time);
		printf("Created %s\n", body);
		if (finfo.sat_stop_time != 0) {
			cftime(body, "%a %b %e %T %Z %Y", &finfo.sat_stop_time);
			printf("Closed  %s\n", body);
		} else
			printf("Closed  <unknown>\n");

		if (n_hosts > 1) {
			struct in_addr addr;
			printf("Merged from the following hosts:\n");
			for (i=0; i < n_hosts; i++) {
				addr.s_addr = hosts[i]->sat_id;
				printf("    %s, hostid %s\n", hosts[i]->sat_name.data,
				       inet_ntoa(addr));
			}
			putchar('\n');
		} else {
			struct in_addr addr;
			addr.s_addr = finfo.sat_host_id;
			printf("Written on host %s", finfo.sat_hostname);
			if (*finfo.sat_domainname)
				printf(".%s", finfo.sat_domainname);
			free(finfo.sat_hostname);
			free(finfo.sat_domainname);
			printf(", hostid %s\n\n", inet_ntoa(addr));
		}
	}

	fileoffset = ftell(in);

	hdr_mask = (SHI_GROUPS | SHI_CWD | SHI_ROOTDIR | SHI_PNAME);
	if (mac_enabled)
		hdr_mask |= SHI_PLABEL;
	if (cap_enabled)
		hdr_mask |= SHI_CAP;

	if (brief == OM_SGIF) {
		sgifbuf = malloc(16);
		memset((void *)sgifbuf, '\0', (size_t) 16);
		memcpy((void *)sgifbuf, (void *)&TypeSGIF, (size_t) 16);
		write(1, sgifbuf, 16);
		free (sgifbuf);
	}
	for (;;) {

		if (brief == OM_SGIF) {
			/* initialize SGIF data fields */
			short count;
			for (count = 0; count < NSGIF; count++)
				afield[count].a_data = (char *)NULL;
		}
		sat_read_header_info(in, &hinfo, hdr_mask, file_major,
			file_minor);

		if (ferror(in)) {
			fprintf (stderr, "Error reading record head: ");
			perror(filename);
			exit (1);
		}

		if (feof(in)) {
			fprintf (stderr, "%d records interpreted\n", rec_count);
			exit (0);
		}

		sat_print_header(&hinfo);

#ifdef MAYBE
		if (!hinfo.sat_host_id)	/* if not in record hdr, assign from file hdr? */
			assign_I32dup(HOST_ID, finfo.sat_host_id); /* u_int */
#endif
		size = hinfo.sat_recsize;
		if (MAX_REC_SIZE < size) {
			fprintf (stderr, "SAT body size error!\n");
			exit (2);
		}
                if (size % 4) {
			size += align[ size % 4 ];
			fprintf (stderr, "Bad record alignment\n");
		}

		fileoffset += hinfo.sat_hdrsize + hinfo.sat_recsize;

		fread (body, size, 1, in);
		if (ferror(in) || feof(in)) {
			fprintf (stderr, "Error reading record body: ");
			perror(filename);
			exit (3);
		}

		sat_print_body (&hinfo, body);


		if (brief == OM_SGIF) {
			short 	i, j;
			int 	retval;
			char *tmp_ptr;		/* current position in sgifbuf */

			/*
			 * Put data from afield array into SGIF format in
			 * sgifbuf, then write it to stdout.  Buffer begins
			 * with a 32-bit int containing record length.
			 */
			rlen += sizeof(__int32_t);
			/* fprintf(stderr,"final rlen %d\n", rlen); */
			sgifbuf = malloc(rlen);
			memset((void *)sgifbuf, '\0', (size_t) rlen);
			memcpy((void *) sgifbuf, (void *) &rlen, sizeof(int));
			tmp_ptr = sgifbuf + sizeof(__int32_t);
			for (i = 0; i < NSGIF; i++) {
				if (afield[i].a_data != NULL) {
					/*
					 * For each afield entry that has data, format
					 * a short containing the index, i, another short
					 * containing the data size, then the data, padded
					 * to a 4-byte boundary with space characters.  If
					 * a_data's space was malloc'd, free it now.
					 */
					memcpy((void *) tmp_ptr, (void *) &i, sizeof(short));
					tmp_ptr += sizeof(short);
					memcpy((void *) tmp_ptr, (void *) &(afield[i].a_size), sizeof(short));
					tmp_ptr += sizeof(short);
					memcpy((void *) tmp_ptr, (void *) afield[i].a_data, (size_t) afield[i].a_size);
					tmp_ptr += afield[i].a_size;
					for (j = 0; j < afield[i].a_pad; j++)
						*tmp_ptr++ = ' ';
					if (afield[i].a_free == 1) {
						free(afield[i].a_data);
						afield[i].a_data = 0;
					}
					afield[i].a_data = NULL;
				}
			}
			if ((retval = (write(1, sgifbuf, rlen))) != rlen) {	
				fprintf(stderr, "main: short write: %d\n", retval);
				exit(1);
			}
			free (sgifbuf);
		}

		/* empty the buffer of previous records */
		memset((void *) body, '\0', (size_t) size);

		/* clear header fields */
		free(hinfo.sat_groups);
		if (mac_enabled)
			free(hinfo.sat_plabel);
		if (cap_enabled)
			free(hinfo.sat_pcap);
		if (hinfo.sat_cwd)
			free(hinfo.sat_cwd);
		if (hinfo.sat_rootdir)
			free(hinfo.sat_rootdir);
		if (hinfo.sat_pname)
			free(hinfo.sat_pname);

		rec_count++;
	}
	/* return (0);	unreachable statement */
}
