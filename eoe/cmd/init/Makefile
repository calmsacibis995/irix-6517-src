#
include	$(ROOT)/usr/include/make/commondefs

INITTARGET=init
AMSDRTARGET=amsdreasons
TARGETS=$(INITTARGET) $(AMSDRTARGET)
INITOBJS=init.o initavailmon.o
LCOPTS=-G0 -Wl,-rdata_shared -fullwarn
LLDOPTS=-nostdlib -fullwarn -Wl,-I,$(ROOTRLDNAME)
OPTIMIZER=-Ofast=ip22_4k -IPA

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(INITTARGET)
	$(INSTALL) -F /etc -ln ../sbin/init init
	$(INSTALL) -F /etc -ln ../sbin/init telinit
	$(INSTALL) -F /sbin -lns init telinit
	$(INSTALL) -F /usr/etc $(AMSDRTARGET) 
	$(INSTALL) -F /var/adm/avail -lns /usr/etc/amsdreasons amsdreasons
	$(INSTALL) -F /var/adm/avail -lns /usr/etc/amsdreasons sdreasons

init:	$(INITOBJS)
	$(CCF) $(INITOBJS) $(LDFLAGS) -o init

amsdreasons: amsdreasons.o
	$(CCF) -DSDREASONS_CMD amsdreasons.o $(LDFLAGS) -o amsdreasons -lgen
amsdreasons.o: initavailmon.c
	$(CCF) -c -DSDREASONS_CMD initavailmon.c -o amsdreasons.o
