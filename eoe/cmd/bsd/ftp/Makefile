#!smake
# Make the ftp command
# $Revision: 2.13 $

include	$(ROOT)/usr/include/make/commondefs

CFILES	=cmds.c cmdtab.c domacro.c ftp.c getpass.c glob.c main.c ruserpass.c \
	uio.c

TARGETS	=ftp

LCOPTS=-fullwarn -signed -common -D_BSD_TIME
LLDOPTS=-Wl,-T,10000000,-D,10040000
LLDLIBS=

default:$(TARGETS)

install:default
	$(INSTALL) -F /usr/bsd $(TARGETS)

include $(COMMONRULES)

ftp:	$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
