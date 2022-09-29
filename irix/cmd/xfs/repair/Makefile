#!smake
#ident	"$Revision: 1.30 $"

BASEVERSION=n32bit
ALTVERSIONS=n32d
OTHERVERSIONS=o32bit o32d

WANTPARALLEL=yes_please

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS = xfs_repair

CFILES =	xfs_repair.c		\
		agheader.c		\
		attr_repair.c		\
		avl.c			\
		avl64.c			\
		bmap.c			\
		dinode.c		\
		dino_chunks.c		\
		dir.c			\
		dir2.c			\
		dir_stack.c		\
		efs.c			\
		globals.c		\
		init.c			\
		incore.c		\
		incore_bmc.c		\
		incore_ino.c		\
		incore_ext.c		\
		io.c			\
		sb.c			\
		scan.c			\
		phase1.c		\
		phase2.c		\
		phase3.c		\
		phase4.c		\
		phase5.c		\
		phase6.c		\
		phase7.c		\
		rt.c			\
		versions.c

HFILES =	agheader.h		\
		attr_repair.h		\
		avl.h			\
		avl64.h			\
		bmap.h			\
		dinode.h		\
		dir.h			\
		dir2.h			\
		dir_stack.h		\
		globals.h		\
		incore.h		\
		protos.h		\
		rt.h			\
		scan.h			\
		versions.h

OFILES = $(CFILES:.c=.o)

# Tracing flags:
#
# -DXR_BMAP_DBG		incore block bitmap debugging
# -DXR_INODE_TRACE	inode processing
# -DXR_BMAP_TRACE	bmap btree processing
# -DXR_DIR_TRACE	directory processing
# -DXR_DUP_TRACE	duplicate extent processing
# -DXR_BCNT_TRACE	incore bcnt freespace btree building
# -DXR_BLD_FREE_TRACE	building on-disk freespace (bcnt/bno) btrees
# -DXR_BLD_INO_TRACE	building on-disk inode allocation btrees
# -DXR_BLD_ADD_EXTENT	track phase 5 block extent creation
# -DXR_BCKPTR_DBG	parent list debugging info
#

#
# Build Flags:
#
# -DXFS_QUOTAS		for building with libsim code that knows
#				about quotas
# -DXFS_NO_ATTRIBUTES	for building without attributes (pre-6.2)
#
LCDEFS+=-D_KMEMUSER -DSTATIC=static -DSIM -DAVL_USER_MODE -DAVL_FUTURE_ENHANCEMENTS -DXFS_QUOTAS -DVERS=V_654 -DXFS_REPAIR_SIM
LCOPTS+= -fullwarn -use_readonly_const
WOFF=-woff 1209

#if defined(DEBUG_VERSION)
OPTIMIZER=
#endif

default: $(TARGETS)

include $(CMDCOMMONRULES)

xfs_repair: $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS)

purify: $(OFILES)
	purify -leaks-at-exit=yes $(CCF) -o xfs_repair $(OFILES) $(LDFLAGS)

n32bitinstall: default
	$(INSTALL) -F /usr/sbin $(COMMANDS)

n32dinstall: default
	$(INSTALL) -F /usr/sbin -idb nostrip -src $(COMMANDS) xfs_repair.debug

o32bitinstall o32dinstall: default

