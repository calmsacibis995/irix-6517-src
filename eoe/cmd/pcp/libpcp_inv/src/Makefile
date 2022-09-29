#!smake
#
# $Id: Makefile,v 1.8 1999/04/30 01:44:04 kenmcd Exp $

PCP_SRC_DEPTH = ../..
include versiondefs
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

LIBRARY		= libpcp_inv.a
C++FILES	= Inv.c++ App.c++ MetricList.c++ \
		  Modulate.c++ ModList.c++ \
		  ScaleMod.c++ YScaleMod.c++ ColorScale.c++ ColorMod.c++ \
		  ColScaleMod.c++ StackMod.c++ ToggleMod.c++ \
		  Menu.c++ LaunchMenu.c++ Launch.c++ Record.c++ \
	          FormUI.c++ Form.c++ Window.c++ \
		  View.c++ tv.c++

#if $(IRIX) == "IRIX5_3"
C++FILES        += VkPixmap.c++
#endif

OBJECTS		= $(C++FILES:S/.c++/.o/g)

WARNOFF		= -woff 1201,1233,1375
LC++DEFS	= $(DEBUG) $(VERSOPT)
LC++OPTS	+= -fullwarn $(WARNOFF) $(PCP_INC_PATH) -I$(PCP_SRC_DEPTH)/libpcp_omc/src
LDIRT		+= $(LIBRARY) tv.c++ tv.h

default: $(LIBRARY)
#if $(OBJECT_STYLE) == $(DEF_OBJECT_STYLE)
	rm -f ../$(LIBRARY)
	ln -s $(OBJECT_STYLE)/$(LIBRARY) ../$(LIBRARY)
#endif

tv.o: tv.h

tv.h:
	ln -s $(PCP_SRC_DEPTH)/pmtime/common/tv.h tv.h

tv.c++:
	ln -s $(PCP_SRC_DEPTH)/pmtime/common/tv.c++ tv.c++

include $(COMMONRULES)

MYMAKELIB:	.USE .PRECIOUS
	$(AR) c$(ARFLAGS) $(.TARGET) $(.OODATE)

$(LIBRARY):	$(OBJECTS) MYMAKELIB 

