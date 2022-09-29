#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/en_AU
	${INSTALL} -F /usr/lib/locale/en_AU LC_COLLATE
	${INSTALL} -F /usr/lib/locale/en_AU LC_CTYPE
	${INSTALL} -F /usr/lib/locale/en_AU LC_MONETARY
	${INSTALL} -F /usr/lib/locale/en_AU LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/en_AU LC_TIME 
	${INSTALL} -F /usr/lib/locale/en_AU chrtbl_en_AU
	${INSTALL} -F /usr/lib/locale/en_AU colltbl_en_AU
	${INSTALL} -F /usr/lib/locale/en_AU montbl_en_AU
	${INSTALL} -F /usr/lib/locale/en_AU nl_lang_en_AU
	${INSTALL} -dir /usr/lib/locale/en_AU/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/en_AU/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_en_AU

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_en_AU

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_en_AU

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_en_AU.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_en_AU
	mkmsgs -o nl_lang_en_AU Xopen_info
