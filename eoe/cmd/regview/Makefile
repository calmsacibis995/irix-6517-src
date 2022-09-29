#!smake
include ${ROOT}/usr/include/make/commondefs

C++FILES = main.c++ MainWindow.c++ RegionDisplay.c++ RegionInfo.c++ \
	SymTab.c++ util.c++
LLDLIBS = -lvk -lXm -lXt -lX11 -lelf -lmangle

TARGETS = regview

default : ${TARGETS}

include ${COMMONRULES}

regview : ${OBJECTS}
	rm -f $@
	${C++F} ${OBJECTS} ${LDFLAGS} -o $@

install: ${TARGETS}
	${INSTALL} -F /usr/lib -m 4755 regview
	${INSTALL} -F /usr/lib/X11/app-defaults -m 444 Regview
