#ident "$Revision: 1.3 $"

SSN_SRC_DEPTH = ..

include $(ROOT)/usr/include/make/commondefs
include $(SSN_SRC_DEPTH)/include/ssncommondefs

CFILES =      qpage.c    usersnpp.c  srvrsnpp.c  queue.c \
              config.c   util.c      ixo.c       ident.c \
              readmail.c sendmail.c

CLNTOBJECTS = qpageclnt.o usersnppclnt.o utilclnt.o readmailclnt.o

INITDPATH   = /etc/init.d
LINKPATH    = ../init.d
RC2PATH     = /etc/rc2.d
RC0PATH     = /etc/rc0.d
RC2SCRIPT   = S99qpage
RC0SCRIPT   = K01qpage
CONFIGPATH  = /etc/config

TARGETS = qpage.d qpage

default: $(TARGETS)

include $(COMMONRULES)

qpage.d: $(OBJECTS) Makefile
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

qpage: $(CLNTOBJECTS) Makefile
	$(CCF) $(CLNTOBJECTS) $(LDFLAGS) -o $@

qpageclnt.o: qpage.c
	$(CC) -DCLIENT_ONLY $(CFLAGS) -c qpage.c -o $@

usersnppclnt.o: usersnpp.c
	$(CC) -DCLIENT_ONLY $(CFLAGS) -c usersnpp.c -o $@

utilclnt.o: util.c
	$(CC) -DCLIENT_ONLY $(CFLAGS) -c util.c -o $@

readmailclnt.o: readmail.c
	$(CC) -DCLIENT_ONLY $(CFLAGS) -c readmail.c -o $@

install: default
	${INSTALL} ${IDB_TAG} -F $(INITDPATH) qpageserver
	${INSTALL} ${IDB_TAG} -F $(RC2PATH) -lns $(LINKPATH)/qpageserver $(RC2SCRIPT)
	${INSTALL} ${IDB_TAG} -F $(RC0PATH) -lns $(LINKPATH)/qpageserver $(RC0SCRIPT)
	${INSTALL} ${IDB_TAG} -F $(ETCPATH) qpage.cf
	${INSTALL} -idb "config(noupdate)" ${IDB_TAG} -F $(CONFIGPATH) -src qpage.chkconfig quickpage
	${INSTALL} ${IDB_TAG} -F $(USRETCPATH) $(TARGETS)
