#ident	"$Revision: 1.50 $"

#define _KERNEL 1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/sema.h>
#include <sys/debug.h>

#undef _KERNEL
#include <bstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef DEBUG
#include <limits.h>
#endif

#include <sys/kmem.h>
#include <sys/ktrace.h>
#include <sys/vnode.h>
#include <sys/uuid.h>

#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>		/* depends on xfs_types.h, xfs_inum.h */
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>		/* depends on xfs_trans.h & xfs_sb.h */
#include <sys/fs/xfs_buf_item.h>
#include <sys/fs/xfs_extfree_item.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_alloc.h>
#include <sys/fs/xfs_bmap.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_inode_item.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir2_sf.h>
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode.h>		/* depends on xfs_inode_item.h */
#include <sys/fs/xfs_log_priv.h>	/* depends on all above */
#include <sys/fs/xfs_log_recover.h>

#ifdef SIM
#include "sim.h"		/* must be last include file */
#endif

#ifndef _KERNEL

#define FULL_READ	(-3)
#define PARTIAL_READ	(-2)
#define BAD_HEADER	(-1)
#define NO_ERROR	(0)
#define      BBTOOFF64(bbs)  (((off64_t)(bbs)) << BBSHIFT)

STATIC void xlog_print_op_header(xlog_op_header_t *op_head, int i, caddr_t *ptr);

static int exit_flag = 0;
static int nodata_flag = 0;
static int noprint_flag = 0;
static int logBBsize;
static char *trans_type[] = {
	"",
	"SETATTR",
	"SETATTR_SIZE",
	"INACTIVE",
	"CREATE",
	"CREATE_TRUNC",
	"TRUNCATE_FILE",
	"REMOVE",
	"LINK",
	"RENAME",
	"MKDIR",
	"RMDIR",
	"SYMLINK",
	"SET_DMATTRS",
	"GROWFS",
	"STRAT_WRITE",
	"DIOSTRAT"
};

typedef struct xlog_split_item {
	struct xlog_split_item	*si_next;
	struct xlog_split_item	*si_prev;
	xlog_tid_t		si_tid;
	int			si_skip;
} xlog_split_item_t;

xlog_split_item_t *split_list = 0;

extern int print_only_data;

STATIC void
xlog_print_add_to_trans(xlog_tid_t	tid,
			int		skip)
{
    xlog_split_item_t *item;

    item	  = (xlog_split_item_t *)calloc(sizeof(xlog_split_item_t), 1);
    item->si_tid  = tid;
    item->si_skip = skip;
    item->si_next = split_list;
    item->si_prev = 0;
    if (split_list)
	split_list->si_prev = item;
    split_list	  = item;
}	/* xlog_print_add_to_trans */


STATIC int
xlog_print_find_tid(xlog_tid_t tid, uint was_cont)
{
    xlog_split_item_t *listp = split_list;

    if (!split_list) {
	if (was_cont != 0)	/* Not first time we have used this tid */
	    return 1;
	else
	    return 0;
    }
    while (listp) {
	if (listp->si_tid == tid)
	    break;
	listp = listp->si_next;
    }
    if (!listp)  {
	return 0;
    }
    if (--listp->si_skip == 0) {
	if (listp == split_list) {		/* delete at head */
	    split_list = listp->si_next;
	    if (split_list)
		split_list->si_prev = 0;
	} else {
	    if (listp->si_next)
		listp->si_next->si_prev = listp->si_prev;
	    listp->si_prev->si_next = listp->si_next;
	}
    }
    free(listp);
    return 1;
}	/* xlog_print_find_tid */


STATIC void
print_xlog_op_line(void)
{
    printf("----------------------------------------------\n");
}	/* print_xlog_op_line */


STATIC int
xlog_print_trans_header(caddr_t *ptr, int len)
{
    xfs_trans_header_t  *h;
    caddr_t		cptr = *ptr;;

    *ptr += len;
    if (len >= 4)
	printf("%c%c%c%c:", *cptr, *(cptr+1), *(cptr+2), *(cptr+3));
    if (len != sizeof(xfs_trans_header_t)) {
	printf("   Not enough data to decode further\n");
	return 1;
    }
    h = (xfs_trans_header_t *)cptr;
    printf("    type: %s       tid: %x       num_items: %d\n",
	   trans_type[h->th_type], h->th_tid, h->th_num_items);
    return 0;
}	/* xlog_print_trans_header */


