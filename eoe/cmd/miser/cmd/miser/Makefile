#!smake

#
# FILE: eoe/cmd/miser/cmd/miser/Makefile
#
# DESCRIPTION:
# 	Makefile for miser daemon.
#

include $(ROOT)/usr/include/make/commondefs

#DEBUG		= -g -DDEBUG

IDB_TAG		= -u root -g sys
IDB_TAGX	= -u root -g sys -m 755
IDB_TAGS	= -u root -g sys -idb "config(suggest)"
IDB_TAGSX	= -u root -g sys -m 755 -idb "config(suggest)"

CXXFILES	= main.o policy_default.o policy_repack.o job.o queue.o debug.o

CFILES		= policy.o

TARGETS		= miser

LDFLAGS		= -L../../lib/libmiser/ -lmiser
LCXXFLAGS	= $(DEBUG) -I../../lib/libmiser/src/

default:	$(TARGETS)

include $(COMMONRULES)

$(TARGETS):	$(OBJECTS)
	$(TOOLROOT)/usr/bin/CC $(OBJECTS) $(LDFLAGS) -n32 -o $@

install: default
	$(INSTALL) -F /usr/etc    $(IDB_TAGX) -o   miser
	$(INSTALL) -F /etc/init.d $(IDB_TAGX) -src miserrc.sh     miser
	$(INSTALL) -F /etc/config $(IDB_TAG)  -src miser.config   miser
	$(INSTALL) -F /etc/config $(IDB_TAGS)       miser.options 
	$(INSTALL) -F /etc        $(IDB_TAGSX)      miser_system.conf
	$(INSTALL) -F /etc        $(IDB_TAGSX)      miser_default.conf
	$(INSTALL) -F /etc        $(IDB_TAGSX)      miser_repack.conf
	$(INSTALL) -F /etc        $(IDB_TAGSX)      miser.conf
	$(INSTALL) -F /etc/rc2.d              -lns ../init.d/miser    S98miser
