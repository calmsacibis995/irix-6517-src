#!smake

#
# FILE: eoe/cmd/miser/cmd/miser_reset/Makefile
#
# DESCRIPTION:
#	Makefile for miser_reset
#

include $(ROOT)/usr/include/make/commondefs

#DEBUG	= -g

LCDEFS	= $(DEBUG)
LCOPTS	= -I../../lib/libmiser/src/ 
LLDLIBS	= -L../../lib/libmiser/ -lmiser
TARGETS	= miser_reset 

IDB_TAG	= -u root -g sys -m 755

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(IDB_TAG) -o miser_reset
