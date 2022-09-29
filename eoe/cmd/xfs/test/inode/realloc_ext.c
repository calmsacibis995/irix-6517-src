
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

	ip->i_u1.iu_extents = NULL;
	ip->i_bytes = 0;

	/*
	 * Make it allocate some space.
	 */
	printf("Test 1\n");
	xfs_iext_realloc(ip, 13);
	if (ip->i_u1.iu_extents == NULL) {
		printf("1: extents NULL\n");
		exit(1);
	}
	sz = 13 * sizeof(xfs_bmbt_rec_t);
	if (ip->i_bytes != sz) {
		printf("1: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}

	/*
	 * Make it allocate some more space.
	 */
	printf("Test 2\n");
	xfs_iext_realloc(ip, 1);
	if (ip->i_u1.iu_extents == NULL) {
		printf("2: extents NULL\n");
		exit(1);
	}
	sz = 14 * sizeof(xfs_bmbt_rec_t);
	if (ip->i_bytes != sz) {
		printf("2: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}

	/*
	 * Make it delete some space.
	 */
	printf("Test 3\n");
	xfs_iext_realloc(ip, -5);
	if (ip->i_u1.iu_extents == NULL) {
		printf("3: extents NULL\n");
		exit(1);
	}
	sz = 9 * sizeof(xfs_bmbt_rec_t);
	if (ip->i_bytes != sz) {
		printf("3: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}

	/*
	 * Make it add some space.
	 */
	printf("Test 4\n");
	xfs_iext_realloc(ip, 3);
	if (ip->i_u1.iu_extents == NULL) {
		printf("4: extents NULL\n");
		exit(1);
	}
	sz = 12 * sizeof(xfs_bmbt_rec_t);
	if (ip->i_bytes != sz) {
		printf("4: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}

	/*
	 * Make it to to inline.
	 */
	printf("Test 5\n");
	xfs_iext_realloc(ip, -11);
	if (ip->i_u1.iu_extents == NULL) {
		printf("5: extents NULL\n");
		exit(1);
	}
	if (ip->i_u1.iu_extents != ip->i_u2.iu_inline_ext) {
		printf("5: extents not inline\n");
		exit (1);
	}
	sz = 1 * sizeof(xfs_bmbt_rec_t);
	if (ip->i_bytes != sz) {
		printf("5: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}

	/*
	 * Make it to to not inline again.
	 */
	printf("Test 6\n");
	xfs_iext_realloc(ip, 19);
	if (ip->i_u1.iu_extents == NULL) {
		printf("6: extents NULL\n");
		exit(1);
	}
	if (ip->i_u1.iu_extents == ip->i_u2.iu_inline_ext) {
		printf("6: extents still inline\n");
		exit (1);
	}
	sz = 20 * sizeof(xfs_bmbt_rec_t);
	if (ip->i_bytes != sz) {
		printf("6: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}

	/*
	 * Delete all the extents.
	 */
	printf("Test 7\n");
	xfs_iext_realloc(ip, -20);
	if (ip->i_u1.iu_extents == NULL) {
		printf("7: extents NULL\n");
		exit(1);
	}
	if (ip->i_u1.iu_extents != ip->i_u2.iu_inline_ext) {
		printf("7: extents not inline\n");
		exit (1);
	}
	sz = 0;
	if (ip->i_bytes != sz) {
		printf("7: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}


	printf("All tests passed\n");
}
	



























