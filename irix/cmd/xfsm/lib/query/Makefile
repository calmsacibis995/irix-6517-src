#!smake
#ident "$Revision: 1.4 $"

DEPTH=../..
include $(DEPTH)/Makedefs

#LLDLIBS=-g -ldisk libxfsutils.a
LLDLIBS=-ldisk 
KERN=../../../../kern
LCINCS=-I$(KERN) -I../../include -I../../../xlv/include

CFILES = xfs_xlv_query.c xfs_disk_query.c xfs_fs_query.c \
	 xfs_get_obj.c xfs_query.c 

TARGETS=$(OBJECTS)

OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -D_SGI_SOURCE
LCOPTS = $(REGOPTS)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	${INSTALL} -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
