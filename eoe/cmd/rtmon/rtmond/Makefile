#!smake
OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs

CFILES = \
    access.c \
    client.c \
    io.c \
    network.c \
    pid.c \
    protosw.c \
    protocol2.c \
    rtmond.c \
    timer.c \
    tstamp.c \
    ${NULL}
LCDEFS	+= -D_KMEMUSER -DUSE_SPROC 
#LCDEFS	+= -D_KMEMUSER
TARGETS = rtmond
LLDLIBS= -lrtmon
#LLDLIBS= -lrtmon -lpthread

# suppress "weak definition" complaints when linking w/ -lpthread
LLDOPTS=-Wl,-woff,134

default:	${CFILES:.c=.o}
		$(CCF) -o rtmond ${CFILES:.c=.o} $(LDOPTS) $(LLDLIBS)

include $(COMMONRULES)

install:	default
	$(INSTALL) -idb EOE -F /usr/etc '$(TARGETS)'
	$(INSTALL) -idb EOE -F /etc/init.d -src rtmondrc.sh rtmond
	$(INSTALL) -idb EOE -F /etc/config -src rtmond.config rtmond
	$(INSTALL) -idb 'EOE config(suggest)' -F /etc/config rtmond.options
	$(INSTALL) -F /etc/rc2.d -lns ../init.d/rtmond S98rtmond
