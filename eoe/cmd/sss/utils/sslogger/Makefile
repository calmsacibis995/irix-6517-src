#!smake
#	"$Revision: 1.3 $"
#
include $(ROOT)/usr/include/make/commondefs
include ../../include/ssscommondefs

SRCDIR = .
CFILES  = sslogger.c
OFILES  = $(CFILES:.c=.o)
LCINCS = -nostdinc -I$(ROOT)/usr/include -I$(ROOT)/usr/include/sss
LIBS = -nostdlib -leventmon -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32
TARGETS = esplogger
OPTIMIZER = -g0 -O
LD_QUICKSTART_INFO=
LWOFF = 

COMMONPREF = sss_

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

clean clobber install:  $(COMMONPREF)$$@

default: $(TARGETS)

$(TARGETS): $(OFILES)
	$(CCF) -o $(TARGETS) $(OFILES) $(LCINCS) $(LIBS)

install: default
	$(INSTALL) -idb $(SSS_EOE) -F /usr/sbin esplogger 
