#!smake
#ident	"$Revision: 1.30 $"

BASEVERSION=n32bit
ALTVERSIONS=n32d 64bit 64d
OTHERVERSIONS=o32bit o32d
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

HFILES=	addr.h agf.h agfl.h agi.h attr.h attrshort.h bit.h block.h bmap.h \
	bmapbt.h bmroot.h bnobt.h check.h cntbt.h command.h convert.h data.h \
	dbread.h debug.h dir.h dir2.h dir2sf.h dirshort.h dquot.h echo.h \
	faddr.h field.h flist.h fprint.h frag.h freesp.h hash.h help.h init.h \
	inobt.h inode.h input.h io.h malloc.h mount.h output.h print.h quit.h \
	sb.h sig.h strvec.h type.h write.h

CFILES=	$(HFILES:.h=.c) main.c
OFILES=	$(CFILES:.c=.o)

LCDEFS+=-DSTATIC=static -DSIM -DVERS=V_654
LCOPTS+=-fullwarn -use_readonly_const
WOFF=
#if defined(DEBUG_VERSION)
OPTIMIZER=
#endif

default: $(TARGETS)

include $(CMDCOMMONRULES)

xfs_db: $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS)

n32bitinstall: default
	$(INSTALL) -F /usr/sbin xfs_db
	$(INSTALL) -F /usr/sbin xfs_check xfs_ncheck
	$(INSTALL) -F /sbin -lns ../usr/sbin/xfs_check xfs_check
	$(INSTALL) -F /usr/sbin xfs_chver

64bitinstall: default
	$(INSTALL) -F /usr/sbin $(IDB_TAG64) -src xfs_db xfs_db64
	$(INSTALL) -F /usr/sbin $(IDB_TAG64) xfs_check64 xfs_ncheck64

n32dinstall: default
	$(INSTALL) -F /usr/sbin -idb nostrip -src xfs_db xfs_db.debug

64dinstall: default
	$(INSTALL) -F /usr/sbin $(IDB_TAG64) -idb nostrip -src xfs_db xfs_db64.debug
