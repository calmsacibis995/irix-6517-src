#ident	"$Revision: 1.3 $"

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/var.h>
#define _KERNEL
#include <sys/buf.h>
#include <sys/cred.h>
#undef _KERNEL
#include <sys/vnode.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <strings.h>
#include <stdlib.h>
#include <bstring.h>
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_mount.h"
#include "xfs_alloc.h"
#include "xfs_ialloc.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_bmap.h"
#include "xfs_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"

#include "sim.h"

int change_lsn = 0;

void
trans_done(void *arg)
{
	printf("%s\n", (char *)arg);
}

xfs_lsn_t
xfs_trans_tail_ail(xfs_mount_t *mp)
{
    if (change_lsn)
	return 0x100000005LL;
    else
	return 0;
}


main(int argc, char **argv)
{
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;
	xfs_inode_t	*ip;
	char		*file;
	dev_t		dev;
	xfs_inode_t	*ip2;
	xfs_trans_t	*tp;
	struct cred	cred;
	xfs_log_ticket_t ticket;
	xfs_tid_t	tid;
	xfs_log_callback_t cb;
	xfs_log_iovec_t	reg[10];
	xfs_lsn_t	start_lsn = 0, tmp_lsn = 0;
	char		buf1[21] = "abcdefghijklmnopqrstu";
	char		buf2[21] = "ABCDEFGHIJKLMNOPQRSTU";
	int i;

	if (argc < 2) {
		printf("usage: log <file>\n");
		exit(1);
	}
	file = argv[1];
	printf("log mount: %s\n", file);
	remove(file);
	dev = dev_open(file);
	if (dev_grow(dev, 20*1024) == -1) {
	    printf("dev_grow error\n");
	    exit(1);
	}
	v.v_buf = NBUF;
	v.v_hbuf = HBUF;
	binit();

	mp = xfs_mount_init();
	if (xfs_log_mount(mp, dev, 0, 0) != 0) {
	    printf("xfs_log_mount error\n");
	    exit(1);
	}
	tid = 0;
	if (xfs_log_reserve(mp, 15000, &ticket,
			    XFS_TRANSACTION_MANAGER, 0) != 0) {
	    printf("xfs_log_reserve error\n");
	    exit(1);
	}

	reg[0].i_addr = buf1;
	reg[0].i_len = 20;
	reg[1].i_addr = buf2;
	reg[1].i_len = 20;


	for (i = 0; i < 19; i++) {
	    if (xfs_log_write(mp, reg, 2, ticket, &tmp_lsn) == -1) {
		printf("xfs_log_write error\n");
		exit(1);
	    }
	    if (start_lsn == 0)
		start_lsn = tmp_lsn;
        }

	change_lsn = 1;
	if (xfs_log_write(mp, reg, 2, ticket, &tmp_lsn) == -1) {
		printf("xfs_log_write error\n");
		exit(1);
	}
	if (xfs_log_write(mp, reg, 2, ticket, &tmp_lsn) == -1) {
		printf("xfs_log_write error\n");
		exit(1);
	}

	xfs_log_done(mp, ticket, 0);

	cb.cb_func = trans_done;
	cb.cb_arg = "I'm done";

	xfs_log_notify(mp, start_lsn, &cb);

	dev_close(dev);
	dev = dev_open(file);
	xfs_log_print(0, dev);
}