STATIC int
xlog_print_trans_buffer(caddr_t *ptr, int len, int *i, int num_ops)
{
    xfs_buf_log_format_t *f;
    xfs_buf_log_format_v1_t *old_f;
    xfs_agi_t		 *agi;
    xfs_agf_t		 *agf;
    xlog_op_header_t	 *head = 0;
    int			 num, skip;
    int			 super_block = 0;
    int			bucket, col, buckets;
    __int64_t		 blkno;
    xfs_buf_log_format_t lbuf;
    int			 size, blen, map_size, struct_size;
    long long		 x, y;
    
    /*
     * bcopy to ensure 8-byte alignment for the long longs in
     * buf_log_format_t structure
     */
    bcopy(*ptr, &lbuf, sizeof(xfs_buf_log_format_t));
    f = &lbuf;
    *ptr += len;

    if (f->blf_type == XFS_LI_BUF) {
	blkno = f->blf_blkno;
	size = f->blf_size;
	blen = f->blf_len;
	map_size = f->blf_map_size;
	struct_size = sizeof(xfs_buf_log_format_t);
    } else {
	old_f = (xfs_buf_log_format_v1_t*)f;
	blkno = old_f->blf_blkno;
	size = old_f->blf_size;
	blen = old_f->blf_len;
	map_size = old_f->blf_map_size;
	struct_size = sizeof(xfs_buf_log_format_v1_t);
    }
    switch (f->blf_type)  {
    case XFS_LI_BUF:
	printf("BUF:  ");
	break;
    case XFS_LI_6_1_BUF:
	printf("6.1 BUF:  ");
	break;
    case XFS_LI_5_3_BUF:
	printf("5.3 BUF:  ");
	break;
    default:
	printf("UNKNOWN BUF:  ");
	break;
    }
    if (len >= struct_size) {
	ASSERT((len - sizeof(struct_size)) % sizeof(int) == 0);
	printf("#regs: %d   start blkno: %llx  len: %d  bmap size: %d\n",
	       size, blkno, blen, map_size);
	if (blkno == 0)
	    super_block = 1;
    } else {
	ASSERT(len >= 4);	/* must have at least 4 bytes if != 0 */
	printf("#regs: %d   Not printing rest of data\n", f->blf_size);
	return size;
    }
    num = size-1;

    /* Check if all regions in this log item were in the given LR ptr */
    if (*i+num > num_ops-1) {
	skip = num - (num_ops-1-*i);
	num = num_ops-1-*i;
    } else {
	skip = 0;
    }
    while (num-- > 0) {
	(*i)++;
	head = (xlog_op_header_t *)*ptr;
	xlog_print_op_header(head, *i, ptr);
	if (super_block) {
		ASSERT((__psunsigned_t)&((xfs_sb_t *)0)->sb_icount ==
							XFS_BLI_CHUNK);
		printf("SUPER BLOCK Buffer: ");
		if (head->oh_len < 4*8) {
			printf("Out of space\n");
		} else {
			printf("\n");
			/*
			 * bcopy because *ptr may not be 8-byte aligned
			 */
			bcopy(*ptr, &x, sizeof(long long));
			bcopy(*ptr+8, &y, sizeof(long long));
			printf("icount: %lld  ifree: %lld  ", x, y);
			bcopy(*ptr+16, &x, sizeof(long long));
			bcopy(*ptr+24, &y, sizeof(long long));
			printf("fdblks: %lld  frext: %lld\n", x, y);
		}
		super_block = 0;
	} else if (*(uint *)(*ptr) == XFS_AGI_MAGIC) {
		agi = (xfs_agi_t *)(*ptr);
		printf("AGI Buffer: XAGI  ");
		if (head->oh_len <
		    sizeof(xfs_agi_t) -
		    XFS_AGI_UNLINKED_BUCKETS*sizeof(xfs_agino_t)) {
			printf("out of space\n");
		} else {
			printf("\n");
			printf("ver: %d  ", agi->agi_versionnum);
			printf("seq#: %d  len: %d  cnt: %d  root: %d\n",
			       agi->agi_seqno, agi->agi_length,
			       agi->agi_count, agi->agi_root);
			printf("level: %d  free#: 0x%x  newino: 0x%x\n",
			       agi->agi_level, agi->agi_freecount,
			       agi->agi_newino);
			if (head->oh_len == 128) {
				buckets = 17;
			} else if (head->oh_len == 256) {
				buckets = 32 + 17;
			} else {
				buckets = XFS_AGI_UNLINKED_BUCKETS;
			}
			for (bucket = 0; bucket < buckets;) {
				printf("bucket[%d - %d]: ", bucket, bucket+3);
				for (col = 0; col < 4; col++, bucket++) {
					if (bucket < buckets) {
						printf("0x%x ",
						       agi->agi_unlinked[bucket]);
					}
				}
				printf("\n");
			}
		}
	} else if (*(uint *)(*ptr) == XFS_AGF_MAGIC) {
		agf = (xfs_agf_t *)(*ptr);
		printf("AGF Buffer: XAGF  ");
		if (head->oh_len < sizeof(xfs_agf_t)) {
			printf("Out of space\n");
		} else {
			printf("\n");
			printf("ver: %d  seq#: %d  len: %d  \n",
			       agf->agf_versionnum, agf->agf_seqno,
			       agf->agf_length);
			printf("root BNO: %d  CNT: %d\n",
			       agf->agf_roots[XFS_BTNUM_BNOi],
			       agf->agf_roots[XFS_BTNUM_CNTi]);
			printf("level BNO: %d  CNT: %d\n",
			       agf->agf_levels[XFS_BTNUM_BNOi],
			       agf->agf_levels[XFS_BTNUM_CNTi]);
			printf("1st: %d  last: %d  cnt: %d  freeblks: %d  longest: %d\n",
			       agf->agf_flfirst, agf->agf_fllast, agf->agf_flcount,
			       agf->agf_freeblks, agf->agf_longest);
		}
		       
	} else {
		extern int print_data;

		printf("BUF DATA\n");
		if (print_data) {
			uint *dp  = (uint *)*ptr;
			int  nums = head->oh_len >> 2;
			int  i = 0;

			while (i < nums) {
				if ((i % 8) == 0)
					printf("%2x ", i);
				printf("%8x ", *dp);
				dp++;
				i++;
				if ((i % 8) == 0)
					printf("\n");
			}
			printf("\n");
		}
	}
	*ptr += head->oh_len;
    }
    if (head && head->oh_flags & XLOG_CONTINUE_TRANS)
	skip++;
    return skip;
}	/* xlog_print_trans_buffer */


