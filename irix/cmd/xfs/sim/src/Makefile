#!smake
#
# Makefile for libsim (xfs)
#
#ident	"$Revision: 1.63 $"

DEPTH=.
RCFILES=behavior.c fs_bio.c fs_subr.c ktrace.c move.c page.c sim.dev.c \
	sim.lock.c sim.random.c sim.rdwr.c sim.strat.c vfs.c vnode.c xfs_stubs.c
LCFILES=xfs_alloc.c xfs_alloc_btree.c xfs_bit.c xfs_bmap.c xfs_bmap_btree.c \
	xfs_btree.c xfs_buf_item.c xfs_da_btree.c xfs_dir.c xfs_dir2.c \
	xfs_dir2_block.c xfs_dir2_data.c xfs_dir2_leaf.c xfs_dir2_node.c \
	xfs_dir2_sf.c xfs_dir_leaf.c xfs_error.c xfs_extfree_item.c \
	xfs_ialloc.c xfs_ialloc_btree.c xfs_iget.c xfs_inode.c \
	xfs_inode_item.c xfs_macros.c xfs_mount.c xfs_rtalloc.c xfs_rtbit.c \
	xfs_trans.c xfs_trans_ail.c xfs_trans_buf.c xfs_trans_extfree.c \
	xfs_trans_inode.c xfs_trans_item.c xfs_vfsops.c xfs_vnodeops.c

CFILES=$(RCFILES) $(LCFILES)
include $(ROOT)/usr/include/make/libleafdefs
KERN=../../../../kern
XFS=$(KERN)/fs/xfs
LIB=../../../../lib
LCINCS=-I$(ROOT)/usr/include/sys/fs
LDIRT=$(LCFILES)
WOFF=-woff 1209
#if defined(DEBUG_VERSION)
OPTIMIZER=
#endif
include $(LIBLEAFRULES)

$(LCFILES):
	@rm -f $@; ln -s $(XFS)/$@ $@
