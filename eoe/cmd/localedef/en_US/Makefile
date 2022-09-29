#
#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/en_US
	${INSTALL} -F /usr/lib/locale/en_US chrtbl_en_US
	${INSTALL} -F /usr/lib/locale/en_US LC_CTYPE
	${INSTALL} -F /usr/lib/locale/en_US LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/en_US colltbl_en_US
	${INSTALL} -F /usr/lib/locale/en_US LC_COLLATE
	${INSTALL} -F /usr/lib/locale/en_US LC_TIME nl_lang_en_US
	${INSTALL} -F /usr/lib/locale/en_US montbl_en_US
	${INSTALL} -F /usr/lib/locale/en_US LC_MONETARY
	${INSTALL} -dir /usr/lib/locale/en_US/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/en_US/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_en_US

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_en_US

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_en_US

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_en_US.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
# Xopen_info: nl_lang_en_US
#	mkmsgs -o nl_lang_en_US Xopen_info
