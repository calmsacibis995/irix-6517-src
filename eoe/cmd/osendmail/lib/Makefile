#!smake
# make sendmail configuration file
# "1.1"

include	$(ROOT)/usr/include/make/commondefs

LCFLAGS=-I../src -DHASSNPRINTF -DNOT_SENDMAIL
#LLDLIBS= -lbsd
CTAGS=	ctags -w

CFILES=	canonhost.c mailstats.c makemap.c safefile.c

ALIAS=	aliases
CANON=	canonhost
CF=	sendmail.cf
FC=	sendmail.fc
CONFIG=	configmail
HELP=	sendmail.hf

INSTETC=$(INSTALL) -m 644 -F /etc
INSTSBIN=$(INSTALL) -m 755 -F /usr/sbin
INSTBSD=$(INSTALL) -m 755 -F /usr/bsd
INSTUETC=$(INSTALL) -m 755 -F /usr/etc

BSDTARGETS= makemap mailstats
TARGETS=$(CANON) $(BSDTARGETS) $(CF) $(FC)

# This needs to be kept the same as in ../src/Makefile
DBMDEF=-DNDBM
DBMLIB=

#if exists ($(ROOT)/usr/include/db.h)
DBMDEF += -DNEWDB
DBMLIB += -ldb
#endif

DIRT= *.o $(TARGETS) safefile.c
#include $(COMMONRULES)

default: $(TARGETS) $(ALIAS) $(CF) $(CONFIG) $(HELP) $(FC) 

headers exports:

makemap: safefile.o
	$(CC) -o $@ $(CFLAGS) $(SRCINC) $(DBMDEF)  $(LDFLAGS) $@.c safefile.o $(DBMLIB) 

safefile.c: ../src/safefile.c
	ln -s ../src/safefile.c .

clean clobber:
	rm -f $(DIRT)

# safefile.o: safefile.c
# 	$(CC) -c $(CFLAGS) $(DBMDEF) safefile.c

mailstats: 
	$(CC) -o $@ $(CFLAGS) $(SRCINC) $(LDFLAGS) $@.c $(DBMLIB) 

$(FC): $(CF)
	cp -f $(CF) $(FC)

#	irixUNAME=`uname -r | sed -e 's/[-\ ].*//'` ;
$(CF): $(CF).m4
	versionName=`fgrep 'Version[]' ../src/version.c | \
	  sed -e 's/^[^\"]*\"//' -e 's/\"[^\"]*$$//' -e 's/^[0-9]*\.SGI/SGI/'` ; \
	versionID=`echo $$versionName  | sed -e 's/^.*SGI\.//' | \
	  awk -F \\. '{printf "%d%02d%02d\n",$$1,$$2,$$3}'` ; \
	irixUNAME=$(RELEASE) ; \
	irixID=`echo $$irixUNAME | \
	  awk -F \\. '{printf "%d%02d%02d\n",$$1,$$2,$$3}'` ; \
	m4 -DIRIX_VERSION=$$irixID -DSENDMAIL_VERSION_NAME=$$versionName \
	  -DSENDMAIL_VERSION=$$versionID < $(CF).m4 > $@

install:default install_default install_cf_m4

install_default:
	$(INSTETC) -idb "std.sw.unix config(suggest)" $(CF) $(ALIAS) $(HELP)
	$(INSTETC) -idb "std.sw.unix config(noupdate)" $(FC) 
	$(INSTALL) -idb "std.sw.unix" -F /usr/lib -lns ../../etc/aliases aliases
	$(INSTALL) -idb "std.sw.unix" -F /usr/lib -lns ../../etc/sendmail.cf sendmail.cf
	$(INSTALL) -idb "std.sw.unix" -F /usr/lib -lns ../../etc/sendmail.hf sendmail.hf
	$(INSTSBIN) -idb "std.sw.unix" $(CANON)
	$(INSTUETC) -idb 'std.sw.unix exitop("chroot $$rbase /bin/sh /usr/etc/configmail updateCFsyntax")' $(CONFIG)
	$(INSTBSD) -idb "std.sw.unix" $(BSDTARGETS)

install_cf_m4:
	find cf -type d -print |  sed 's/^cf/\/usr\/lib\/sendmail\.cf\_m4/' > cf_m4.dir_list
	find cf -type f -print | sed 's/^cf\///' > cf_m4.file_list
	$(INSTALL) -dir `cat cf_m4.dir_list`
	(cd cf ; $(INSTALL) -m 644 -F /usr/lib/sendmail.cf_m4 `cat ../cf_m4.file_list`)
	rm -f cf_m4.file_list cf_m4.dir_list