STATIC int
xlog_print_trans_efd(caddr_t *ptr, uint len)
{
    xfs_efd_log_format_t *f;
    xfs_extent_t	 *ex;
    int			 i;
    xfs_efd_log_format_t lbuf;
    
    /*
     * bcopy to ensure 8-byte alignment for the long longs in
     * xfs_efd_log_format_t structure
     */
    bcopy(*ptr, &lbuf, sizeof(xfs_efd_log_format_t));
    f = &lbuf;
    ASSERT(len == sizeof(xfs_efd_log_format_t));
    *ptr += len;
    if (len >= sizeof(xfs_efd_log_format_t)) {
	printf("EFD:  #regs: %d    num_extents: %d  id: 0x%llx\n",
	       f->efd_size, f->efd_nextents, f->efd_efi_id);
	ex = f->efd_extents;
	for (i=0; i< f->efd_size; i++) {
		printf("(s: 0x%llx, l: %d) ", ex->ext_start, ex->ext_len);
		if (i % 4 == 3) printf("\n");
		ex++;
	}
	if (i % 4 != 0) printf("\n");
	return 0;
    } else {
	printf("EFD: Not enough data to decode further\n");
	return 1;
    }
}	/* xlog_print_trans_efd */


STATIC int
xlog_print_trans_efi(caddr_t *ptr, uint len)
{
    xfs_efi_log_format_t *f;
    xfs_extent_t	 *ex;
    int			 i;
    xfs_efi_log_format_t lbuf;

    /*
     * bcopy to ensure 8-byte alignment for the long longs in
     * xfs_efi_log_format_t structure
     */
    bcopy(*ptr, &lbuf, sizeof(xfs_efi_log_format_t));
    f = &lbuf;
    *ptr += len;
    ASSERT(len == sizeof(xfs_efi_log_format_t));
    if (len >= sizeof(xfs_efi_log_format_t)) {
	printf("EFI:  #regs: %d    num_extents: %d  id: 0x%llx\n",
	       f->efi_size, f->efi_nextents, f->efi_id);
	ex = f->efi_extents;
	for (i=0; i< f->efi_size; i++) {
		printf("(s: 0x%llx, l: %d) ", ex->ext_start, ex->ext_len);
		if (i % 4 == 3) printf("\n");
		ex++;
	}
	if (i % 4 != 0) printf("\n");
	return 0;
    } else {
	printf("EFI: Not enough data to decode further\n");
	return 1;
    }
}	/* xlog_print_trans_efi */


