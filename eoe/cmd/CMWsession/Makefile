#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=CMWsession CMWloop CMWlogin CMWdialog CMWpasswd CMWsitecheck
LOGTARGETS=CMWbackground

LLDLIBS=-lcmd

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -m 755 -F /usr/bin/X11 $(TARGETS)
	${INSTALL} -m 444 -u cmwlogin -F /usr/CMW $(LOGTARGETS)
