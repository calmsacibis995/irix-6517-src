include	${ROOT}/usr/include/make/commondefs

TARGETS= mknod

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -F /sbin ${TARGETS}
	${INSTALL} -F /etc -lns ../sbin/${TARGETS} ${TARGETS} 