/* ARGSUSED */
STATIC void
xlog_print_trans_inode_core(xfs_dinode_core_t *ip)
{
    printf("INODE CORE\n");
    printf("magic 0x%hx mode 0%ho version %d format %d\n",
	   ip->di_magic, ip->di_mode, (int)ip->di_version,
	   (int)ip->di_format);
    printf("nlink %hd uid %d gid %d\n",
	   ip->di_nlink, ip->di_uid, ip->di_gid);
    printf("atime 0x%x mtime 0x%x ctime 0x%x\n",
	   ip->di_atime.t_sec, ip->di_mtime.t_sec, ip->di_ctime.t_sec);
    printf("size 0x%llx nblocks 0x%llx extsize 0x%x nextents 0x%x\n",
	   ip->di_size, ip->di_nblocks, ip->di_extsize, ip->di_nextents);
    printf("naextents 0x%x forkoff %d dmevmask 0x%x dmstate 0x%hx\n",
	   ip->di_anextents, (int)ip->di_forkoff, ip->di_dmevmask,
	   ip->di_dmstate);
    printf("flags 0x%x gen 0x%x\n",
	   ip->di_flags, ip->di_gen);
}

STATIC void
xlog_print_dir_sf(xfs_dir_shortform_t *sfp, int size)
{
	xfs_ino_t	ino;
	int		count;
	int		i;
	char		namebuf[257];
	xfs_dir_sf_entry_t	*sfep;

	printf("SHORTFORM DIRECTORY size %d count %d\n",
	       size, sfp->hdr.count);
	bcopy(&(sfp->hdr.parent), &ino, sizeof(ino));
	printf(".. ino 0x%llx\n", ino);

	count = (uint)(sfp->hdr.count);
	sfep = &(sfp->list[0]);
	for (i = 0; i < count; i++) {
		bcopy(&(sfep->inumber), &ino, sizeof(ino));
		bcopy((sfep->name), namebuf, sfep->namelen);
		namebuf[sfep->namelen] = '\0';
		printf("%s ino 0x%llx namelen %d\n",
		       namebuf, ino, sfep->namelen);
		sfep = XFS_DIR_SF_NEXTENTRY(sfep);
	}
}

STATIC int
xlog_print_trans_inode(caddr_t *ptr, int len, int *i, int num_ops)
{
    xfs_inode_log_format_t *f;
    xfs_inode_log_format_t_v1 *old_f;
    xfs_dinode_core_t	   dino;
    xlog_op_header_t	   *op_head;
    int			   version;
    xfs_inode_log_format_t lbuf;
    int			   mode;
    int			   size;

    /*
     * print inode type header region
     *
     * bcopy to ensure 8-byte alignment for the long longs in
     * xfs_inode_log_format_t structure
     */
    bcopy(*ptr, &lbuf, sizeof(xfs_inode_log_format_t));
    version = lbuf.ilf_type;
    f = &lbuf;
    (*i)++;					/* bump index */
    *ptr += len;
    if (version == XFS_LI_5_3_INODE) {
	old_f = (xfs_inode_log_format_t_v1 *)f;
	if (len == sizeof(xfs_inode_log_format_t_v1)) {
	    printf("5.3 INODE: #regs: %d   ino: 0x%llx  flags: 0x%x   dsize: %d\n",
		   old_f->ilf_size, old_f->ilf_ino,
		   old_f->ilf_fields, old_f->ilf_dsize);
	} else {
	    ASSERT(len >= 4);	/* must have at least 4 bytes if != 0 */
	    printf("5.3 INODE: #regs: %d   Not printing rest of data\n",
		   old_f->ilf_size);
	    return old_f->ilf_size;
	}
    } else {
	if (len == sizeof(xfs_inode_log_format_t)) {
	    if (version == XFS_LI_6_1_INODE)
		printf("6.1 INODE: ");
	    else printf("INODE: ");
	    printf("#regs: %d   ino: 0x%llx  flags: 0x%x   dsize: %d\n",
		   f->ilf_size, f->ilf_ino, f->ilf_fields, f->ilf_dsize);
	    printf("        blkno: %lld  len: %d  boff: %d\n",
		   f->ilf_blkno, f->ilf_len, f->ilf_boffset);
	} else {
	    ASSERT(len >= 4);	/* must have at least 4 bytes if != 0 */
	    printf("INODE: #regs: %d   Not printing rest of data\n",
		   f->ilf_size);
	    return f->ilf_size;
	}
    }

    if (*i >= num_ops)			/* end of LR */
	    return f->ilf_size-1;

    /* core inode comes 2nd */
    op_head = (xlog_op_header_t *)*ptr;
    xlog_print_op_header(op_head, *i, ptr);
    bcopy(*ptr, &dino, sizeof(dino));
    mode = dino.di_mode & IFMT;
    size = (int)dino.di_size;
    xlog_print_trans_inode_core(&dino);
    *ptr += sizeof(xfs_dinode_core_t);
    if (XLOG_SET(op_head->oh_flags, XLOG_CONTINUE_TRANS))  {
	return f->ilf_size-1;
    }

    if (*i == num_ops-1 && f->ilf_size == 3)  {
	    return 1;
    }

    /* does anything come next */
    op_head = (xlog_op_header_t *)*ptr;
    switch (f->ilf_fields & XFS_ILOG_NONCORE) {
	case XFS_ILOG_DEXT: {
	    ASSERT(f->ilf_size == 3);
	    (*i)++;
	    xlog_print_op_header(op_head, *i, ptr);
	    printf("EXTENTS inode data\n");
	    *ptr += op_head->oh_len;
	    if (XLOG_SET(op_head->oh_flags, XLOG_CONTINUE_TRANS))  {
		return 1;
	    }
	    break;
	}
	case XFS_ILOG_DBROOT: {
	    ASSERT(f->ilf_size == 3);
	    (*i)++;
	    xlog_print_op_header(op_head, *i, ptr);
	    printf("BTREE inode data\n");
	    *ptr += op_head->oh_len;
	    if (XLOG_SET(op_head->oh_flags, XLOG_CONTINUE_TRANS))  {
		return 1;
	    }
	    break;
	}
	case XFS_ILOG_DDATA: {
	    ASSERT(f->ilf_size == 3);
	    (*i)++;
	    xlog_print_op_header(op_head, *i, ptr);
	    printf("LOCAL inode data\n");
	    if (mode == IFDIR) {
		xlog_print_dir_sf((xfs_dir_shortform_t*)*ptr, size);
	    }
	    *ptr += op_head->oh_len;
	    if (XLOG_SET(op_head->oh_flags, XLOG_CONTINUE_TRANS))
		return 1;
	    break;
	}
	case XFS_ILOG_DEV: {
	    ASSERT(f->ilf_size == 2);
	    printf("DEV inode: no extra region\n");
	    break;
	}
	case XFS_ILOG_UUID: {
	    ASSERT(f->ilf_size == 2);
	    printf("UUID inode: no extra region\n");
	    break;
	}
	case 0: {
	    ASSERT(f->ilf_size == 2);
	    break;
	}
	default: {
	    xlog_panic("xlog_print_trans_inode: illegal inode type");
	}
    }
    return 0;
}	/* xlog_print_trans_inode */



