#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/es_AR
	${INSTALL} -F /usr/lib/locale/es_AR LC_COLLATE
	${INSTALL} -F /usr/lib/locale/es_AR LC_CTYPE
	${INSTALL} -F /usr/lib/locale/es_AR LC_MONETARY
	${INSTALL} -F /usr/lib/locale/es_AR LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/es_AR LC_TIME 
	${INSTALL} -F /usr/lib/locale/es_AR chrtbl_es_AR
	${INSTALL} -F /usr/lib/locale/es_AR colltbl_es_AR
	${INSTALL} -F /usr/lib/locale/es_AR montbl_es_AR
	${INSTALL} -F /usr/lib/locale/es_AR nl_lang_es_AR
	${INSTALL} -dir /usr/lib/locale/es_AR/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/es_AR/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_es_AR

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_es_AR

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_es_AR

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_es_AR.
# For the pres_AR we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes_AR, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_es_AR
	mkmsgs -o nl_lang_es_AR Xopen_info
