#!smake
#
# Make the Resource Reservation Daemon
#
# $Revision: 1.11 $

include $(ROOT)/usr/include/make/commondefs

IGMP_FILES=	igmp.c inet.c kern.c
IGMP_OBJS=	$(IGMP_FILES:.c=.o)
ROUTER_FILES=	callout.c config.c main.c prune.c route.c vif.c version.c
ROUTER_OBJS=	$(ROUTER_FILES:.c=.o)
CFILES=		$(IGMP_FILES) $(ROUTER_FILES)

RSVPFILTER_OBJS = filter.o rsvp_vers.o
RTAP_OBJS = rtap_main.o rtap_cmds.o Pm_parse.o rsvp_vers.o
RSTAT_OBJS = rstat.o netutil.o rsvp_specs.o rsvp_print.o rsvp_vers.o
RSVPEEP_OBJS = rsvpeep.o netutil.o rsvp_specs.o rsvp_print.o rsvp_vers.o
PSIFCONFIG_OBJS = psifconfig.o rsvp_vers.o
TARGETS	=rtap rsvpeep rsvpfilter rstat psifconfig

# RSVP_ISI already defined in ip_mroute.h
DEFINES= -DSCHEDULE -DRSRR_RCN
LCDEFS=	-DBSD=43 -D_BSD_SIGNALS $(DEFINES)

LDIRT = rsvp_specs.c rsvp_print.c rsvp_vers.c

#
# Put a little more info in the rsvp_vers.c file
#
SGIINFO =SGI rsvp tools for IRIX6.5 based on ISI rel4.1a6

#
# Do this until the headers are installed in $(ROOT)
#
LCDEFS += -I$(ROOT)/usr/include/rsvp

default all: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/etc rstat rtap rsvpeep psifconfig rsvpfilter

rsvp_vers.c:
	rm -f rsvp_vers.c
	date +'char comp_date[]="${SGIINFO} compiled on %h %d %Y ";' > rsvp_vers.c

rtap: ${RTAP_OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} ${RTAP_OBJS} -lrsvp -o rtap

rsvpfilter: ${RSVPFILTER_OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} ${RSVPFILTER_OBJS} -lrsvp -o rsvpfilter

rsvpeep: ${RSVPEEP_OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} ${RSVPEEP_OBJS} -lrsvp -o rsvpeep

rstat: ${RSTAT_OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} ${RSTAT_OBJS} -lrsvp -o rstat

psifconfig: ${PSIFCONFIG_OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} ${PSIFCONFIG_OBJS} -o $@

rsvp_specs.c:
	ln -s ../rsvpd/rsvp_specs.c rsvp_specs.c

rsvp_print.c:
	ln -s ../rsvpd/rsvp_print.c rsvp_print.c
