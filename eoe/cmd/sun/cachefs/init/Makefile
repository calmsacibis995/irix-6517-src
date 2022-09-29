include ${ROOT}/usr/include/make/commondefs

IDB_TAG= -idb onc3.sw.cachefs

TARGETS= 

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -m 0744 -F /etc/init.d $(IDB_TAG) -src cachefs cachefs
	${INSTALL} -F /etc/rc2.d -lns ../init.d/cachefs $(IDB_TAG) S99cachefs
	${INSTALL} -F /etc/rc0.d -lns ../init.d/cachefs $(IDB_TAG) K04cachefs
