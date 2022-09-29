#!smake

#ident	"$Revision: 1.15 $"

# the kernel interface is only 32-bits wide to avoid converting 
# in mixed situations
OBJECT_STYLE=N32_M3
include	$(ROOT)/usr/include/make/commondefs

CFILES	=slip.c pputil.c \
	callers.c conn.c getargs.c interface.c strsave.c sysfiles.c \
	ulockf.c uucpdefs.c uufake.c

TARGETS	=slip remoteslip dbslip

LLDLIBS	=-lm
LCOPTS	=-fullwarn
LCDEFS	=-DSLIP

I_FLAGS	=-idb "slip.sw.slip"
CI_FLAGS=-idb "slip.sw.slip config(suggest)"

default:$(TARGETS)

include $(COMMONRULES)

install:$(TARGETS)
	$(INSTALL) $(I_FLAGS) -F /usr/etc slip
	$(INSTALL) $(CI_FLAGS) -F /usr/etc remoteslip dbslip

slip:	$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
