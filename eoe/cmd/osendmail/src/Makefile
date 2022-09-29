#!smake
# stripped down sendmail make file
#
# "/plroot/eoe/irix6.5m/.RCS/PL/cmd/sendmail.OLD/src/RCS/Makefile,v 1.1 1999/11/05 00:42:15 mende Exp"

include	$(ROOT)/usr/include/make/commondefs

# OPTIMIZER=-g

SRCS=   alias.c \
	arpadate.c \
	clock.c \
	collect.c \
	conf.c \
	convtime.c \
	daemon.c \
	deliver.c \
	domain.c \
	envelope.c \
	err.c \
	headers.c \
	macro.c \
	main.c \
	map.c \
	mci.c \
	mime.o \
	parseaddr.c \
	queue.c \
	readcf.c \
	recipient.c \
	safefile.c \
	savemail.c \
	srvrsmtp.c \
	stab.c \
	stats.c \
	sysexits.c \
	trace.c \
	udb.c \
	usersmtp.c \
	util.c \
	version.c \
	sgi_compat.c

OBJS= $(SRCS:.c=.o)

#if exists ($(ROOT)/usr/include/sys/sysctl.h)
SGI_OPT_IP_ALIAS= -DSGI_IP_ALIAS
#endif

#if exists ($(ROOT)/usr/include/t6net.h)
SGI_OPT_SECURITY= -DSGI_SECURITY
#endif

#if exists ($(ROOT)/usr/include/ns_api.h)
SGI_FEATURES_HASSNPRINTF= -DHASSNPRINTF
SGI_OPT_NSD= -DSGI_MAP_NSD
#endif

#if exists ($(ROOT)/usr/include/db.h)
SGI_OPT_UDB= -DNEWDB -DUSERDB
SGI_LIBS_UDB= -ldb
#endif

SGI_FEATURES=$(SGI_FEATURES_HASSNPRINTF)

SGI_OPT_EXTENSIONS= $(SGI_OPT_IP_ALIAS) $(SGI_OPT_SECURITY) \
	$(SGI_OPT_NSD) $(SGI_OPT_UDB)
SGI_OPT_LIBS= $(SGI_LIBS_UDB)

LCOPTS	=-DVMUNIX -DUSG -DSCANF -DNDBM -DNIS -DIRIX5 -DBUGFIX $(SGI_FEATURES) \
	-DVENDOR_DEFAULT=VENDOR_SGI -DSGI_EXTENSIONS $(SGI_OPT_EXTENSIONS) \
	-signed -G0 -use_readonly_const -OPT:Olimit=3000
LLDLIBS += $(SGI_OPT_LIBS)
LLDOPTS	=-Wl,-T,10000000,-D,10107000,-rdata_shared
#LLDOPTS=-Wl,-T,10000000,-D,10102000,-rdata_shared

TARGETS=sendmail

default:$(TARGETS)

include $(COMMONRULES)

sendmail:$(OBJS)
	$(CCF) $(OBJS) $(LDFLAGS) -o $@ $(LLDLIBS)

# install rule only for clover
install:default
	$(INSTALL) -idb "std.sw.unix" -F /usr/lib -u root -m 4755 $(TARGETS)
	$(INSTALL) -idb "std.sw.unix" -u root -g mail -m 0775 -dir /var/spool/mqueue
	$(INSTALL) -idb "delhist" -idb "std.sw.unix" -dir /usr/spool/mqueue
	$(INSTALL) -idb "std.sw.unix" -F /usr/bsd -lns /usr/lib/sendmail mailq
	$(INSTALL) -idb "std.sw.unix" -F /usr/bsd -lns /usr/lib/sendmail newaliases

$(OBJS):cdefs.h mailstats.h sendmail.h conf.h pathnames.h useful.h

