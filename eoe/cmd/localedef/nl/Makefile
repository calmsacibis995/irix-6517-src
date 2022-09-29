#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/nl
	${INSTALL} -F /usr/lib/locale/nl LC_COLLATE
	${INSTALL} -F /usr/lib/locale/nl LC_CTYPE
	${INSTALL} -F /usr/lib/locale/nl LC_MONETARY
	${INSTALL} -F /usr/lib/locale/nl LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/nl LC_TIME 
	${INSTALL} -F /usr/lib/locale/nl chrtbl_nl
	${INSTALL} -F /usr/lib/locale/nl colltbl_nl
	${INSTALL} -F /usr/lib/locale/nl montbl_nl
	${INSTALL} -F /usr/lib/locale/nl nl_lang_nl
	${INSTALL} -dir /usr/lib/locale/nl/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/nl/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_nl

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_nl

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_nl

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_nl.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_nl
	mkmsgs -o nl_lang_nl Xopen_info
