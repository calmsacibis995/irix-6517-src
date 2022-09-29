
#include <sys/param.h>
#define _KERNEL
#include <sys/buf.h>
#include <sys/time.h>
#include <sys/grio.h>
#undef _KERNEL
#include <sys/vnode.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <bstring.h>
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"
#include "sim.h"



int
main(int argc, char *argv[])
{
	xfs_inode_t	*ip;
	size_t		sz;

	ip = calloc(1, sizeof(xfs_inode_t));

	ip->i_broot = NULL;
	ip->i_broot_bytes = 0;

	/*
	 * Make it allocate some space.
	 */
	printf("Test 1\n");
	xfs_iroot_realloc(ip, 13);
	if (ip->i_broot == NULL) {
		printf("1: broot NULL\n");
		exit(1);
	}
	sz = sizeof(xfs_btree_block_t) +
	     (13 * (sizeof(xfs_bmbt_rec_t) + sizeof(xfs_agblock_t)));
	if (ip->i_broot_bytes != sz) {
		printf("1: broot_bytes wrong %d %d\n", sz, ip->i_broot_bytes);
		exit(1);
	}

	/*
	 * Make it allocate some more space.
	 */
	printf("Test 2\n");
	xfs_iroot_realloc(ip, 1);
	if (ip->i_broot == NULL) {
		printf("2: broot NULL\n");
		exit(1);
	}
	sz = sizeof(xfs_btree_block_t) +
	     (14 * (sizeof(xfs_bmbt_rec_t) + sizeof(xfs_agblock_t)));
	if (ip->i_broot_bytes != sz) {
		printf("2: broot_bytes wrong %d %d\n", sz, ip->i_broot_bytes);
		exit(1);
	}

	/*
	 * Free some space.
	 */
	printf("Test 3\n");
	xfs_iroot_realloc(ip, -2);
	if (ip->i_broot == NULL) {
		printf("3: broot NULL\n");
		exit(1);
	}
	sz = sizeof(xfs_btree_block_t) +
	     (12 * (sizeof(xfs_bmbt_rec_t) + sizeof(xfs_agblock_t)));
	if (ip->i_broot_bytes != sz) {
		printf("3: broot_bytes wrong %d %d\n", sz, ip->i_broot_bytes);
		exit(1);
	}

	/*
	 * Make it allocate some more space.
	 */
	printf("Test 4\n");
	xfs_iroot_realloc(ip, 5);
	if (ip->i_broot == NULL) {
		printf("4: broot NULL\n");
		exit(1);
	}
	sz = sizeof(xfs_btree_block_t) +
	     (17 * (sizeof(xfs_bmbt_rec_t) + sizeof(xfs_agblock_t)));
	if (ip->i_broot_bytes != sz) {
		printf("4: broot_bytes wrong %d %d\n", sz, ip->i_broot_bytes);
		exit(1);
	}

	/*
	 * Free all the records.
	 */
	printf("Test 5\n");
	xfs_iroot_realloc(ip, -17);
	if (ip->i_broot == NULL) {
		printf("5: broot NULL\n");
		exit(1);
	}
	sz = sizeof(xfs_btree_block_t);
	if (ip->i_broot_bytes != sz) {
		printf("5: broot_bytes wrong %d %d\n", sz, ip->i_broot_bytes);
		exit(1);
	}

	/*
	 * Make it allocate some more space.
	 */
	printf("Test 6\n");
	xfs_iroot_realloc(ip, 5);
	if (ip->i_broot == NULL) {
		printf("6: broot NULL\n");
		exit(1);
	}
	sz = sizeof(xfs_btree_block_t) +
	     (5 * (sizeof(xfs_bmbt_rec_t) + sizeof(xfs_agblock_t)));
	if (ip->i_broot_bytes != sz) {
		printf("6: broot_bytes wrong %d %d\n", sz, ip->i_broot_bytes);
		exit(1);
	}

	printf("All tests passed\n");
}
	



























