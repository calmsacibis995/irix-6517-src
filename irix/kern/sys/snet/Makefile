#!smake
#
# Makefile for header file installation.
#
#ident "$Revision: 1.10 $"

DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/sys
.PATH:$(KPATH)

#
# List of "Public" header files.  These are headers included by user
# code or used by device driver module writers
#

PUBLIC_HEADERS=ll_proto.h ll_control.h timer.h system.h uint.h x25trace.h \
	x25_proto.h x25_control.h ip_control.h wan_proto.h wan_control.h

#
# List of headers that only system software utilities need
# These are public within SGI for internal development of other ISM's
# but are not shipped to customers.
#
FRIEND_HEADERS=llc2.h llc2match_if.h llc2match.h x25_proto.h x25_control.h \
	echo_control.h ip_control.h uint.h timer.h ll_proto.h ll_control.h \
	system.h wan_proto.h wan_control.h dl_control.h dl_proto.h \
	dl_version.h

#
# Any remaining headers not listed in one of the above lists is
# "Private" meaning that it is never used outside of the "dlpi"
# ISM.  They are not installed in any public include directory.
#

include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:

headers!
	$(INSTALL) -idb DLPI -m 444 -F /usr/include/sys/snet $(PUBLIC_HEADERS)
	${INSTALL} -idb "noship" -idb ROOT -m 444 \
		-F /usr/include/sys/snet ${FRIEND_HEADERS}
