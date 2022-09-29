/*
 * shm interface support
 *
 * $Id: shm.h,v 1.1 1997/11/20 19:57:26 kenmcd Exp $
 */

typedef struct {
    int         id;         /* identifier for a set of semaphores */
    key_t       key;        /* key, public or IPC_PRIVATE */
} shm_map_t;
