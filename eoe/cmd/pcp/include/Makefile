#!smake
#
# $Id: Makefile,v 2.26 1998/11/15 08:35:24 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

#
# Note:
# PUBLIC_ROOT and PUBLIC_EOE_ROOT are both incompat with NOSHIP and PUBLIC
# (i.e. they are never installed together), but NOSHIP and PUBLIC are often
# installed together.
#
PUBLIC		= pmapi.h impl.h pmda.h trace.h platform_defs.h

PUBLIC_EOE_ROOT	= pmapi.h impl.h pmda.h pmapi_dev.h pmdbg.h commondefs isacommondefs platform_defs.h

PUBLIC_ROOT	= trace.h malloc-audit.h no-malloc-audit.h VkPixmap.h \
		trace_dev.h trace_hash.h

NOSHIP		= pmapi_dev.h malloc-audit.h no-malloc-audit.h VkPixmap.h \
		trace_dev.h trace_hash.h pmdbg.h

CFILES	= 
TARGETS	=
LCOPTS	=
LCDEFS	=
LLDOPTS	=
LLDLIBS	=
IDB_TAG_PUBLIC =	PCP_BASE
IDB_TAG_ROOT =		PCP_ROOT
IDB_TAG_EOE_ROOT =	PCP_EOE_ROOT
IDB_TAG_NOSHIP =	NOSHIP_EOE
LDIRT	= pmdbg.h platform_defs.h

#if $(ISM_NAME) == "irix"
default:	$(PUBLIC_EOE_ROOT)
#else
default:	$(PUBLIC) $(NOSHIP) $(PUBLIC_ROOT) $(PUBLIC_EOE_ROOT)
#endif

include $(COMMONRULES)

pmdbg.h :	impl.h mk_pmdbg
	./mk_pmdbg

platform_defs.h : irix_defs.h
	rm -f platform_defs.h
	cp irix_defs.h platform_defs.h
	chmod 644 platform_defs.h

headers:	install

install :	default
	$(INSTALL) -F /usr/include/pcp -m 444 -idb $(IDB_TAG_EOE_ROOT) $(PUBLIC_EOE_ROOT)
#if $(ISM_NAME) == "pcp"
	$(INSTALL) -F /usr/include/pcp -m 444 -idb $(IDB_TAG_PUBLIC) $(PUBLIC)
	$(INSTALL) -F /usr/include/pcp -m 444 -idb $(IDB_TAG_NOSHIP) $(NOSHIP)
	$(INSTALL) -F /usr/include/pcp -m 444 -idb $(IDB_TAG_ROOT) $(PUBLIC_ROOT)
#endif
