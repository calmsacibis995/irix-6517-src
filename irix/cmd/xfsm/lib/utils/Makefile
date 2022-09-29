#!smake
#ident "$Revision: 1.3 $"

DEPTH=../..
include $(DEPTH)/Makedefs

KERN=../../../../kern
LCINCS=-I$(KERN) -I../../include -I../../../xlv/include

CFILES=	xfs_addbuf.c \
	xfs_debug.c \
	xfs_gen.c \
	xfs_get_hosts.c \
	xfs_get_info.c \
	xfs_group.c \
	xfs_popen.c \
	gethosts.c \
	group.c

TARGETS=$(OBJECTS)

OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -D_SGI_SOURCE
LCOPTS = $(REGOPTS)

default:$(TARGETS)

include $(COMMONRULES)

install: default
#	${INSTALL} -F /sbin $(TARGETS)
#	${INSTALL} -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
