#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/fr
	${INSTALL} -F /usr/lib/locale/fr LC_COLLATE
	${INSTALL} -F /usr/lib/locale/fr LC_CTYPE
	${INSTALL} -F /usr/lib/locale/fr LC_MONETARY
	${INSTALL} -F /usr/lib/locale/fr LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/fr LC_TIME 
	${INSTALL} -F /usr/lib/locale/fr chrtbl_fr
	${INSTALL} -F /usr/lib/locale/fr colltbl_fr
	${INSTALL} -F /usr/lib/locale/fr montbl_fr
	${INSTALL} -F /usr/lib/locale/fr nl_lang_fr
	${INSTALL} -dir /usr/lib/locale/fr/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/fr/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_fr

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_fr

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_fr

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_fr.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_fr
	mkmsgs -o nl_lang_fr Xopen_info
