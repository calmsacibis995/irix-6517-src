#!smake
#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

LOCALE_TARGETS	= LC_CTYPE LC_COLLATE LC_NUMERIC LC_MONETARY
MSG_TARGETS	= Xopen_info
TARGETS		= $(LOCALE_TARGETS) $(MSG_TARGETS)

LDIRT	= ctype.c

LOCALE	= tr

CHRTBL	= $(TOOLROOTSAFE) $(TOOLROOT)/usr/bin/chrtbl
COLLTBL	= $(TOOLROOTSAFE) $(TOOLROOT)/usr/bin/colltbl
MKMSGS	= $(TOOLROOTSAFE) $(TOOLROOT)/usr/bin/mkmsgs
MONTBL	= $(TOOLROOTSAFE) $(TOOLROOT)/usr/bin/montbl

CHRTBL_SRC	= chrtbl_$(LOCALE)
COLLTBL_SRC	= colltbl_$(LOCALE)
MSG_SRC		= nl_lang_$(LOCALE)
MONTBL_SRC	= montbl_$(LOCALE)

SOURCES	= $(CHRTBL_SRC) $(COLLTBL_SRC) $(MSG_SRC) $(MONTBL_SRC)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -dir /usr/lib/locale/$(LOCALE)
	$(INSTALL) -F /usr/lib/locale/$(LOCALE) $(LOCALE_TARGETS) LC_TIME
	$(INSTALL) -dir /usr/lib/locale/$(LOCALE)/LC_MESSAGES
	$(INSTALL) -F /usr/lib/locale/$(LOCALE)/LC_MESSAGES $(MSG_TARGETS)
	$(INSTALL) -F /usr/lib/locale/$(LOCALE) $(SOURCES)

LC_NUMERIC: LC_CTYPE
LC_CTYPE: $(CHRTBL_SRC)
	$(CHRTBL) $(CHRTBL_SRC)

LC_COLLATE: $(COLLTBL_SRC)
	$(COLLTBL) $(COLLTBL_SRC)

LC_MONETARY: $(MONTBL_SRC)
	$(MONTBL) $(MONTBL_SRC)

LC_TIME:

Xopen_info: $(MSG_SRC)
	$(MKMSGS) -o $(MSG_SRC) Xopen_info
