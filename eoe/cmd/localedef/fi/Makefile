#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/fi
	${INSTALL} -F /usr/lib/locale/fi LC_COLLATE
	${INSTALL} -F /usr/lib/locale/fi LC_CTYPE
	${INSTALL} -F /usr/lib/locale/fi LC_MONETARY
	${INSTALL} -F /usr/lib/locale/fi LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/fi LC_TIME 
	${INSTALL} -F /usr/lib/locale/fi chrtbl_fi
	${INSTALL} -F /usr/lib/locale/fi colltbl_fi
	${INSTALL} -F /usr/lib/locale/fi montbl_fi
	${INSTALL} -F /usr/lib/locale/fi nl_lang_fi
	${INSTALL} -dir /usr/lib/locale/fi/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/fi/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_fi

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_fi

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_fi

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_fi.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_fi
	mkmsgs -o nl_lang_fi Xopen_info
