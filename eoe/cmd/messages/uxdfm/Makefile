include $(ROOT)/usr/include/make/commondefs

PREF=dfm
TARGETS=$(PREF).str ux$(PREF)

default:$(TARGETS)

include $(COMMONRULES)

install:default
	$(INSTALL) -m 444 -F /usr/lib/locale/C/LC_MESSAGES ux$(PREF)

$(PREF).str:msgs
	sed -e '1,10d' $? >$@

ux$(PREF):$(PREF).str
	$(TOOLROOT)/usr/bin/mkmsgs -o $? $@
