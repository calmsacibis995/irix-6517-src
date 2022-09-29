#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/de_AT
	${INSTALL} -F /usr/lib/locale/de_AT LC_COLLATE
	${INSTALL} -F /usr/lib/locale/de_AT LC_CTYPE
	${INSTALL} -F /usr/lib/locale/de_AT LC_MONETARY
	${INSTALL} -F /usr/lib/locale/de_AT LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/de_AT LC_TIME 
	${INSTALL} -F /usr/lib/locale/de_AT chrtbl_de_AT
	${INSTALL} -F /usr/lib/locale/de_AT colltbl_de_AT
	${INSTALL} -F /usr/lib/locale/de_AT montbl_de_AT
	${INSTALL} -F /usr/lib/locale/de_AT nl_lang_de_AT
	${INSTALL} -dir /usr/lib/locale/de_AT/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/de_AT/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_de_AT

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_de_AT

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_de_AT

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_de_AT.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_de_AT
	mkmsgs -o nl_lang_de_AT Xopen_info
