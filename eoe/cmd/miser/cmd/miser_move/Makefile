#!smake

#
# FILE: eoe/cmd/miser/cmd/miser_move/Makefile
#
# DESCRIPTION:
#	Makefile for miser_move
#

include $(ROOT)/usr/include/make/commondefs

#DEBUG	= -g 

LCDEFS	= $(DEBUG)
LCOPTS	= -I../../lib/libmiser/src/ 
LLDLIBS	= -L../../lib/libmiser/ -lmiser
TARGETS	= miser_move 

IDB_TAG	= -u root -g sys -m 755

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(IDB_TAG) -o miser_move
