#ifndef __SYS_EI_H__
#define __SYS_EI_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ei.h
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

/* definitions for external interrupts */
#include <sys/time.h>

#define EIIOC ('e' << 8)

#define EIIOCSTROBE	(EIIOC|1)	/* generate outgoing interrupts */
#define EIIOCENABLE	(EIIOC|2)	/* enable incoming interrupts */
#define EIIOCDISABLE	(EIIOC|3)	/* disable incoming interrupts */
#define EIIOCSETHI	(EIIOC|4)	/* set output lines hi */
#define EIIOCSETLO	(EIIOC|5)	/* set output lines lo */
#define EIIOCRECV	(EIIOC|6)	/* wait for an incoming interrupt */
#define EIIOCSETOPW	(EIIOC|7)	/* set outgoing pulse width */
#define EIIOCGETOPW	(EIIOC|8)	/* get outgoing pulse width */
#define EIIOCSETIPW	(EIIOC|9)	/* set incoming pulse width */
#define EIIOCGETIPW	(EIIOC|10)	/* get incoming pulse width */
#define EIIOCSETSPW	(EIIOC|11)	/* set stuck pulse width */
#define EIIOCGETSPW	(EIIOC|12)	/* get stuck pulse width */
#define EIIOCSETSIG	(EIIOC|13)	/* set signal to send in intr */
#define EIIOCCLRSTATS	(EIIOC|14)	/* clear debugging stats */
#define EIIOCGETSTATS	(EIIOC|15)	/* get debugging stats */
#define EIIOCSETINTRCPU	(EIIOC|16)	/* set interrupt cpu */
#define EIIOCSETSYSCPU	(EIIOC|17)	/* set syscall cpu */
#define EIIOCREGISTERULI (EIIOC|18)	/* register ULI */
#define EIIOCSETPERIOD	(EIIOC|19)	/* set pulse period */
#define EIIOCGETPERIOD	(EIIOC|20)	/* get pulse period */
#define EIIOCPULSE	(EIIOC|21)	/* set pulse output mode */
#define EIIOCSQUARE	(EIIOC|22)	/* set square wave output mode */
#define EIMKHWG		(EIIOC|23)	/* make hwgraph alias */
#define EIIOCENABLELB	(EIIOC|24)	/* enable loopback interrupt */
#define EIIOCDISABLELB	(EIIOC|25)	/* disable loopback interrupt */

/* directory where external interrupt devices reside in hwgraph */
#define EI_HWG_DIR	"external_int"

struct eistats {
    int intvals[16];
    long long longvals[16];
};

struct eiargs {
    int intval;
    struct timeval timeval;
};

extern int eicinit(void);
extern void *eicinit_f(int);

extern void eicclear(void);
extern void eicclear_f(void*);

extern int eicbusywait(int);
extern int eicbusywait_f(void*, int);

#ifdef __cplusplus
}
#endif

#endif
