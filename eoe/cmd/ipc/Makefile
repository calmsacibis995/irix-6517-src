#!smake

#ident "$Revision: 1.7 $"

BASEVERSION=32bit
ALTVERSIONS=64bit
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

LCDEFS=-D_KMEMUSER

#if $(VERSION) == "32bit"
COMMANDS=ipcrm ipcs
#endif


# -diag_error - issue error for 'function declared implicitly'

#if $(VERSION) == "64bit"
COMMANDS=ipcs
LCDEFS+=-diag_error 1196,1498
#endif

CMD_COPTS=-fullwarn

default:$(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	${INSTALL} -F /usr/sbin -m 2755 -u root -g sys $(IDB_TAG32) ipcs
	${INSTALL} -F /usr/sbin -m 755 -u root -g sys ipcrm
	${INSTALL} -F /usr/bin -lns ../sbin/ipcrm ipcrm
	${INSTALL} -F /usr/bin -lns ../sbin/ipcs ipcs

64bitinstall: default
	${INSTALL} -F /usr/sbin -m 2755 -u root -g sys $(IDB_TAG64) ipcs
	${INSTALL} -F /usr/bin -lns ../sbin/ipcs ipcs
