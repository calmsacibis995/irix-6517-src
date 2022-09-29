/* mqPxLibP.h - private POSIX message queue library header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,03feb94,kdl  moved structure definitions from mqPxLib.c.
01b,12jan94,kdl  changed mqLibInit() to mqPxLibInit(); added defines for
		 default queue and message size.
01a,02dec93,dvs  written
*/

#ifndef __INCmqPxLibPh
#define __INCmqPxLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "mqueue.h"
#include "objlib.h"
#include "qlib.h"
#include "symlib.h"
#include "private/siglibp.h"

/* defines */

#define MQ_HASH_SIZE_DEFAULT	6	/* default hash size */
#define MQ_NUM_MSGS_DEFAULT	16	/* default messages per queue */
#define MQ_MSG_SIZE_DEFAULT	16	/* default message size in bytes */


struct mq_des
    {
    OBJ_CORE		f_objCore;
    int			f_flag;
    struct msg_que	*f_data;
    };

struct msg_que
    {
    Q_HEAD		msgq_cond_read;
    Q_HEAD		msgq_cond_data;
    SYMBOL		msgq_sym;
    int			msgq_sigTask;
    struct sigpend	msgq_sigPend;
    unsigned long	msgq_links;
    unsigned long	msgq_bmap;
    struct sll_node	*msgq_data_list[32];
    struct sll_node	*msgq_free_list;
    struct mq_attr	msgq_attr;
    };

struct sll_node
    {
    struct sll_node *sll_next;
    size_t sll_size;
    };


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern int	mqPxLibInit (int hashSize);

#else	/* __STDC__ */

extern int	mqPxLibInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmqPxLibPh */
