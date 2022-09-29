#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/da
	${INSTALL} -F /usr/lib/locale/da LC_COLLATE
	${INSTALL} -F /usr/lib/locale/da LC_CTYPE
	${INSTALL} -F /usr/lib/locale/da LC_MONETARY
	${INSTALL} -F /usr/lib/locale/da LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/da LC_TIME 
	${INSTALL} -F /usr/lib/locale/da chrtbl_da
	${INSTALL} -F /usr/lib/locale/da colltbl_da
	${INSTALL} -F /usr/lib/locale/da montbl_da
	${INSTALL} -F /usr/lib/locale/da nl_lang_da
	${INSTALL} -dir /usr/lib/locale/da/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/da/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_da

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_da

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_da

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_da.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_da
	mkmsgs -o nl_lang_da Xopen_info
