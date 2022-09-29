#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/es_MX
	${INSTALL} -F /usr/lib/locale/es_MX LC_COLLATE
	${INSTALL} -F /usr/lib/locale/es_MX LC_CTYPE
	${INSTALL} -F /usr/lib/locale/es_MX LC_MONETARY
	${INSTALL} -F /usr/lib/locale/es_MX LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/es_MX LC_TIME 
	${INSTALL} -F /usr/lib/locale/es_MX chrtbl_es_MX
	${INSTALL} -F /usr/lib/locale/es_MX colltbl_es_MX
	${INSTALL} -F /usr/lib/locale/es_MX montbl_es_MX
	${INSTALL} -F /usr/lib/locale/es_MX nl_lang_es_MX
	${INSTALL} -dir /usr/lib/locale/es_MX/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/es_MX/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_es_MX

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_es_MX

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_es_MX

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_es_MX.
# For the pres_MX we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes_MX, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_es_MX
	mkmsgs -o nl_lang_es_MX Xopen_info
