#!smake
#
# Makefile for SSDB Source Code
#
OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs
include ../../../include/ssscommondefs

SRCDIR = .

MYSQLBASEdir = /usr/etc
MYSQLDATAdir = /var/esp/ssdb/data
MYSQLSHAREdir = /usr/share/mysql

SWSUBSYS = $(SSS_EOE)
SWDEST   = $(MYSQLBASEdir)

LLDLIBS   = -nostdlib -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32 \
	    -L$(ROOT)/usr/lib32/internal \
	    -L../isam -L../merge -L../heap -L../mysys \
	    -L../regex -L../strings \
	    -lnisam -lmerge -lheap -lmysys -lregex -lmystrings \
	    -lm -lcrypt -lpthread

LCXXINCS = -nostdinc -I$(SRCDIR)/../include -I$(SRCDIR)/../regex \
	-I$(SRCDIR)/.. -I../include -I.. -I. -I$(ROOT)/usr/include

LCINCS = $(LCXXINCS)

LCXXDEFS  = -DMYSQL_SERVER -DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
	-DDATADIR="\"$(MYSQLDATAdir)\"" \
	 -DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
	-DHAVE_CONFIG_H -DDBUG_OFF

LCDEFS = $(LCXXDEFS)

OPTIMIZER = -O 


YACC = bison -y

YFLAGS = -d

TARGETS = espdbd

CFILES = item.cc item_sum.cc item_buff.cc item_func.cc \
	item_cmpfunc.cc item_strfunc.cc item_timefunc.cc \
	thr_malloc.cc \
	field.cc key.cc sql_class.cc sql_list.cc \
	net_serv.c net_pkg.cc lock.cc my_lock.c \
	sql_string.cc sql_map.cc \
	mysqld.cc password.c hash_filo.cc hostname.cc \
	sql_lex.cc convert.cc sql_parse.cc sql_yacc.yy \
	sql_base.cc dfunc.cc sql_select.cc sql_insert.cc \
	sql_update.cc sql_delete.cc \
	procedure.cc item_uniq.cc sql_test.cc \
	log.cc init.cc derror.cc sql_acl.cc unireg.cc \
	time.cc opt_range.cc opt_sum.cc \
	records.cc filesort.cc handler.cc \
	sql_db.cc crebas.cc sql_table.cc sql_crypt.cc \
	sql_load.cc field_conv.cc sql_show.cc sql_udf.cc

OFILES  = $(CFILES:.c=.o)

MYSQL_OBJS = item.o item_sum.o item_buff.o item_func.o \
	item_cmpfunc.o item_strfunc.o item_timefunc.o thr_malloc.o field.o \
	key.o sql_class.o sql_list.o net_serv.o net_pkg.o lock.o my_lock.o \
	sql_string.o sql_map.o mysqld.o password.o hash_filo.o hostname.o \
	sql_lex.o convert.o sql_parse.o sql_yacc.o sql_base.o dfunc.o \
	sql_select.o sql_insert.o sql_update.o sql_delete.o procedure.o \
	item_uniq.o sql_test.o log.o init.o derror.o sql_acl.o unireg.o time.o \
	opt_range.o opt_sum.o records.o filesort.o handler.o sql_db.o crebas.o \
	sql_table.o sql_crypt.o sql_load.o field_conv.o sql_show.o sql_udf.o

COMMONPREF =    SSS

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

clean clobber install : $(COMMONPREF)$$@


espdbd : $(MYSQL_OBJS) 
	$(CXXF) -o $@ $(MYSQL_OBJS) $(MYLIBS) $(LDLIBS)

install: default
	$(INSTALL) -F $(SWDEST) -idb $(SWSUBSYS) $(TARGETS) 
