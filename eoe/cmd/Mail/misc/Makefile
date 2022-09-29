# install Mail control & help files on clover
# "$Revision: 1.4 $

include	$(ROOT)/usr/include/make/commondefs

HELPFILES = Mail.help Mail.Thelp Mail.rc

INST	= $(INSTALL) -F /usr/lib -m 444

default all:	$(HELPFILES)

include $(COMMONRULES)

install:	all
	$(INST) -idb "std.sw.unix config(suggest)" "Mail.rc"
	$(INST) -idb "std.sw.unix" "Mail.help"
	$(INST) -idb "std.sw.unix" -src Mail.Thelp "Mail.help.~"
