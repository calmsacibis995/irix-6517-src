#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/pl
	${INSTALL} -F /usr/lib/locale/pl LC_COLLATE
	${INSTALL} -F /usr/lib/locale/pl LC_CTYPE
	${INSTALL} -F /usr/lib/locale/pl LC_MONETARY
	${INSTALL} -F /usr/lib/locale/pl LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/pl LC_TIME 
	${INSTALL} -F /usr/lib/locale/pl chrtbl_pl
	${INSTALL} -F /usr/lib/locale/pl colltbl_pl
	${INSTALL} -F /usr/lib/locale/pl montbl_pl
	${INSTALL} -F /usr/lib/locale/pl nl_lang_pl
	${INSTALL} -dir /usr/lib/locale/pl/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/pl/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_pl

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_pl

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_pl

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_pl.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_pl
	mkmsgs -o nl_lang_pl Xopen_info
