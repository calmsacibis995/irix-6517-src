#!smake
#
#	Makefile for gated
#
# $Revision: 1.18 $

include $(ROOT)/usr/include/make/commondefs

GATED	= af.c egp.c ext.c hello.c if.c icmp.c inet.c init.c main.c \
	rip.c rt_control.c rt_egp.c rt_if.c rt_init.c \
	rt_nss.c rt_table.c sgmp.c snmp.c str.c trace.c
RIPQUERY=ripquery.c

CFILES	=$(GATED) $(RIPQUERY)

INCLUDES= af.h defs.h egp.h egp_param.h hello.h if.h include.h \
	rip.h routed.h rt_control.h rt_table.h \
	snmp.h trace.h

TARGETS =gated ripquery

SGMP_INCLUDE=
SNMP_INCLUDE=

# do not worry about multiple definitions of global data
CVERSION=-cckr
# do not worry about nested comments or functions that are really void
LCOPTS  =-signed -woff 1009,1116
LCDEFS	=-DBSD43
LLDOPTS =-Wl,-T,10000000,-D,10040000
LDIRT	=sgi_version.c

default:$(TARGETS)

include $(COMMONRULES)

install:default
	$(INSTALL) -F /usr/etc gated
	$(INSTALL) -idb "config(suggest)" \
		-u root -g sys -m 644 -F /etc gated.conf
	$(INSTALL) -F /usr/etc -lns ../../etc/gated.conf gated.conf

gated: $(GATED:.c=.o) sgi_version.o
	$(CCF) $(GATED:.c=.o) sgi_version.o $(LDFLAGS) -o $@

ripquery: $(RIPQUERY:.c=.o)
	$(CCF) $(RIPQUERY:.c=.o) $(LDFLAGS) -o $@

sgi_version.c: version.c
	rm -f $@
	grep version version.c > $@
	@echo "char *build_date = \"`date`\";" >> $@
