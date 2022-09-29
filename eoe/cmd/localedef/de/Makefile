#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/de
	${INSTALL} -F /usr/lib/locale/de LC_COLLATE
	${INSTALL} -F /usr/lib/locale/de LC_CTYPE
	${INSTALL} -F /usr/lib/locale/de LC_MONETARY
	${INSTALL} -F /usr/lib/locale/de LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/de LC_TIME 
	${INSTALL} -F /usr/lib/locale/de chrtbl_de
	${INSTALL} -F /usr/lib/locale/de colltbl_de
	${INSTALL} -F /usr/lib/locale/de montbl_de
	${INSTALL} -F /usr/lib/locale/de nl_lang_de
	${INSTALL} -dir /usr/lib/locale/de/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/de/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_de

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_de

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_de

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_de.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_de
	mkmsgs -o nl_lang_de Xopen_info
