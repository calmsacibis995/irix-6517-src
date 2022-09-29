#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/no
	${INSTALL} -F /usr/lib/locale/no LC_COLLATE
	${INSTALL} -F /usr/lib/locale/no LC_CTYPE
	${INSTALL} -F /usr/lib/locale/no LC_MONETARY
	${INSTALL} -F /usr/lib/locale/no LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/no LC_TIME 
	${INSTALL} -F /usr/lib/locale/no chrtbl_no
	${INSTALL} -F /usr/lib/locale/no colltbl_no
	${INSTALL} -F /usr/lib/locale/no montbl_no
	${INSTALL} -F /usr/lib/locale/no nl_lang_no
	${INSTALL} -dir /usr/lib/locale/no/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/no/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_no

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_no

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_no

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_no.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_no
	mkmsgs -o nl_lang_no Xopen_info