/******************************************************************************
 *
 *		Log print routines
 *
 ******************************************************************************
 */

STATIC void
xlog_print_lseek(xlog_t *log, int fd, daddr_t blkno, int whence)
{
	off64_t offset;

	if (whence == SEEK_SET)
		offset = BBTOOFF64(blkno+log->l_logBBstart);
	else
		offset = BBTOOFF64(blkno);
	if (lseek64(fd, offset, whence) < 0)
	    xlog_panic("xlog_lseek: lseek error");
}	/* xlog_print_lseek */


STATIC void
print_lsn(caddr_t	string,
	  xfs_lsn_t	*lsn)
{
    printf("%s: %d,%d", string, ((uint *)lsn)[0], ((uint *)lsn)[1]);
}


/*
 * Given a pointer to a data segment, print out the data as if it were
 * a log operation header.
 */
STATIC void
xlog_print_op_header(xlog_op_header_t	*op_head,
		     int		i,
		     caddr_t		*ptr)
{
    xlog_op_header_t hbuf;

    /*
     * bcopy because on 64/n32, partial reads can cause the op_head
     * pointer to come in pointing to an odd-numbered byte
     */
    bcopy(op_head, &hbuf, sizeof(xlog_op_header_t));
    op_head = &hbuf;
    *ptr += sizeof(xlog_op_header_t);
    printf("Oper (%d): tid: %x  len: %d  clientid: %s  ",
	   i, op_head->oh_tid, op_head->oh_len,
	   (op_head->oh_clientid == XFS_TRANSACTION ? "TRANS" :
	    (op_head->oh_clientid == XFS_LOG ? "LOG" : "ERROR")));
    printf("flags: ");
    if (op_head->oh_flags) {
	if (op_head->oh_flags & XLOG_START_TRANS)
	    printf("START ");
	if (op_head->oh_flags & XLOG_COMMIT_TRANS)
	    printf("COMMIT ");
	if (op_head->oh_flags & XLOG_WAS_CONT_TRANS)
	    printf("WAS_CONT ");
	if (op_head->oh_flags & XLOG_UNMOUNT_TRANS)
	    printf("UNMOUNT ");
	if (op_head->oh_flags & XLOG_CONTINUE_TRANS)
	    printf("CONTINUE ");
	if (op_head->oh_flags & XLOG_END_TRANS)
	    printf("END ");
    } else {
	printf("none");
    }
    printf("\n");
}	/* xlog_print_op_header */


