#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/el
	${INSTALL} -F /usr/lib/locale/el LC_COLLATE
	${INSTALL} -F /usr/lib/locale/el LC_CTYPE
	${INSTALL} -F /usr/lib/locale/el LC_MONETARY
	${INSTALL} -F /usr/lib/locale/el LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/el LC_TIME 
	${INSTALL} -F /usr/lib/locale/el chrtbl_el
	${INSTALL} -F /usr/lib/locale/el colltbl_el
	${INSTALL} -F /usr/lib/locale/el montbl_el
	${INSTALL} -F /usr/lib/locale/el nl_lang_el
	${INSTALL} -dir /usr/lib/locale/el/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/el/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_el

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_el

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_el

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_el.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_el
	mkmsgs -o nl_lang_el Xopen_info
