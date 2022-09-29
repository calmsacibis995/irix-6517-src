#
# Makefile for rpcbind, the SVR4 replacement for portmap
#
#ident $Revision: 1.10 $

include	$(ROOT)/usr/include/make/commondefs

LCDEFS	= -D_SVR4_TIRPC -DPORTMAP -DCHECK_LOCAL
LCOPTS	= 

LLDOPTS = -L. -Wl,-transitive_link,-T,10000000,-D,10040000 -Wl,-woff,85
LLDLIBS = -lnsl
LIBDMALLOC=dmalloc/libdmalloc.a
COMMONPREF=rpc_
SUBDIRS=dmalloc

CFILES	= check_bound.c pmap_svc.c rpcb_svc.c rpcbind.c stricmp.c warmstart.c

LINTFLAGS=-woff 91,102

TARGETS	= rpcbind

targets default:	$(TARGETS)

$(LIBDMALLOC):
	cd dmalloc; $(MAKE) libdmalloc.a

rpcbind: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

include $(COMMONRULES)

$(COMMONTARGS):	$(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

install: default
	$(INSTALL) -F /usr/etc -idb svr4net $(TARGETS)
