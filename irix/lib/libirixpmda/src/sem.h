/*
 * Kudzu semaphore interface support
 *
 * $Id: sem.h,v 1.2 1997/11/21 06:34:27 kenmcd Exp $
 */

#if !defined(IRIX6_2) && !defined(IRIX6_3) && !defined(IRIX6_4)

typedef struct {
    int         set_id;         /* identifier for a set of semaphores */
    key_t       set_key;        /* key, public or IPC_PRIVATE */
    int         set_nsems;      /* 0..set_nsems-1 == each set element */
} sem_set;

extern int __semstatus(struct semstat *);

#endif /* IRIX6.5 or later */
