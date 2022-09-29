#ifndef _FUNC_H_
#define _FUNC_H_

#include "htab.h"

typedef struct func_state_s {
	hashable fs_hash;
	uint64_t fs_addr;
	uint64_t fs_self_hits;
	uint64_t fs_child_hits;
} func_state;

#define N_FUNC_BUCKETS 1021

#define HASH_FUNC(addr) (addr)

#define HASHABLE_TO_FS(hptr) baseof(func_state, fs_hash, hptr)
#define FS_TO_HASHABLE(fs) &(fs)->fs_hash

#endif /* _FUNC_H_ */
