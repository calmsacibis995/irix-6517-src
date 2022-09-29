#!smake
#
# Makefile for KLIB headers
#
#ident "$Header: "

include $(ROOT)/usr/include/make/commondefs

INCLDIR = $(ROOT)/usr/include/klib
SRCDIR  = .
HEADERS = alloc.h btnode.h htnode.h queue.h hwreg.h hwregcmd.h \
		  kl_alloc.h kl_cmp.h kl_debug.h kl_dwarflib.h kl_error.h \
		  kl_hwconfig.h kl_hwgraph.h kl_libhwconfig.h kl_libkern.h \
		  kl_libmem.h kl_libswconfig.h kl_libsym.h kl_libtrace.h \
		  kl_page.h kl_print.h kl_stringtab.h kl_struct_sizes.h \
		  kl_stubs.h kl_swconfig.h kl_sysconfig.h kl_types.h klib.h

COMMONPREF = klib_

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

headers: install

exports:

clean clobber install:  $(COMMONPREF)$$@

install: default
	$(INSTALL) -idb "noship ROOT" -m 444 -F /usr/include/klib $(HEADERS)
