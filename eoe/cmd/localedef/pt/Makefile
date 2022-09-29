#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/pt
	${INSTALL} -F /usr/lib/locale/pt LC_COLLATE
	${INSTALL} -F /usr/lib/locale/pt LC_CTYPE
	${INSTALL} -F /usr/lib/locale/pt LC_MONETARY
	${INSTALL} -F /usr/lib/locale/pt LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/pt LC_TIME 
	${INSTALL} -F /usr/lib/locale/pt chrtbl_pt
	${INSTALL} -F /usr/lib/locale/pt colltbl_pt
	${INSTALL} -F /usr/lib/locale/pt montbl_pt
	${INSTALL} -F /usr/lib/locale/pt nl_lang_pt
	${INSTALL} -dir /usr/lib/locale/pt/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/pt/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_pt

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_pt

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_pt

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_pt.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_pt
	mkmsgs -o nl_lang_pt Xopen_info
