#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/rmdir/RCS/Makefile,v 1.10 1992/08/13 01:17:03 ism Exp $"

include	$(ROOT)/usr/include/make/commondefs

TARGETS=rmdir
# We need libgen.a
LLDLIBS = -lgen
IDB_TAG = -idb "std.sw.unix mr"

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(IDB_TAG) $(TARGETS)
