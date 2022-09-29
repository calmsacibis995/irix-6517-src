#!smake
#
# Makefile for the client api from mysql
#	"$Revisions$"

OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs
include ../../../include/ssscommondefs

SRCDIR  =	.

SSDBTOOLSdir = /usr/sbin
SWSUBSYS = $(SSS_EOE)

SWDEST = $(SSDBTOOLSdir)

LCXXINCS =-nostdinc -I$(SRCDIR)/../include -I../include \
	  -I$(SRCDIR)/.. -I.. -I$(ROOT)/usr/include 

LCINCS = $(LCXXINCS)

LLDLIBS	= -nostdlib -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32 \
	  -L$(ROOT)/usr/lib32/internal -lm  -lpthread

TARGETS = libmysqlclient.a espadmin 

OPTIMIZER = -O

OFILES	=	$(CFILES:.c=.o)

LIBRARY =	libmysqlclient.a

LIBMYSQLCLIENT_OBJS = libmysql.o net.o password.o get_password.o errmsg.o

MYSQLADMIN_SRCS	=	mysqladmin.c

MYSQLADMIN_OBJS	=	mysqladmin.o

MYSQL_DEPENDENCY=	-L../mysys -lmysys -L../strings -lmystrings \
			-L../dbug -ldbug -lcurses 

LIBMYSQLCLIENT_LIBADD = $(MYSQL_DEPENDENCY)

LIBDEP = -L. -lmysqlclient

COMMONPREF=SSS

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

.SUFFIXES:
.SUFFIXES: .c .cc .h .o .so .yy

.cc.o:
	 $(CXXF) -c $<
.yy.cc:
	$(YACC) $(YFLAGS) $< && mv y.tab.c $@

.yy.h:
	$(YACC) $(YFLAGS) $< && mv y.tab.c $@


clean clobber install: $(COMMONPREF)$$@


libmysqlclient.a :	$(LIBMYSQLCLIENT_OBJS) 
			ar ccurl libmysqlclient.a $(LIBMYSQLCLIENT_OBJS)

espadmin : $(MYSQLADMIN_OBJS) libmysqlclient.a 
		$(CXXF) -o $@ $(MYSQLADMIN_OBJS) $(LIBDEP) $(MYSQL_DEPENDENCY) $(LLIBS) $(LDLIBS)

install: espadmin
	$(INSTALL) -F $(SWDEST) -idb $(SWSUBSYS) espadmin
#	$(INSTALL) -F $(SWDEST) -idb $(SWSUBSYS) ssdbsql
