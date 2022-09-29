/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	suite3_h
#	define suite3_h " @(#) suite3.h:3.2 5/30/92 20:18:49"
#endif

/*
** suite3.h
**
**	Common include file for Suite 3 benchmark.
**
*/

#include <stdio.h>
#include <errno.h>

/* Default to SysV */
#if !defined(SYS5) && !defined(V7)
#define SYS5
#endif

#if defined(MULTIUSER)
#define EXTERN
#else
#define EXTERN		extern
#endif

#if !defined(TRUE)
#define TRUE		1
#define FALSE		0
#endif

#define WORKLD		50
#define WORKFILE	"workfile"
#define CONFIGFILE	"config"
#define STROKES		50		/* baud rate per user typing */
#define MAXITR		10		/* max number of iteration */
#define MAXDRIVES	255		/* max number of HD drives to test */
#define RTMSEC(x)	rtmsec(x)
#define CHILDHZ(x)	((long) times(&t[0]),t[0].tms_cstime+t[0].tms_cutime)
#define LIST		100		/* results array size */
#define LOADNUM		50		/* number of procs to do per user */
#define CONTINUE	1
#define STRLEN		80

#ifndef HZ   				/* default 60-Hz clock freq */
#define HZ		60
#endif

char	*ctime();			/* to get the date */
void	killall();			/* for signal handling */
void	letgo();
void	dead_kid();			/* for signal handling */
void	math_err();

/*----------------------------------------------
** Declared GLOBALs for use in other files
**----------------------------------------------
*/
EXTERN int	*p_i1;
EXTERN long	*p_fcount;
EXTERN long	debug;

typedef union {
	char	c;
	short	s;
	int	i;
	long	l;
	float	f;
	double	d;
} Result;
#if defined(MULTIUSER)
Result results[LIST];
#endif

int	pipe_cpy(), creat_clo();
int	disk_rr(), disk_rw(), disk_rd(), disk_wrt(), disk_cp(), disk_src();
int	add_long(), add_short(), add_float(), add_double();
int	fun_cal(), fun_cal1(), fun_cal2(), fun_cal15();
int	ram_rshort(), ram_rlong(), ram_rchar();
int	ram_wshort(), ram_wlong(), ram_wchar();
int	ram_cshort(), ram_clong(), ram_cchar();
int	mul_short(), mul_long(), mul_float(), mul_double();
int	div_short(), div_long(), div_float(), div_double();

#ifdef __STDC__
typedef void	(*FPtr)();
#else
typedef int	(*FPtr)();
#endif

typedef struct	CARGS {
	char	*name;
	char	*args;
	int	(*f)();
} Cargs;
	
/*
 *	the following array is added to enable command line arguments
 *	to be passed to the individual tests to help foil optimizing
 *	compilers.  the array cmdargs[] and the function hasargs() are
 *	used to do the assignments.  this particular approach was chosen
 *	over others because it seems to keep the actual size of "multiuser"
 *	to a minimum.  cmdargs[] entry 0 is used instead of passing a null
 *	character pointer around to indicate a null strings.  this keeps
 *	the platforms that don't handle these null string pointers from
 *	barfing.
 */

#if defined(MULTIUSER)
Cargs cmdargs[] = { 
	"NOCMD",	"NOARGS", (FPtr)0,
	"add_double",	"256 1.23456789123E-9 9.876543210987E-9", add_double,
	"add_float",	"256 1.2345678E-9 9.8765432E-9", add_float,
	"add_long",	"1 1 256", add_long,
	"add_short",	"1 1 256", add_short,
	"div_double",	"512 999989.7E20 1.1", div_double,
	"div_float",	"512 999989.7E10 1.1", div_float,
	"div_long",	"999999998 3 512", div_long,
	"div_short",	"29998 3 512", div_short,
	"mul_double",	"512 1.23456789123E20 9.876543210987E-1", mul_double,
	"mul_float",	"512 1.2345678E10 9.8765432E-1", mul_float,
	"mul_long",	"1 1 512", mul_long,
	"mul_short",	"1 1 512", mul_short,
	"fun_cal",	"1 0", fun_cal,
	"fun_cal1",	"1 0", fun_cal1,
	"fun_cal2",	"1 0", fun_cal2,
	"fun_cal15",	"1 0", fun_cal15,
	"ram_cchar",	"64", ram_cchar,
	"ram_clong",	"64", ram_clong,
	"ram_cshort",	"64", ram_cshort,
	"ram_rchar",	"32", ram_rchar,
	"ram_rlong",	"8", ram_rlong,
	"ram_rshort",	"16", ram_rshort,
	"ram_wchar",	"32 64", ram_wchar,
	"ram_wlong",	"8 64", ram_wlong,
	"ram_wshort",	"16 64", ram_wshort,
	"pipe_cpy",	"NOARGS", pipe_cpy,
	"disk_src",	"NOARGS", disk_src,
/* creat/rd/wrt/cp have args, but they are dynamically determined at runtime **
** runtap() knows to send args if there >1 filsys to be tested -TL 11/8/89   */
	"creat-clo",	"DISKS", creat_clo,
	"disk_rr",	"DISKS", disk_rr,
	"disk_rw",	"DISKS", disk_rw,
	"disk_rd",	"DISKS", disk_rd,
	"disk_wrt",	"DISKS", disk_wrt,
	"disk_cp",	"DISKS", disk_cp
};
#define MAXCMDARGS	(sizeof(cmdargs) / sizeof(Cargs))
#endif

/*
** Defines for disk tests
*/
#define TMPFILE1 "tmpa.common"
#define TMPFILE2 "tmpb.XXXXXX"

/*
** Definitions for adaptive timer/incrementer
**
** 	(tn1 / tn) >= THRESHOLD
**	else
**		new user increment = o_inc * (3 / (tn / tn_avg))
**
** 12/6/89 Tin
*/
#if defined(MULTIUSER)

#define ADAPT_START	8	/* start adaptive timer after 8 datapoints */
#define THRESHOLD_MIN	1.20
#define THRESHOLD_MAX	1.30

double	a_tn;		/* real time t at n users */
double	a_tn1;		/* real time t at (n+1) users */

#endif

#undef EXTERN
