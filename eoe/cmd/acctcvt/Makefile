#!smake
#
# Makefile for acctcvt
#

#ident "$Revision: 1.2 $"

OBJECT_STYLE=N32

include $(ROOT)/usr/include/make/commondefs

TARGETS=acctcvt
CFILES=	main.c		\
	io.c		\
	header.c	\
	record.c	\
	sathdr.c	\
	extacct62.c	\
	extacct64.c	\
	extacct65.c	\
	svr4.c		\
	text.c		\
	$(NULL)
OBJFILES=$(CFILES:.c=.o)
LDIRT=acctcvt.pure

default: $(TARGETS)

include $(COMMONRULES)

acctcvt: $(OBJFILES)
	$(CCF) $(OBJFILES) $(LDFLAGS) -lgen -o $@

pure: acctcvt
	purify acctcvt

install: default
	${INSTALL} -F /usr/bin $(TARGETS)
