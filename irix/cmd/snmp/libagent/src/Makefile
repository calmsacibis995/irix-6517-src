#!smake
#
# Makefile for libsnmp.
#
#ident	"$Revision: 1.4 $"

DEPTH=.
include $(ROOT)/usr/include/make/libleafdefs

HEADERS= agent.h commauth.h remotesubagent.h sat.h sgisa.h snmpsa.h \
	subagent.h systemsa.h table.h traphandler.h

C++FILES= commauth.c++ remotesubagent.c++ sat.c++ sgisa.c++ snmpsa.c++ \
    subagent.c++ systemsa.c++ table.c++ traphandler.c++

include $(LIBLEAFRULES)

exports:

headers install:
	${INSTALL} -m 444 -idb SNMPINC -F /usr/include/snmp ${HEADERS}

