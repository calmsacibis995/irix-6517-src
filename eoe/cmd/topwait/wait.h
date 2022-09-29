#ifndef __WAIT_H__
#define __WAIT_H__

#include "htab.h"

typedef struct wl_state_s {
	hashable wl_hash;

	char wl_name[16];	/* name of wchan */
	uint64_t wl_addr;	/* kernel address */
	int wl_type;		/* type of sync obj - see sys/par.h */

	int wl_load;		/* number of procs blocked */
	int wl_max_load;	/* maximum number of procs blocked */

	uint64_t wl_wait_when;	/* timestamp of first wait */
	uint64_t wl_wait_total;	/* total time spent waiting */

	uint64_t wl_nswtch;	/* number of context switches */
} wait_lock_state;

typedef struct wp_state_s {
	hashable wp_hash;

	pid_t wp_pid;		/* pid */
	uint64_t wp_tstamp;	/* tstamp of last state change */

	enum wp_reason_t {
		wp_running,
		wp_waiting,
		wp_preempted,
		wp_yielding,
		wp_stopped,
		wp_dead,
		wp_unknown
	} wp_reason;

	wait_lock_state *wp_locking;	/* ...valid if in wp_waiting state */
} wait_pid_state;

typedef struct wait_state_s {
	htab_t ws_locks;
	htab_t ws_pids;

	/* process states - must match wp_reason_t values */
	int ws_running;
	int ws_preempted;
	int ws_yielding;
	int ws_waiting;
	int ws_stopped;
	int ws_dead;
	int ws_unknown;

	struct {
		uint64_t total;
		uint64_t preempt;
		uint64_t wait;
		uint64_t yield;
	} ws_nswtch;
} wait_state;

#define N_LOCK_BUCKETS 1021
#define N_PID_BUCKETS 1021

#define HASH_PID(pid) (pid)
#define HASH_LOCK(lock) (lock)

#define HASHABLE_TO_WP(hptr) baseof(wait_pid_state, wp_hash, hptr)
#define HASHABLE_TO_WL(hptr) baseof(wait_lock_state, wl_hash, hptr)
#define WP_TO_HASHABLE(wp) &(wp)->wp_hash
#define WL_TO_HASHABLE(wl) &(wl)->wl_hash

#endif /* __WAIT_H__ */
