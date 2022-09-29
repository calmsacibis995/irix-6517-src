#!smake

#
# FILE: eoe/cmd/miser/cmd/cpuset/Makefile
#
# DESCRIPTION:
#	Makefile for cpuset
#

include $(ROOT)/usr/include/make/commondefs

#DEBUG	= -g 

LCDEFS	= $(DEBUG)
LCOPTS	= -I../../lib/libcpuset/src/ 
LLDLIBS	= -L../../lib/libcpuset/ -lcpuset

TARGETS	= cpuset 

IDB_TAG	= -u root -g sys -m 755

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(IDB_TAG) -o cpuset
	$(INSTALL) -F /usr/sbin -lns cpuset miser_cpuset
