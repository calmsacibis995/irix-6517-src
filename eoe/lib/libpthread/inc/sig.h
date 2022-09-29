#ifndef _SIG_H_
#define _SIG_H_

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* Global names
 */
#define sig_bootstrap		PFX_NAME(sig_bootstrap)
#define sig_deliver_pending	PFX_NAME(sig_deliver_pending)
#define sig_set_to_kset		PFX_NAME(sig_set_to_kset)
#define sig_kset_to_set		PFX_NAME(sig_kset_to_set)

#include <sys/types.h>
#include <signal.h>

#define SIGINDEX(sig)	((sig - 1)/(sizeof(int) * 8))
#define SIGSHIFT(sig)	(1 << (sig - 1) % (sizeof(int) * 8))

#define sigemptyset(set) 				\
	((int *)set)[0] = ((int *)set)[1] = 		\
	((int *)set)[2] = ((int *)set)[3] = 0

#define sigfillset(set) 				\
	((int *)set)[0] = ((int *)set)[1] = 		\
	((int *)set)[2] = ((int *)set)[3] = ~0

#define sigaddset(set, sig) 				\
	((int *)set)[SIGINDEX(sig)] |= SIGSHIFT(sig)

#define sigdelset(set, sig) 				\
	((int *)set)[SIGINDEX(sig)] &= ~SIGSHIFT(sig)

#define sigorset(set1, set2) 				\
	((int *)set1)[0] |= ((int *)set2)[0], 		\
	((int *)set1)[1] |= ((int *)set2)[1], 		\
	((int *)set1)[2] |= ((int *)set2)[2], 		\
	((int *)set1)[3] |= ((int *)set2)[3]

#define sigandset(set1, set2) 				\
	((int *)set1)[0] &= ((int *)set2)[0], 		\
	((int *)set1)[1] &= ((int *)set2)[1], 		\
	((int *)set1)[2] &= ((int *)set2)[2], 		\
	((int *)set1)[3] &= ((int *)set2)[3]

#define sigdiffset(set1, set2) 				\
	((int *)set1)[0] &= ~(((int *)set2)[0]), 	\
	((int *)set1)[1] &= ~(((int *)set2)[1]), 	\
	((int *)set1)[2] &= ~(((int *)set2)[2]), 	\
	((int *)set1)[3] &= ~(((int *)set2)[3])

#define sigismember(set, sig) 				\
	(((int *)set)[SIGINDEX(sig)] & SIGSHIFT(sig))

#define sigisempty(set) 				\
	(((int *)set)[0] == 0 && ((int *)set)[1] == 0 	\
	&& ((int *)set)[2] == 0 && ((int *)set)[3] == 0)


extern void	sig_bootstrap(void);
extern int	sig_deliver_pending(void);

extern void	(*sig_set_to_kset)(sigset_t *, k_sigset_t *);
extern void	(*sig_kset_to_set)(k_sigset_t *, sigset_t *);

#endif /* !_SIG_H_ */
