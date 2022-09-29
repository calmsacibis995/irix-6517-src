#ident "$Revision: 1.2 $"

AM_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(AM_SRC_DEPTH)/include/amcommondefs

INITDPATH =   /etc/init.d
RC2PATH   =   /etc/rc2.d
LINKPATH  =   ../init.d
RC2SCRIPT =   S95availmon
RC0PATH   =   /etc/rc0.d
RC0SCRIPT =   K05availmon

default: availmon amdiag amstart install.sh

include $(COMMONRULES)

install: default
	${INSTALL} $(IDB_TAG) -F $(INITDPATH) availmon
	${INSTALL} $(IDB_TAG) -F $(RC0PATH) -lns $(LINKPATH)/availmon $(RC0SCRIPT)
	${INSTALL} $(IDB_TAG) -F $(RC2PATH) -lns $(LINKPATH)/availmon $(RC2SCRIPT)
	${INSTALL} $(IDB_TAG) -F $(ETCPATH) $(TARGETS) amdiag amstart
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH) -lns $(ETCPATH)/amdiag amdiag
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH)  install.sh
