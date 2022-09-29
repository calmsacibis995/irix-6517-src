#!smake
#
# Makefile for SGI XFS File System
#
# $Revision: 1.60 $
DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/fs/xfs
.PATH:$(KPATH)

#if defined(DBOPTS) && !empty(DBOPTS)
#else
#
# when compiling a nondebug kernel, turn off compiler warnings about variables 
# being set but not used. with DEBUG disabled, the ASSERT statements are gone.
#
KWOFF=,1552
#endif

#if defined(CELL)
SUBDIRS=cell
#endif

LCDEFS+= -D_NOPROJQUOTAS
KCOPTS=-fullwarn $(XFSCOPTS)
XFS_NAME=xfs.a
XFS_CSRCS=	xfs_alloc.c \
		xfs_alloc_btree.c \
		xfs_attr.c \
		xfs_attr_leaf.c \
		xfs_bit.c \
		xfs_bmap.c \
		xfs_bmap_btree.c \
		xfs_btree.c \
		xfs_buf_item.c \
		xfs_da_btree.c \
		xfs_dir.c \
		xfs_dir2.c \
		xfs_dir2_block.c \
		xfs_dir2_data.c \
		xfs_dir2_leaf.c \
		xfs_dir2_node.c \
		xfs_dir2_sf.c \
		xfs_dir2_trace.c \
		xfs_dir_leaf.c \
		xfs_error.c \
		xfs_extfree_item.c \
		xfs_fsops.c \
		xfs_ialloc.c \
		xfs_ialloc_btree.c \
		xfs_icrash.c \
		xfs_iget.c \
		xfs_inode.c \
		xfs_inode_item.c \
		xfs_itable.c \
		xfs_dfrag.c \
		xfs_log.c \
		xfs_log_recover.c \
		xfs_macros.c \
		xfs_misc.c \
		xfs_mount.c \
		xfs_rename.c \
		xfs_rw.c \
		xfs_trans.c \
		xfs_trans_ail.c \
		xfs_trans_buf.c \
		xfs_trans_extfree.c \
		xfs_trans_inode.c \
		xfs_trans_item.c \
		xfs_utils.c \
		xfs_vfsops.c \
		xfs_vnodeops.c


XFS_OBJS=$(XFS_CSRCS:.c=.o)
$(XFS_NAME):$(XFS_NAME)($(XFS_OBJS)) MAKELIB

XFSRT_NAME=xfsrt.a
XFSRT_CSRCS=	xfs_grio.c \
		xfs_rtalloc.c \
		xfs_rtbit.c
XFSRT_OBJS=$(XFSRT_CSRCS:.c=.o)
$(XFSRT_NAME):$(XFSRT_NAME)($(XFSRT_OBJS)) MAKELIB

XFSQUOTA_NAME=xfsquotas.a
XFSQUOTA_CSRCS=	xfs_dquot.c \
		xfs_qm.c \
		xfs_qm_syscalls.c \
		xfs_trans_dquot.c \
		xfs_dquot_item.c 
XFSQUOTA_OBJS=$(XFSQUOTA_CSRCS:.c=.o)
$(XFSQUOTA_NAME):$(XFSQUOTA_NAME)($(XFSQUOTA_OBJS)) MAKELIB

XFSDMAPI_NAME=xfsdmapi.a
XFSDMAPI_CSRCS=xfs_dmapi.c
XFSDMAPI_OBJS=$(XFSDMAPI_CSRCS:.c=.o)
$(XFSDMAPI_NAME):$(XFSDMAPI_NAME)($(XFSDMAPI_OBJS)) MAKELIB

XIDBG_NAME=xfsidbg.o
XIDBG_CSRCS=xfsidbg.c

ACL_NAME=acl_xfs.o
ACL_CSRCS=acl_xfs.c
ACL_OBJS=$(ACL_CSRCS:.c=.o)

MAC_NAME=mac_xfs.o
MAC_CSRCS=mac_xfs.c
MAC_OBJS=$(MAC_CSRCS:.c=.o)

FETCH_NAME=xfs_attr_fetch.o
FETCH_CSRCS=xfs_attr_fetch.c
FETCH_OBJS=$(FETCH_CSRCS:.c=.o)

TARGETS=$(XFS_NAME) $(XFSQUOTA_NAME) $(XFSRT_NAME) $(XFSDMAPI_NAME) \
	$(XIDBG_NAME) $(MAC_NAME) $(ACL_NAME) $(FETCH_NAME)

LINTNAME=xfs
CFILES=$(XFS_CSRCS) $(XFSQUOTA_CSRCS) $(XFSRT_CSRCS) $(XFSDMAPI_CSRCS) \
	$(XIDBG_CSRCS) $(MAC_CSRCS) $(ACL_CSRCS) $(FETCH_CSRCS)
HFILES=	xfs_ag.h \
	xfs_alloc.h \
	xfs_alloc_btree.h \
	xfs_attr.h \
	xfs_attr_leaf.h \
	xfs_attr_sf.h \
	xfs_bit.h \
	xfs_bmap.h \
	xfs_bmap_btree.h \
	xfs_btree.h \
	xfs_buf_item.h \
	xfs_clnt.h \
	xfs_dinode.h \
	xfs_da_btree.h \
	xfs_dir.h \
	xfs_dir2.h \
	xfs_dir2_block.h \
	xfs_dir2_data.h \
	xfs_dir2_leaf.h \
	xfs_dir2_node.h \
	xfs_dir2_sf.h \
	xfs_dir2_trace.h \
	xfs_dir_leaf.h \
	xfs_dir_sf.h \
	xfs_dmapi.h \
	xfs_dquot.h \
	xfs_dquot_item.h \
	xfs_dqblk.h \
	xfs_error.h \
	xfs_extfree_item.h \
	xfs_fsops.h \
	xfs_ialloc.h \
	xfs_ialloc_btree.h \
	xfs_imap.h \
	xfs_inode.h \
	xfs_inode_item.h \
	xfs_inum.h \
	xfs_itable.h \
	xfs_dfrag.h \
	xfs_log.h \
	xfs_log_priv.h \
	xfs_log_recover.h \
	xfs_macros.h \
	xfs_mount.h \
	xfs_qm.h \
	xfs_quota.h \
	xfs_quota_priv.h \
	xfs_rtalloc.h \
	xfs_rw.h \
	xfs_sb.h \
	xfs_trans.h \
	xfs_trans_priv.h \
	xfs_trans_space.h \
	xfs_types.h \
	xfs_utils.h \
        cxfs_clnt.h

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${XFS_NAME}
	${KINSTALL} ${XFSRT_NAME}
	${KINSTALL} ${XIDBG_NAME}
	${KINSTALL} ${XFSQUOTA_NAME}
	${KINSTALL} ${XFSDMAPI_NAME}
	${KINSTALL} ${ACL_NAME}
	${KINSTALL} ${MAC_NAME}
	${KINSTALL} ${FETCH_NAME}
	${KDEVINSTALL} ${TARGETS}

headers!
	$(INSTALL) -m 444 -F /usr/include/sys/fs $(HFILES)

xfsidbg.o: xfsidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/xfsidbg.c && \
	$(LDF) $(PRODOBJECT) -G 0 -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

xfs_icrash.o: xfs_icrash.c
	$(CCF) -g3 -c $(JALR) $(KPATH)/xfs_icrash.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)
