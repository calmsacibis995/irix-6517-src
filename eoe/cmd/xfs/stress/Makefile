#!smake
#ident	"$Revision"

include $(ROOT)/usr/include/make/commondefs
WOFF=
LCOPTS = -fullwarn
INSTDIR = /usr/stress/xfs
SUBDIRS = error
CFILES = alloc.c attrtest.c dirperf.c dirstress.c dbtest64.c \
	fsstress.c holes.c joe.c metaperf.c \
	nametest.c permname.c randholes.c truncfile.c 
TARGETS = $(CFILES:.c=) run_metaperf repair_test

default: $(TARGETS)
	$(SUBDIRS_MAKERULE)
COMMONPREF=stress_
include $(COMMONRULES)

install $(COMMONTARGS): $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

$(COMMONPREF)install: default
	$(INSTALL) -F $(INSTDIR) $(TARGETS)

$(SUBDIRS): $(_FORCE)
	@$(ECHO) "====\t$@"; cd $@; $(MAKE)