int
xlog_print_record(int		  fd,
		 int		  num_ops,
		 int		  len,
		 int		  *read_type,
		 caddr_t	  *partial_buf,
		 xlog_rec_header_t *rhead)
{
    xlog_op_header_t	*op_head;
    xlog_rec_header_t	*rechead;
    caddr_t		buf, ptr;
    int			read_len, skip;
    int			ret, n, i;

    if (noprint_flag)
	    return NO_ERROR;

    /* read_len must read up to some block boundary */
    read_len = (int) BBTOB(BTOBB(len));

    /* read_type => don't malloc() new buffer, use old one */
    if (*read_type == FULL_READ) {
	ptr = buf = (caddr_t) kmem_alloc(read_len, 0);
    } else {
	read_len -= *read_type;
	buf = (caddr_t)((__psint_t)(*partial_buf) + (__psint_t)(*read_type));
	ptr = *partial_buf;
    }
    if ((ret = (int) read(fd, buf, read_len)) == -1) {
	printf("xlog_print_record: read error\n");
	exit(1);
    }

    /* Did we overflow the end? */
    if (*read_type == FULL_READ &&
	BLOCK_LSN(rhead->h_lsn)+BTOBB(read_len) >= logBBsize) {
	*read_type = BBTOB(logBBsize-BLOCK_LSN(rhead->h_lsn)-1);
	*partial_buf = buf;
	return PARTIAL_READ;
    }
    /* Did we read everything? */
    if ((ret == 0 && read_len != 0) || ret != read_len) {
	*read_type = ret;
	*partial_buf = buf;
	return PARTIAL_READ;
    }
    if (*read_type != FULL_READ)
	read_len += *read_type;
    
    /* Everything read in.  Start from beginning of buffer */
    buf = ptr;
    for (i = 0; ptr < buf + read_len; ptr += BBSIZE, i++) {
	rechead = (xlog_rec_header_t *)ptr;
	if (rechead->h_magicno == XLOG_HEADER_MAGIC_NUM) {
	    xlog_print_lseek(0, fd, -read_len+i*BBSIZE, SEEK_CUR);
	    free(buf);
	    return -1;
	} else {
		if (rhead->h_cycle != *(uint *)ptr) {
		    if (*read_type == FULL_READ)
			return -1;
		    else if (rhead->h_cycle+1 != *(uint *)ptr)
			return -1;
		}
	}
	*(uint *)ptr = rhead->h_cycle_data[i];
    }
    ptr = buf;
    for (i=0; i<num_ops; i++) {
	print_xlog_op_line();
	op_head = (xlog_op_header_t *)ptr;
	xlog_print_op_header(op_head, i, &ptr);

	/* print transaction data */
	if (nodata_flag ||
	    ((XLOG_SET(op_head->oh_flags, XLOG_WAS_CONT_TRANS) ||
	      XLOG_SET(op_head->oh_flags, XLOG_CONTINUE_TRANS)) && 
	     op_head->oh_len == 0)) {
	    for (n = 0; n < op_head->oh_len; n++) {
		printf("%c", *ptr);
		ptr++;
	    }
	    printf("\n");
	    continue;
	}
	if (xlog_print_find_tid(op_head->oh_tid,
				op_head->oh_flags & XLOG_WAS_CONT_TRANS)) {
	    printf("Left over region from split log item\n");
	    ptr += op_head->oh_len;
	    continue;
	}
	if (op_head->oh_len != 0) {
	    if (*(uint *)ptr == XFS_TRANS_HEADER_MAGIC) {
		skip = xlog_print_trans_header(&ptr, op_head->oh_len);
	    } else {
		switch (*(unsigned short *)ptr) {
		    case XFS_LI_5_3_BUF:
		    case XFS_LI_6_1_BUF:
		    case XFS_LI_BUF: {
			skip = xlog_print_trans_buffer(&ptr, op_head->oh_len,
						       &i, num_ops);
			break;
		    }
		    case XFS_LI_5_3_INODE:
		    case XFS_LI_6_1_INODE:
		    case XFS_LI_INODE: {
			skip = xlog_print_trans_inode(&ptr, op_head->oh_len,
						      &i, num_ops);
			break;
		    }
		    case XFS_LI_EFI: {
			skip = xlog_print_trans_efi(&ptr, op_head->oh_len);
			break;
		    }
		    case XFS_LI_EFD: {
			skip = xlog_print_trans_efd(&ptr, op_head->oh_len);
			break;
		    }
		    case XLOG_UNMOUNT_TYPE: {
			printf("Unmount filesystem\n");
			skip = 0;
			break;
		    }
		    default: {
			printf("0x%hx - ", *(unsigned short *)ptr);
			xlog_warn("unrecognized type of log operation");
			skip = 0;
			ptr += op_head->oh_len;
		    }
		} /* switch */
	    } /* else */
	    if (skip != 0)
		xlog_print_add_to_trans(op_head->oh_tid, skip);
	}
    }
    printf("\n");
    free(buf);
    return NO_ERROR;
}	/* xlog_print_record */


