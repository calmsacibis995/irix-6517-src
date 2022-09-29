#ifndef __SMTD_NN_H
#define __SMTD_NN_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.7 $
 */

extern void nn_reset(SMT_MAC *);
extern void nn_receive(SMT_MAC *, SMT_FB *);
extern void nn_verify(SMT_MAC *);
extern void nn_respond(SMT_MAC *, SMT_FB *, u_long);
extern void nn_expire(SMT_MAC *);

#endif /* __SMTD_NN_H */
