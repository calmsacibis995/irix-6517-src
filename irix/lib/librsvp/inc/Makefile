#!smake
#ident "$Revision: 1.8 $

DEPTH=.
include $(ROOT)/usr/include/make/libleafdefs

#
# headers shared by rsvpd, librsvp and used by user applications
# Actually, rsvp_types.h and rsvp_intserv.c is also needed by
# user applications.
#
TOP_HEADERS=rapi_lib.h rapi_err.h

#
# headers shared by rsvpd and librsvp
# tc_isps.h is now in irix/kern/bsd/net
#
RSVP_HEADERS= config.h rsrr.h rsrr_var.h rsvp.h rsvp_TCif.h \
	rsvp_api.h rsvp_daemon.h rsvp_global.h rsvp_mac.h rsvp_mstat.h \
	rsvp_proto.h rsvp_specs.h rsvp_types.h rsvp_var.h rsvp_lpm.h \
	rsvp_intserv.h rsa_md5c.h rsvp_stats.h rsa_md5_global.h Pm_parse.h \
	rsvp_mib.h peer_mib.h

include ${LIBLEAFRULES}

install:headers exports

exports:

headers:
	$(INSTALL) -F /usr/include -m 444 $(TOP_HEADERS)
	$(INSTALL) -F /usr/include/rsvp -m 444 $(RSVP_HEADERS)
