#!smake
#
# Makefile for libxlv
#
#ident "$Revision: 1.25 $"

DEPTH=.

LAB_CFILES=vh_to_tab.c xlv_lab1.c xlv_lab2.c xlv_labfile.c xlv_lower_admin.c
UTIL_CFILES=pathnames.c xlv_stubs.c xlv_devgen.c xlv_error.c xlv_tab.c \
	xlv_error_text.c plex_block_map.c plex_revive.c xlv_utils.c \
	license_check.c xlv_equiv_classes.c xlv_cmds.c
CFILES=$(LAB_CFILES) $(UTIL_CFILES)

include $(ROOT)/usr/include/make/libleafdefs

XLV=../../include
KERN=../../../../kern
DEBUG=-g -DDEBUG
WOFF=
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852

SUBDIR_CINCS=-I$(XLV) -I$(KERN)

TARGETS+= xlv_error_text.c plex_block_map.c license_check_stub.o

defaults install: $(TOP)$(LIBRARY) $(TARGETS)

include $(LIBLEAFRULES)

plex_block_map.c:	../../../../kern/io/xlv/plex_block_map.c
	rm -f plex_block_map.c
	ln -s ../../../../kern/io/xlv/plex_block_map.c plex_block_map.c

xlv_error_text.c:	../xlv.err
	awk -f xlv_c.awk ../xlv.err > xlv_error_text.c


debug:
	exec $(MAKE) 'SUBDIR_COPTS=$(DEBUG)'

lint:
	exec $(MAKE) 'SUBDIR_COPTS=$(LINTOPTS)'
