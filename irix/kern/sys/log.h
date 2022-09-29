/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/log.h	10.1"*/
#ident	"$Revision: 3.9 $"
/*
 * Header file for the Streams Log Driver
 */

#include <sys/strlog.h>


struct log {
	unsigned int	log_state;
	struct queue	*log_rdq;
        struct msgb *log_tracemp;
};

/*
 * Driver state values.
 */
#define LOGOPEN         0x01
#define LOGERR          0x02
#define LOGTRC          0x04
#define LOGCONS         0x08
#define LOGDEF          0x10


/* 
 * Module information structure fields
 */
#define LOG_MID		44
#define LOG_NAME	"LOG"
#define LOG_MINPS	0
#define LOG_MAXPS	1024
#define LOG_HIWAT	1024
#define LOG_LOWAT	256

extern int log_bsz;			/* size of internal buffer of log messages */

/*
 * STRLOG(mid,sid,level,flags,fmt,args) should be used for those trace
 * calls that are only to be made during debugging.
 * Extra parentheses are needed in the calls to keep the compiler quiet.
 */
#ifdef DEBUG
#define STRLOG(x)	strlog x
#else
#define STRLOG(x)
#endif


/*
 * Utility macros for strlog.
 */

/*
 * logadjust - move a character pointer up to the next long boundary
 * after its current value.  Assumes sizeof(long) is 2**n bytes for some integer n. 
 */
#define logadjust(wp) (unsigned char *)(((__psunsigned_t)wp + sizeof(long)) & ~(sizeof(long)-1))

/*
 * logstrcpy(dp, sp) copies string sp to dp.
 */

#ifdef u3b2
asm 	char *
logstrcpy(dp, sp) 
{
%	reg s1, s2;

	MOVW s1,%r0
	MOVW s2,%r1
	STRCPY
	MOVW %r0,s1
	MOVW %r1,s2
}
#else
	/*
	 * This is a catchall definition for those processors that have not had
	 * this coded in assembler above.
	 */
#	define logstrcpy(dp, sp)  for (; *dp = *sp; dp++, sp++)
#endif
