#!smake

#ident "$Revision: 1.5 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

# unlike normally, these have to be done *after* libleafdefs, because
# libleafdefs includes ../versiondefs.
OBJECT_STYLE=32_M2
CSTYLE=$(CSTYLE_32_M2)

# since this runs native, we want to always build it shared; otherwise
# we inherit the -non_shared flag from versiondefs for the non-shared
# C libs; we don't want to require all irix builders to have all the
# nonshared c libs installed.
VERSION_COPTS=-G0 -call_shared
LIBRARY_COPTS=-fullwarn

TARGETS=mbwc_wrap.h ./iconv_top_include.m4
LDIRT=mbwcoffs make_table mbwc_wrap.h iconv_top_include.m4

targets: $(TARGETS)

include $(COMMONRULES)

all default: $(TARGETS)

install: all default

mbwc_wrap.h: ./mbwcoffs
	./mbwcoffs > $@

./mbwcoffs: mbwcoffs.c
	$(CC) $(CFLAGS) mbwcoffs.c -o $@

./make_table: make_table.c
	$(CC) $(CFLAGS) make_table.c -o $@

./matrix: matrix.c
	$(CC) $(CFLAGS) matrix.c -o $@

./iconv_top_include.m4 iconv_top_include.m4 : ./matrix
	./matrix > $@


