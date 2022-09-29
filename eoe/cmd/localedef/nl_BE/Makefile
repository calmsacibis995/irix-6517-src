#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/nl_BE
	${INSTALL} -F /usr/lib/locale/nl_BE LC_COLLATE
	${INSTALL} -F /usr/lib/locale/nl_BE LC_CTYPE
	${INSTALL} -F /usr/lib/locale/nl_BE LC_MONETARY
	${INSTALL} -F /usr/lib/locale/nl_BE LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/nl_BE LC_TIME 
	${INSTALL} -F /usr/lib/locale/nl_BE chrtbl_nl_BE
	${INSTALL} -F /usr/lib/locale/nl_BE colltbl_nl_BE
	${INSTALL} -F /usr/lib/locale/nl_BE montbl_nl_BE
	${INSTALL} -F /usr/lib/locale/nl_BE nl_lang_nl_BE
	${INSTALL} -dir /usr/lib/locale/nl_BE/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/nl_BE/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_nl_BE

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_nl_BE

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_nl_BE

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_nl_BE.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_nl_BE
	mkmsgs -o nl_lang_nl_BE Xopen_info
