#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/pt_BR
	${INSTALL} -F /usr/lib/locale/pt_BR chrtbl_pt_BR
	${INSTALL} -F /usr/lib/locale/pt_BR LC_CTYPE
	${INSTALL} -F /usr/lib/locale/pt_BR LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/pt_BR colltbl_pt_BR
	${INSTALL} -F /usr/lib/locale/pt_BR LC_COLLATE
	${INSTALL} -F /usr/lib/locale/pt_BR LC_TIME nl_lang_pt_BR
	${INSTALL} -F /usr/lib/locale/pt_BR montbl_pt_BR
	${INSTALL} -F /usr/lib/locale/pt_BR LC_MONETARY
	${INSTALL} -dir /usr/lib/locale/pt_BR/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/pt_BR/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_pt_BR

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_pt_BR

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_pt_BR

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_pt_BR.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_pt_BR
	mkmsgs -o nl_lang_pt_BR Xopen_info
