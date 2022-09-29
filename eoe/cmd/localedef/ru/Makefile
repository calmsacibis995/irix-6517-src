#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/ru
	${INSTALL} -F /usr/lib/locale/ru LC_COLLATE
	${INSTALL} -F /usr/lib/locale/ru LC_CTYPE
	${INSTALL} -F /usr/lib/locale/ru LC_MONETARY
	${INSTALL} -F /usr/lib/locale/ru LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/ru LC_TIME 
	${INSTALL} -F /usr/lib/locale/ru chrtbl_ru
	${INSTALL} -F /usr/lib/locale/ru colltbl_ru
	${INSTALL} -F /usr/lib/locale/ru montbl_ru
	${INSTALL} -F /usr/lib/locale/ru nl_lang_ru
	${INSTALL} -dir /usr/lib/locale/ru/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/ru/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_ru

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_ru

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_ru

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_ru.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_ru
	mkmsgs -o nl_lang_ru Xopen_info
