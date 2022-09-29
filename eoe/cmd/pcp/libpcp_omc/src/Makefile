#!smake
#
# $Id: Makefile,v 1.4 1997/10/24 06:04:37 markgw Exp $

LIBRARY		= libpcp_omc.a
C++FILES	= String.c++ Args.c++ Desc.c++ Indom.c++ Source.c++ \
		  Context.c++ Metric.c++

OBJECTS		= $(C++FILES:S/.c++/.o/g)

PCP_SRC_DEPTH = ../..

include versiondefs
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

LC++DEFS	= $(DEBUG) $(VERSOPT)
LC++OPTS	+= -fullwarn $(PCP_INC_PATH)
LDIRT		+= $(LIBRARY)

default: $(LIBRARY)
#if $(OBJECT_STYLE) == $(DEF_OBJECT_STYLE)
	rm -f ../$(LIBRARY)
	ln -s $(OBJECT_STYLE)/$(LIBRARY) ../$(LIBRARY)
#endif

include $(COMMONRULES)

MYMAKELIB:	.USE .PRECIOUS
	$(AR) c$(ARFLAGS) $(.TARGET) $(.OODATE)

$(LIBRARY):	$(OBJECTS) MYMAKELIB

