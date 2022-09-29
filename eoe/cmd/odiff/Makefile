#!smake
# make the BSD diff command

include	$(ROOT)/usr/include/make/commondefs
MKDEPFILE=Makedepend

CFILES=diff.c diffdir.c diffreg.c diffh.c

TARGETS=odiff diffh

DIFFOBJS=diff.o diffdir.o diffreg.o

DIFF=/usr/bin/odiff
DIFFH=/usr/lib/diffh
PR=/usr/bin/pr

LLDLIBS=-lPW
LCDEFS=-DDIFF='"${DIFF}"' -DDIFFH='"${DIFFH}"' -DPR='"${PR}"'
I_FLAGS=-idb std.sw.unix

default:$(TARGETS)

install:default
	$(INSTALL) $(I_FLAGS) -F /usr/bin odiff
	$(INSTALL) $(I_FLAGS) -F /usr/lib diffh

include $(COMMONRULES)

odiff: ${DIFFOBJS}
	$(CCF) -o $@ $(DIFFOBJS) $(LDFLAGS)
