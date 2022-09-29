#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/fr_CA
	${INSTALL} -F /usr/lib/locale/fr_CA LC_COLLATE
	${INSTALL} -F /usr/lib/locale/fr_CA LC_CTYPE
	${INSTALL} -F /usr/lib/locale/fr_CA LC_MONETARY
	${INSTALL} -F /usr/lib/locale/fr_CA LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/fr_CA LC_TIME 
	${INSTALL} -F /usr/lib/locale/fr_CA chrtbl_fr_CA
	${INSTALL} -F /usr/lib/locale/fr_CA colltbl_fr_CA
	${INSTALL} -F /usr/lib/locale/fr_CA montbl_fr_CA
	${INSTALL} -F /usr/lib/locale/fr_CA nl_lang_fr_CA
	${INSTALL} -dir /usr/lib/locale/fr_CA/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/fr_CA/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_fr_CA

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_fr_CA

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_fr_CA

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_fr_CA.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_fr_CA
	mkmsgs -o nl_lang_fr_CA Xopen_info
