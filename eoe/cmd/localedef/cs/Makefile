#
#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/cs
	${INSTALL} -F /usr/lib/locale/cs LC_COLLATE
	${INSTALL} -F /usr/lib/locale/cs LC_CTYPE
	${INSTALL} -F /usr/lib/locale/cs LC_MONETARY
	${INSTALL} -F /usr/lib/locale/cs LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/cs LC_TIME 
	${INSTALL} -F /usr/lib/locale/cs chrtbl_cs
	${INSTALL} -F /usr/lib/locale/cs colltbl_cs
	${INSTALL} -F /usr/lib/locale/cs montbl_cs
	${INSTALL} -F /usr/lib/locale/cs nl_lang_cs
	${INSTALL} -dir /usr/lib/locale/cs/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/cs/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_cs

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_cs

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl chrtbl_cs
	../chrtbl/hostchrtbl chrtbl_cs

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_cs.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_cs
	mkmsgs -o nl_lang_cs Xopen_info
