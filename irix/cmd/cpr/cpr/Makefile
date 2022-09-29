#!smake
#	"$Revision: 1.40 $"

BASEVERSION=32bit
ALTVERSIONS=64bit
WANTPARALLEL=yes-please

include	$(ROOT)/usr/include/make/cmdcommondefs

COMMANDS = cpr

CFILES	= cpr.c

LLDLIBS	+=-lcpr

LCOPTS	+=-fullwarn -g3 -no_unresolved
LCDEFS	+=-D_KMEMUSER -DCKPT -DNDEBUG -DLIBPW_WAR
LWOFF	=,1209

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	${INSTALL} -F /usr/sbin -m 4755 -O $(IDB_TAG32) cpr
	${INSTALL} -F /usr/bin -lns ../sbin/cpr cpr
	${INSTALL} -F /etc -m 644 cpr_proto

64bitinstall: default
	${INSTALL} -F /usr/sbin -m 4755 -O $(IDB_TAG64) cpr
	${INSTALL} -F /usr/bin -lns ../sbin/cpr cpr
	${INSTALL} -F /etc -m 644 cpr_proto

