#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/is
	${INSTALL} -F /usr/lib/locale/is LC_COLLATE
	${INSTALL} -F /usr/lib/locale/is LC_CTYPE
	${INSTALL} -F /usr/lib/locale/is LC_MONETARY
	${INSTALL} -F /usr/lib/locale/is LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/is LC_TIME 
	${INSTALL} -F /usr/lib/locale/is chrtbl_is
	${INSTALL} -F /usr/lib/locale/is colltbl_is
	${INSTALL} -F /usr/lib/locale/is montbl_is
	${INSTALL} -F /usr/lib/locale/is nl_lang_is
	${INSTALL} -dir /usr/lib/locale/is/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/is/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_is

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_is

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_is

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_is.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_is
	mkmsgs -o nl_lang_is Xopen_info
