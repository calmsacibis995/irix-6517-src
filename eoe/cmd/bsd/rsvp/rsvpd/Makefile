#!smake
#
# Make the Resource Reservation Daemon
#
# $Revision: 1.13 $

include $(ROOT)/usr/include/make/commondefs

#
# add these files to the OBJS list and -DRTAP to DEFINES to use
# interactive debugging rsvpd.
# rsvp_rtap.o rtap_cmds.o Pm_parse.o
#

OBJS =	rsvp_debug.o rsvp_path.o rsvp_resv.o rsvp_util.o \
	rsvp_timer.o rsvp_err.o rsvp_main.o rsvp_mstat.o \
	rsvp_specs.o rsvp_api.o  rsvp_bord.o rsvp_rsrr.o rsvp_netio.o \
	rsvp_unicast.o rsvp_print.o rsvp_key.o rsa_md5c.o tc_isps.o \
	rsvp_mib.o rsvp_mglue.o license.o

#
# Put a little more information in the rsvp_vers.c file
#
SGIINFO =SGI rsvpd 2.0 based on IRIX6.5 and ISI rel4.1a6

CFILES=$(OBJS:.o=.c)
LDIRT = rsvp_vers.c rsvp_vers.o rtap_cmds.c Pm_parse.c

TARGETS	= rsvpd

# OPTIMIZER = -g

# RSVP_ISI already defined in ip_mroute.h
# DEFINES= -DSCHEDULE -DSECURITY -DSTATS -D_LICENSED -fullwarn
DEFINES= -DSCHEDULE -DSECURITY -DSTATS -D_MIB -D_PEER -fullwarn
LCDEFS=	-I$(ROOT)/usr/include -I$(ROOT)/usr/include/rsvp -I./ -DBSD=43 -D_BSD_SIGNALS $(DEFINES) -signed
LLDOPTS= -L.

default all: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/etc rsvpd

rsvpd: rsvp_vers.o ${OBJS} ${RTAPOBJS}
	${CCF} -o $@ ${LDFLAGS} ${OBJS} rsvp_vers.o -lrsvp -lpeer -llmsgi

rsvp_vers.c : $(CFILES)
	rm -f rsvp_vers.c
	date +'char vers_comp_date[]="${SGIINFO} compiled on %h %d %Y";' > rsvp_vers.c

Pm_parse.c:
	ln -s ../tools/Pm_parse.c Pm_parse.c

rtap_cmds.c:
	ln -s ../tools/rtap_cmds.c rtap_cmds.c
