#!smake
# Berkeley Mail
#	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/Mail/RCS/Makefile,v 1.20 1996/08/22 21:53:37 danc Exp $"
#

BASEVERSION=n32bit
WANTPARALLEL=yes-please
include	$(ROOT)/usr/include/make/cmdcommondefs
MKDEPFILE=Makedepend

OS=	USG
LCOPTS=	-D$(OS) -D_OLD_TERMIOS -signed
CTAGS=	ctags -w
LWOFF=	,1116

MSRCS=	$(OS).local.c aux.c cmd1.c cmd2.c cmd3.c cmdtab.c collect.c \
	edit.c fio.c getname.c glob.c head.c lex.c list.c lock.c main.c \
	names.c quit.c send.c strings.c temp.c tty.c \
	vars.c version.c
MOBJS=	$(MSRCS:.c=.o)
FSRCS	=fmt.c
FOBJS=	$(FSRCS:.c=.o) head.o
VSRCS	=vacation.c
VOBJS=	$(VSRCS:.c=.o)
CFILES=	$(MSRCS) $(FSRCS) $(VSRCS)
DIRS=	misc

COMMANDS=Mail fmt vacation

default:$(TARGETS)

include $(CMDCOMMONRULES)

n32bitinstall: $(TARGETS)
	$(INSTALL) -g mail -m 2755 -F /usr/sbin Mail
	$(INSTALL) -F /usr/sbin vacation fmt
	$(INSTALL) -F /usr/lib -m 444 -idb "config(suggest)" Mail.rc
	$(INSTALL) -F /usr/lib -m 444 Mail.help
	$(INSTALL) -F /usr/lib -m 444 -src Mail.Thelp "Mail.help.~"

Mail:	$(MOBJS)
	$(CCF) $(MOBJS) $(LDFLAGS) -o Mail

fmt:	$(FOBJS)
	$(CCF) $(FOBJS) $(LDFLAGS) -o fmt

vacation: $(VOBJS)
	  $(CCF) $(VOBJS) $(LDFLAGS) -o vacation
