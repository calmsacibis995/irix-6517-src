#
# 	Makefile for getconf
#

include $(ROOT)/usr/include/make/commondefs

TARGETS=getconf

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
	${INSTALL} -F /usr/sbin -ln $(TARGETS) sysconf
	${INSTALL} -F /usr/sbin -ln $(TARGETS) pathconf
