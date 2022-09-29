#!smake
#ident	"$Revision: 1.28 $"

BASEVERSION=n32bit
ALTVERSIONS=n32d
OTHERVERSIONS=o32bit o32d
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=xfs_logprint

KERN=$(DEPTH)/../../kern
XFS=$(KERN)/fs/xfs
LCFILES=xfs_log_recover.c xfs_log_print.c
TLINKXARGS+=-x 'xfs_log_recover.c' -x 'xfs_log_print.c'
LCINCS+=-I$(DEPTH)/sim/src
LCOPTS+=-fullwarn
LLDOPTS+=-L$(DEPTH)/sim
CFILES = log_print_trans.c log_misc.c logprint.c $(LCFILES)
OFILES = $(CFILES:.c=.o)
LDIRT=$(LCFILES)
WOFF=
#if defined(DEBUG_VERSION)
OPTIMIZER=
#endif

default: $(TARGETS)

include $(CMDCOMMONRULES)

xfs_logprint: $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS)

$(ALTVERSIONS)!$(LCFILES)
$(LCFILES):
	@rm -f $@; ln -s $(XFS)/$@ $@

n32bitinstall: default
	$(INSTALL) -F /usr/sbin xfs_logprint

n32dinstall:
	$(INSTALL) -F /usr/sbin -idb nostrip -src xfs_logprint xfs_logprint.debug
