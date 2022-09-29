#!smake
#
#	Makefile for SNMP agent library

C++FILES= commauth.c++ 	sat.c++ \

DEPTH=.

C++FILES= commauth.c++ 	sat.c++ \
	  subagent.c++ 	table.c++ 	\
	  snmpsa.c++ 	systemsa.c++ 	sgisa.c++ \
	  traphandler.c++

IFILES= agent.h \
	commauth.h \
	remotesubagent.h \
	sat.h \
	sgisa.h \
	snmpd.auth \
	snmpsa.h \
	subagent.h \
	systemsa.h \
	table.h \
	traphandler.h

include $(ROOT)/usr/include/make/libleafdefs

include $(LIBLEAFRULES)

headers:
	${INSTALL} -F /usr/include/snmp ${IFILES}
