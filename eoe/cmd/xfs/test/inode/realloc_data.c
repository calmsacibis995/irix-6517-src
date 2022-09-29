
#include <sys/types.h>
#define _KERNEL
#include <sys/buf.h>
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
#include "xfs_ialloc.h"
#include "xfs_bmap_btree.h"
#include "xfs_btree.h"
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
	ip->i_real_bytes = 0;

	/*
	 * Make it allocate some space.
	 */
	printf("Test 1\n");
	xfs_idata_realloc(ip, 13);
	if (ip->i_u1.iu_data == NULL) {
		printf("1: data NULL\n");
		exit(1);
	}
	sz = 13;
	if (ip->i_bytes != sz) {
		printf("1: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}
	if (ip->i_real_bytes != 0) {
		printf("1: real_bytes wrong %d %d %d\n", sz, ip->i_bytes,
		       ip->i_real_bytes);
		exit(1);
	}
	if (ip->i_u1.iu_data != ip->i_u2.iu_inline_data) {
		printf("1: iu_data pointing to wrong thing\n");
		exit(1);
	}

	/*
	 * Make it allocate some more space.
	 */
	printf("Test 2\n");
	xfs_idata_realloc(ip, 1);
	if (ip->i_u1.iu_data == NULL) {
		printf("2: data NULL\n");
		exit(1);
	}
	sz = 14;
	if (ip->i_bytes != sz) {
		printf("2: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}
	if (ip->i_real_bytes != 0) {
		printf("2: real_bytes wrong %d %d %d\n", sz, ip->i_bytes,
		       ip->i_real_bytes);
		exit(1);
	}
	if (ip->i_u1.iu_data != ip->i_u2.iu_inline_data) {
		printf("2: iu_data pointing to wrong thing\n");
		exit(1);
	}

	/*
	 * Make it allocate some more space and leave the inline buffer.
	 */
	printf("Test 3\n");
	xfs_idata_realloc(ip, 25);
	if (ip->i_u1.iu_data == NULL) {
		printf("2: data NULL\n");
		exit(1);
	}
	sz = 39;
	if (ip->i_bytes != sz) {
		printf("3: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}
	if (ip->i_real_bytes != 40) {
		printf("3: real_bytes wrong %d %d %d\n", sz, ip->i_bytes,
		       ip->i_real_bytes);
		exit(1);
	}
	if (ip->i_u1.iu_data == ip->i_u2.iu_inline_data) {
		printf("3: iu_data pointing to wrong thing\n");
		exit(1);
	}

	/*
	 * Make it allocate some more space without reallocing the buffer.
	 */
	printf("Test 4\n");
	xfs_idata_realloc(ip, 1);
	if (ip->i_u1.iu_data == NULL) {
		printf("2: data NULL\n");
		exit(1);
	}
	sz = 40;
	if (ip->i_bytes != sz) {
		printf("4: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}
	if (ip->i_real_bytes != 40) {
		printf("4: real_bytes wrong %d %d %d\n", sz, ip->i_bytes,
		       ip->i_real_bytes);
		exit(1);
	}
	if (ip->i_u1.iu_data == ip->i_u2.iu_inline_data) {
		printf("4: iu_data pointing to wrong thing\n");
		exit(1);
	}


	/*
	 * Make it delete some space.
	 */
	printf("Test 5\n");
	xfs_idata_realloc(ip, -5);
	if (ip->i_u1.iu_data == NULL) {
		printf("5: data NULL\n");
		exit(1);
	}
	sz = 35;
	if (ip->i_bytes != sz) {
		printf("5: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}
	if (ip->i_real_bytes != 36) {
		printf("5: real_bytes wrong %d %d %d\n", sz, ip->i_bytes,
		       ip->i_real_bytes);
		exit(1);
	}
	if (ip->i_u1.iu_data == ip->i_u2.iu_inline_data) {
		printf("5: iu_data pointing to wrong thing\n");
		exit(1);
	}

	/*
	 * Make it delete some space and go inline.
	 */
	printf("Test 6\n");
	xfs_idata_realloc(ip, -5);
	if (ip->i_u1.iu_data == NULL) {
		printf("5: data NULL\n");
		exit(1);
	}
	sz = 30;
	if (ip->i_bytes != sz) {
		printf("6: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}
	if (ip->i_real_bytes != 0) {
		printf("6: real_bytes wrong %d %d %d\n", sz, ip->i_bytes,
		       ip->i_real_bytes);
		exit(1);
	}
	if (ip->i_u1.iu_data != ip->i_u2.iu_inline_data) {
		printf("6: iu_data pointing to wrong thing\n");
		exit(1);
	}


	/*
	 * Make it add some space again.
	 */
	printf("Test 7\n");
	xfs_idata_realloc(ip, 10);
	if (ip->i_u1.iu_data == NULL) {
		printf("6: data NULL\n");
		exit(1);
	}
	sz = 40;
	if (ip->i_bytes != sz) {
		printf("7: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}
	if (ip->i_real_bytes != 40) {
		printf("7: real_bytes wrong %d %d %d\n", sz, ip->i_bytes,
		       ip->i_real_bytes);
		exit(1);
	}
	if (ip->i_u1.iu_data == ip->i_u2.iu_inline_data) {
		printf("7: iu_data pointing to wrong thing\n");
		exit(1);
	}


	/*
	 * Shrink it by one and make sure the i_real_data doesn't
	 * change.
	 */
	printf("Test 8\n");
	xfs_idata_realloc(ip, -1);
	if (ip->i_u1.iu_data == NULL) {
		printf("8: data NULL\n");
		exit(1);
	}
	sz = 39;
	if (ip->i_bytes != sz) {
		printf("8: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}
	if (ip->i_real_bytes != 40) {
		printf("8: real_bytes wrong %d %d %d\n", sz, ip->i_bytes,
		       ip->i_real_bytes);
		exit(1);
	}
	if (ip->i_u1.iu_data == ip->i_u2.iu_inline_data) {
		printf("8: data pointing to wrong thing\n");
		exit (1);
	}

	/*
	 * Delete all the data.
	 */
	printf("Test 9\n");
	xfs_idata_realloc(ip, -39);
	if (ip->i_u1.iu_data != NULL) {
		printf("9: data not NULL\n");
		exit(1);
	}
	sz = 0;
	if (ip->i_bytes != sz) {
		printf("9: bytes wrong %d %d\n", sz, ip->i_bytes);
		exit(1);
	}
	if (ip->i_real_bytes != 0) {
		printf("9: real_bytes wrong %d %d %d\n", sz, ip->i_bytes,
		       ip->i_real_bytes);
		exit(1);
	}


	printf("All tests passed\n");
}
	



























