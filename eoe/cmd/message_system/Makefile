#!smake
#
# makefile for cmd/message_system
#
#ident "$Revision: 1.14 $"

BASEVERSION	= n32bit
#ifndef SRC_PRD
ALTVERSIONS	= troot
#endif
WANTPARALLEL	= yes-please
include $(ROOT)/usr/include/make/cmdcommondefs

CFILES          = explain.c caterr.c
COMFILES        = cmdmsgfmt.c cmdgetmsg.c cmdmsg.c

COMOBJS         = $(COMFILES:.c=.o)

LDIRT		= msgsys.exp* msgsys.cat* *.i

MSG_SRC		= errmsg
MSG_CAT_NAME	= msgsys
EXPLAIN_CAT_SUFFIX = .exp
MESSAGE_CAT_SUFFIX = .cat
MKMSGS_SUFFIX	= .m

default: $(TARGETS) $(MSG_CAT_NAME)$(MESSAGE_CAT_SUFFIX) $(MSG_CAT_NAME)$(EXPLAIN_CAT_SUFFIX)

headers: tmac.sg
	$(INSTALL) -m 644 -F /usr/share/lib/tmac tmac.sg

include $(CMDCOMMONRULES)
TLINKXARGS	+= -x tmac.sg

n32bitinstall: default
	$(INSTALL) -F /usr/bin $(COMMANDS)
	$(INSTALL) -F /usr/lib/locale/C/LC_MESSAGES \
		$(MSG_CAT_NAME)$(MESSAGE_CAT_SUFFIX)
	$(INSTALL) -F /usr/lib/locale/C/LC_MESSAGES \
		$(MSG_CAT_NAME)$(MESSAGE_CAT_SUFFIX)$(MKMSGS_SUFFIX)
	$(INSTALL) -F /usr/lib/locale/C/LC_MESSAGES \
		$(MSG_CAT_NAME)$(EXPLAIN_CAT_SUFFIX)
	$(INSTALL) -F /usr/lib/locale/C/LC_MESSAGES \
		$(MSG_CAT_NAME)$(EXPLAIN_CAT_SUFFIX)$(MKMSGS_SUFFIX)
	$(INSTALL) -F /usr/share/lib/tmac tmac.sg

trootinstall: default
	$(INSTALL) -F /usr/bin $(COMMANDS)
	$(INSTALL) -F /usr/lib/locale/C/LC_MESSAGES \
		$(MSG_CAT_NAME)$(MESSAGE_CAT_SUFFIX)
	$(INSTALL) -F /usr/lib/locale/C/LC_MESSAGES \
		$(MSG_CAT_NAME)$(MESSAGE_CAT_SUFFIX)$(MKMSGS_SUFFIX)
	$(INSTALL) -F /usr/lib/locale/C/LC_MESSAGES \
		$(MSG_CAT_NAME)$(EXPLAIN_CAT_SUFFIX)
	$(INSTALL) -F /usr/lib/locale/C/LC_MESSAGES \
		$(MSG_CAT_NAME)$(EXPLAIN_CAT_SUFFIX)$(MKMSGS_SUFFIX)

explain: explain.o $(COMOBJS)
	$(CCF) explain.o $(COMOBJS) $(LDFLAGS) -o $@

caterr: caterr.o $(COMOBJS)
	$(CCF) caterr.o $(COMOBJS) $(LDFLAGS) -o $@

$(MSG_CAT_NAME)$(MESSAGE_CAT_SUFFIX): $(MSG_SRC).msg $(MSG_SRC).h
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/caterr -c \
	$(MSG_CAT_NAME)$(MESSAGE_CAT_SUFFIX) \
	-s -Yc,$(TOOLROOT)/usr/bin/gencat \
	$(MSG_SRC).msg

$(MSG_CAT_NAME)$(EXPLAIN_CAT_SUFFIX): $(MSG_SRC).msg $(MSG_SRC).h
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/caterr -c \
	$(MSG_CAT_NAME)$(EXPLAIN_CAT_SUFFIX) \
	-s -e -Yc,$(TOOLROOT)/usr/bin/gencat \
	-Yn,$(TOOLROOT)/usr/bin/nroff \
	-Ym,$(ROOT)/usr/share/lib/tmac/tmac.sg $(MSG_SRC).msg
