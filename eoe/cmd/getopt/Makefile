# $Revision: 1.10 $

include $(ROOT)/usr/include/make/commondefs

CFILES=getopt.c
SHFILES=getoptcvt.sh
TARGETS=getopt getoptcvt

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin getopt
	${INSTALL} -F /usr/lib getoptcvt
