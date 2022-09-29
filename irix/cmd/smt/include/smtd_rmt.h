#ifndef __SMTD_RMT_H
#define __SMTD_RMT_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.5 $
 */

#define T_NON_OP	1	/* 1 sec */
#define T_STUCK		8	/* 8 sec */
#define T_ANNOUNCE	3	/* 3 sec > 2.5(default) */
#define T_DIRECT	1	/* TBD: 362.773 msec */

/* stay out of the ring this long after duplicate address 1st detected */
#define DUPSULK		15
/* do not stay out longer than this */
#define MAX_DUPSULK	(5*60)

extern void smtd_rmt(SMT_MAC *);
extern void rmt_reset(SMT_MAC *);
extern void dup_det(SMT_MAC *);
#endif /* __SMTD_RMT_H */
