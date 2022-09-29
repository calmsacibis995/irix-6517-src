#!smake
#
# Makefile for build esphttpd daemon
#
include $(ROOT)/usr/include/make/commondefs
include ../../include/ssscommondefs

SRCDIR       = .
OBJECT_STYLE = N32
CFILES       = ssrv_main.c ssrv_util.c ssrv_mime.c ssrv_base64.c
OFILES       = $(CFILES:.c=.o)
LCINCS       = -nostdinc -I$(ROOT)/usr/include -I. \
	       -I$(ROOT)/usr/include/sss           \
               -I$(SRCDIR)/../include 
TARGETS      = esphttpd
INITPROGS    = $(TARGETS)
COMMONPREF   = sss_

LD_QUICKSTART_INFO=

all: default

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

clean clobber install:  $(COMMONPREF)$$@

default: $(TARGETS)

$(TARGETS):  $(OFILES)
	$(CCF) -woff 1048 -o $@ $(OFILES) $(LDOPTS) $(LDLIBS) -lpthread -lssdb

install: default
	$(INSTALL) -idb $(SSS_EOE) -F /usr/etc $(TARGETS)