int
xlog_print_rec_head(xlog_rec_header_t *head, int *len)
{
    int i;
    
    if (noprint_flag)
	    return head->h_num_logops;

    if (head->h_magicno != XLOG_HEADER_MAGIC_NUM)
	return BAD_HEADER;
    printf("cycle: %d	version: %d	", head->h_cycle, head->h_version);
    print_lsn("	lsn", &head->h_lsn);
    print_lsn("	tail_lsn", &head->h_tail_lsn);
    printf("\n");
    printf("length of Log Record: %d	prev offset: %d		num ops: %d\n",
	   head->h_len, head->h_prev_block, head->h_num_logops);
    
    printf("cycle num overwrites: ");
    for (i=0; i< XLOG_RECORD_BSIZE/BBSIZE; i++) {
	printf("%x  ", head->h_cycle_data[i]);
    }
    printf("\n");
    
    *len = head->h_len;
    return(head->h_num_logops);
}	/* xlog_print_rec_head */


#if !defined(_KERNEL) && !defined(SIM)
/*
 * Code needs to look at cycle # at start of block  XXXmiken
 */
int
xlog_find_head(dev_t log_dev, int log_bbnum)
{
    xlog_rec_header_t	*head;
    int			block_start = 0;
    int			block_no = 0;
    int			cycle_no = 0;
    buf_t		*bp;
    
    while (block_no < log_bbnum) {
	bp = bread(log_dev, block_no, 1);
	if (bp->b_flags & B_ERROR) {
	    brelse(bp);
	    xlog_panic("xlog_find_head");
	}
	head = (xlog_rec_header_t *)bp->b_dmaaddr;
	if (head->h_magicno != XLOG_HEADER_MAGIC_NUM) {
	    block_no++;
	    brelse(bp);
	    continue;
	}
	if (cycle_no == 0) {
	    cycle_no	= CYCLE_LSN(head->h_lsn);
	    block_start = block_no;
	} else if (CYCLE_LSN(head->h_lsn) < cycle_no) {
	    cycle_no	= CYCLE_LSN(head->h_lsn);
	    block_start	= block_no;
	    brelse(bp);
	    break;
	}
	block_no++;
	brelse(bp);
    }
    return block_start;
}	/* xlog_find_head */
#endif

STATIC void
print_xlog_record_line(void)
{
    printf("===============================================================\n");
}	/* print_xlog_record_line */

STATIC void
print_xlog_bad_header(daddr_t blkno, caddr_t buf)
{
	printf("*******************************************\n");
	printf("*			                  *\n");
	printf("* ERROR header (blk_no: %lld) (cycle #: %d) *\n",
		(__int64_t)blkno, GET_CYCLE(buf));
	printf("*			                  *\n");
	printf("*******************************************\n");
	if (exit_flag)
	    xlog_exit("Bad log record header");
}	/* print_xlog_bad_header */

STATIC void
print_xlog_bad_data(daddr_t blkno)
{
	printf("*****************************\n");
	printf("*			    *\n");
	printf("* ERROR data (blk_no: %lld)   *\n", (__int64_t)blkno);
	printf("*			    *\n");
	printf("*****************************\n");
	if (exit_flag)
	    xlog_exit("Bad data in log");
}	/* print_xlog_bad_data */


/*
 * This code is gross and needs to be rewritten.
 */
