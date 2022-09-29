#!smake
VDEPTH=..
include $(ROOT)/usr/include/make/commondefs
TARGETS= $(OFILES)
OFILES = $(CFILES:.c=.o)
CFILES = sss_pthreads.c
LCDEFS= -I. -I../include -I$(ROOT)/usr/include -I$(ROOT)/usr/include/sss \
	-D_POSIX_C_SOURCE=199506L
OPTIMIZER=-g
COMMONPREF=sss_
all: default

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

$(COMMONPREF)install: $(TARGETS)
default: $(TARGETS)


