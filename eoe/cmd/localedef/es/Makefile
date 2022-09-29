#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/es
	${INSTALL} -F /usr/lib/locale/es LC_COLLATE
	${INSTALL} -F /usr/lib/locale/es LC_CTYPE
	${INSTALL} -F /usr/lib/locale/es LC_MONETARY
	${INSTALL} -F /usr/lib/locale/es LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/es LC_TIME 
	${INSTALL} -F /usr/lib/locale/es chrtbl_es
	${INSTALL} -F /usr/lib/locale/es colltbl_es
	${INSTALL} -F /usr/lib/locale/es montbl_es
	${INSTALL} -F /usr/lib/locale/es nl_lang_es
	${INSTALL} -dir /usr/lib/locale/es/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/es/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_es

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_es

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_es

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_es.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_es
	mkmsgs -o nl_lang_es Xopen_info
