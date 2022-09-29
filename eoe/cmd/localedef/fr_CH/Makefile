#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/fr_CH
	${INSTALL} -F /usr/lib/locale/fr_CH LC_COLLATE
	${INSTALL} -F /usr/lib/locale/fr_CH LC_CTYPE
	${INSTALL} -F /usr/lib/locale/fr_CH LC_MONETARY
	${INSTALL} -F /usr/lib/locale/fr_CH LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/fr_CH LC_TIME 
	${INSTALL} -F /usr/lib/locale/fr_CH chrtbl_fr_CH
	${INSTALL} -F /usr/lib/locale/fr_CH colltbl_fr_CH
	${INSTALL} -F /usr/lib/locale/fr_CH montbl_fr_CH
	${INSTALL} -F /usr/lib/locale/fr_CH nl_lang_fr_CH
	${INSTALL} -dir /usr/lib/locale/fr_CH/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/fr_CH/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_fr_CH

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_fr_CH

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_fr_CH

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_fr_CH.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_fr_CH
	mkmsgs -o nl_lang_fr_CH Xopen_info
