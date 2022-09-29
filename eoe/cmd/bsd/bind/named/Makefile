#!smake
#
# Makefile for BIND 4.9.3 named and named-xfer
#
# $Revision: 3.9 $

include $(ROOT)/usr/include/make/commondefs
include $(RELEASEDEFS)

LCDEFS	= -DSABER # avoids sccsid & rcsid
LCOPTS	=
LCINCS	= -I../../../include
LLDOPTS =
LDIRT	= version.c version.o

IDBTAG	= -idb std.sw.named

NAMEDSRC= db_dump.c \
	db_glue.c \
	db_load.c \
	db_lookup.c \
	db_reload.c \
	db_save.c \
	db_secure.c \
	db_update.c \
	inet_ntop.c \
	inet_pton.c \
	ns_forw.c \
	ns_init.c \
	ns_main.c \
	ns_maint.c \
	ns_ncache.c \
	ns_req.c \
	ns_resp.c \
	ns_sort.c \
	ns_stats.c \
	ns_validate.c \
	ns_xfr.c \
	nsap_addr.c \
	sgi_util.c \
	storage.c \
	tree.c \
	base64.c \
	res_debug.c
XFERSRC	= named-xfer.c \
	base64.c \
	res_debug.c
CFILES	= $(NAMEDSRC) $(XFERSRC)
NAMEDOBJS= $(NAMEDSRC:.c=.o)
XFEROBJS= $(XFERSRC:.c=.o) db_glue.o nsap_addr.o

TARGETS	=named named-xfer

CMDS	=named.reload named.restart

default all: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) $(IDBTAG) -F /usr/sbin $(TARGETS) $(CMDS)

named: $(NAMEDOBJS)
	sh newvers.sh $(RELEASE)
	$(CCF) $(NAMEDOBJS) version.c $(LDFLAGS) -o $@

named-xfer: $(XFEROBJS) named
	$(CCF) $(XFEROBJS) version.c $(LDFLAGS) -o $@
