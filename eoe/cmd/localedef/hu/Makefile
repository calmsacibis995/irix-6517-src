#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/hu
	${INSTALL} -F /usr/lib/locale/hu LC_COLLATE
	${INSTALL} -F /usr/lib/locale/hu LC_CTYPE
	${INSTALL} -F /usr/lib/locale/hu LC_MONETARY
	${INSTALL} -F /usr/lib/locale/hu LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/hu LC_TIME 
	${INSTALL} -F /usr/lib/locale/hu chrtbl_hu
	${INSTALL} -F /usr/lib/locale/hu colltbl_hu
	${INSTALL} -F /usr/lib/locale/hu montbl_hu
	${INSTALL} -F /usr/lib/locale/hu nl_lang_hu
	${INSTALL} -dir /usr/lib/locale/hu/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/hu/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_hu

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_hu

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_hu

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_hu.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_hu
	mkmsgs -o nl_lang_hu Xopen_info
