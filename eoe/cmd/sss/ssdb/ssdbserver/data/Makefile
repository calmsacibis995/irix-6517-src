#!smake
#
# Makefile for SSDB Source Code
#
OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs
include ../../../include/ssscommondefs

default: $(TARGETS)

DATAFILES = db.frm host.frm user.frm func.frm 

ERRMSGFILES = errmsg.txt errmsg.sys

MYSQLDATAdir = /var/esp/init/datafiles

SSDBSETUPDIR = /var/esp/ssdb/data/mysql

MYSQLSHARE = /usr/share/mysql/english

SWSUBSYS = $(SSS_EOE)
SWDEST   = $(MYSQLDATAdir)
ERRMSGS  = $(MYSQLSHARE)

install: default
	@ for f in $(DATAFILES); do \
		$(ECHO) "$(INSTALL) -idb $(SWSUBSYS) -F $(SWDEST) $$f"; \
		$(INSTALL) -F $(SWDEST) -idb $(SWSUBSYS) -m 644 $$f;\
		$(ECHO) "$(INSTALL) -idb $(SWSUBSYS) -F $(SSDBSETUPDIR) $$f"; \
		$(INSTALL) -F $(SSDBSETUPDIR) -idb $(SWSUBSYS) -m 644 $$f;\
        done
	@ for f in $(ERRMSGFILES); do \
		$(ECHO) "$(INSTALL) -idb $(SWSUBSYS) -F $(ERRMSGS) $$f"; \
		$(INSTALL) -F $(ERRMSGS) -idb $(SWSUBSYS) -m 644 $$f;\
	done
include $(COMMONRULES)
