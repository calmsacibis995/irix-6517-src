#
#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_CTYPE LC_NUMERIC LC_COLLATE LC_MONETARY Xopen_info
LDIRT=ctype.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -dir /usr/lib/locale/sv
	${INSTALL} -F /usr/lib/locale/sv LC_COLLATE
	${INSTALL} -F /usr/lib/locale/sv LC_CTYPE
	${INSTALL} -F /usr/lib/locale/sv LC_MONETARY
	${INSTALL} -F /usr/lib/locale/sv LC_NUMERIC
	${INSTALL} -F /usr/lib/locale/sv LC_TIME 
	${INSTALL} -F /usr/lib/locale/sv chrtbl_sv
	${INSTALL} -F /usr/lib/locale/sv colltbl_sv
	${INSTALL} -F /usr/lib/locale/sv montbl_sv
	${INSTALL} -F /usr/lib/locale/sv nl_lang_sv
	${INSTALL} -dir /usr/lib/locale/sv/LC_MESSAGES
	${INSTALL} -f /usr/lib/locale/sv/LC_MESSAGES Xopen_info

LC_MONETARY:../montbl/hostmontbl
	../montbl/hostmontbl montbl_sv

../montbl/hostmontbl:
	cd ../montbl; \
	$(MAKE) hostmontbl

LC_COLLATE:../colltbl/hostcolltbl
	../colltbl/hostcolltbl colltbl_sv

../colltbl/hostcolltbl:
	cd ../colltbl; \
	$(MAKE) hostcolltbl

LC_CTYPE LC_NUMERIC:../chrtbl/hostchrtbl
	../chrtbl/hostchrtbl chrtbl_sv

../chrtbl/hostchrtbl:
	cd ../chrtbl; \
	$(MAKE) hostchrtbl

# Xopen_info is got from nl_lang_sv.
# For the present we statically use the compiler and checkin Xopen_info
# If the compiler mkmsgs format changes, we will need to change
# Xopen_info.
#
Xopen_info: nl_lang_sv
	mkmsgs -o nl_lang_sv Xopen_info
