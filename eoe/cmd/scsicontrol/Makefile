#ident "$Revision: 1.14 $"

include $(ROOT)/usr/include/make/commondefs

LLDLIBS = -lds -Wl,-woff,84
LCFLAGS = -fullwarn

CFILES=scsicontrol.c scsiha.c scsiquiesce.c scsifo.c
TARGETS= scsiquiesce scsicontrol scsiha scsifo

default:$(TARGETS) $(QL_TARGETS) 

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
	${INSTALL} -F /usr/share/src/irix/examples/scsi -m 444 -src Makefile.gifts Makefile
	${INSTALL} -F /usr/share/src/irix/examples/scsi -m 444 $(CFILES)