/* ARGSUSED */
void xfs_log_print(xfs_mount_t	*mp,
		   dev_t	log_dev,
		   daddr_t	blk_offset,
		   int		num_bblks,
		   int		print_block_start,
		   uint		flags)
{
    int		fd = bmajor(log_dev);
    char	hbuf[XLOG_HEADER_SIZE];
    xlog_t	log;
    int		num_ops, len;
    daddr_t	block_end, block_start, blkno, error;
    int		read_type = FULL_READ;
    caddr_t	partial_buf;
    extern void init_log_struct(xlog_t *, dev_t, daddr_t, int);
    
    if (! xlog_debug)
	return;
    
    if (flags & XFS_LOG_PRINT_EXIT)
	exit_flag++;
    if (flags & XFS_LOG_PRINT_NO_DATA)
	nodata_flag++;

    ASSERT(blk_offset <= INT_MAX);
    bzero(&log, sizeof(log));
    init_log_struct(&log, log_dev, blk_offset, num_bblks);

    if (mp == NULL)
	mp = log.l_mp;

    /*
     * Normally, block_start and block_end are the same value since we
     * are printing the entire log.  However, if the start block is given,
     * we still end at the end of the logical log.
     */
    if (error = xlog_print_find_oldest(&log, &block_end)) {
	    fprintf(stderr, "xfs_log_print: problem finding oldest LR\n");
	    return;
    }
    if (print_block_start == -1)
	    block_start = block_end;
    else
	    block_start = print_block_start;
    xlog_print_lseek(&log, fd, block_start, SEEK_SET);
    blkno    = block_start;
    
    for (;;) {
	if (read(fd, hbuf, 512) == 0) {
	    printf("xfs_log_print: physical end of log\n");
	    print_xlog_record_line();
	    break;
        }
	if (print_only_data) {
		extern void xlog_recover_print_data(caddr_t, int);
		printf("BLKNO: %lld\n", (__int64_t)blkno);
		xlog_recover_print_data(hbuf, 512);
		blkno++;
		goto loop;
	}
	num_ops = xlog_print_rec_head((xlog_rec_header_t *)hbuf, &len);
	blkno++;
	if (num_ops == BAD_HEADER) {
	    print_xlog_bad_header(blkno-1, hbuf);
	    goto loop;
	}
	error =	xlog_print_record(fd, num_ops, len, &read_type, &partial_buf,
				  (xlog_rec_header_t *)hbuf);
	switch (error) {
	    case 0: {
		blkno += BTOBB(len);
		if (print_block_start != -1 &&
		    blkno >= block_end)		/* If start specified, we */
			goto end;		/* end early */
		break;
	    }
	    case -1: {
		print_xlog_bad_data(blkno-1);
		if (print_block_start != -1 &&
		    blkno >= block_end)		/* If start specified, */
			goto end;		/* we end early */
		xlog_print_lseek(&log, fd, blkno, SEEK_SET);
		goto loop;
	    }
	    case PARTIAL_READ: {
		printf("=================================\n");
		printf("xfs_log_print: physical end of log\n");
		printf("=================================\n");
		blkno = 0;
		xlog_print_lseek(&log, fd, 0, SEEK_SET);
		/*
		 * We may have hit the end of the log when we started at 0.
		 * In this case, just end.
		 */
		if (block_start == 0)
			goto end;
		goto partial_log_read;
	    }
	    default: xlog_panic("illegal value");
	}
	print_xlog_record_line();
loop:
	if (blkno >= num_bblks) {
		printf("xfs_log_print: physical end of log\n");
		print_xlog_record_line();
		break;
	}
    }

    /* Do we need to print the first part of physical log? */
    if (block_start != 0) {
	blkno = 0;
	xlog_print_lseek(&log, fd, 0, SEEK_SET);
	for (;;) {
	    if (read(fd, hbuf, 512) == 0) {
		xlog_panic("xlog_find_head: bad read");
	    }
	    if (print_only_data) {
		extern void xlog_recover_print_data(caddr_t, int);
		printf("BLKNO: %lld\n", (__int64_t)blkno);
		xlog_recover_print_data(hbuf, 512);
		blkno++;
		goto loop2;
	    }
	    num_ops = xlog_print_rec_head((xlog_rec_header_t *)hbuf, &len);
	    blkno++;
	    if (num_ops == BAD_HEADER) {
		print_xlog_bad_header(blkno-1, hbuf);
		if (blkno >= block_end)
		    break;
		continue;
	    }
partial_log_read:
	    error= xlog_print_record(fd, num_ops, len, &read_type,
				    &partial_buf, (xlog_rec_header_t *)hbuf);
	    if (read_type != FULL_READ)
		len -= read_type;
	    read_type = FULL_READ;
	    if (!error)
		blkno += BTOBB(len);
	    else {
		print_xlog_bad_data(blkno-1);
		xlog_print_lseek(&log, fd, blkno, SEEK_SET);
		goto loop2;
	    }
	    print_xlog_record_line();
loop2:
	    if (blkno >= block_end)
		break;
        }
    }
    
end:
    printf("xfs_xlog_print: logical end of log\n");
    print_xlog_record_line();
}
#endif /* !_KERNEL */
