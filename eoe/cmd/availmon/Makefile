#ident "$Revision: 1.22 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS =	amconfig amconvert amfilter amnotify amparse amreport amreceive \
		amsend amsysinfo amsyslog amtickerd amtime1970 amuudecode
AMFLAGS =	autoemail autoemail.list statusinterval 

LLDLIBS =	-lgen

INITDPATH =	/etc/init.d
RC2PATH =	/etc/rc2.d
LINKPATH =	../init.d
RC2SCRIPT =	S95availmon
RC0PATH =	/etc/rc0.d
RC0SCRIPT =	K05availmon
AVAILPATH =	/var/adm/avail
CONFIGPATH =	$(AVAILPATH)/config
ETCPATH =	/usr/etc

SGFLAGS =	-m 644 -idb "config(suggest)"
NOFLAGS =	-m 644 -idb "config(noupdate)"

LOWFLAGS =	-idb "mach(CPUBOARD=IP20 CPUBOARD=IP22 CPUBOARD=IP26 CPUBOARD=IP28 CPUBOARD=IP30 CPUBOARD=IP32)"
HIGHFLAGS =	-idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25 CPUBOARD=IP27)"

default:	$(TARGETS) amregister amdiag availmon amstart

include $(COMMONRULES)

install:	default
		${INSTALL} -F $(INITDPATH) availmon
		${INSTALL} -F $(RC0PATH) -lns $(LINKPATH)/availmon $(RC0SCRIPT)
		${INSTALL} -F $(RC2PATH) -lns $(LINKPATH)/availmon $(RC2SCRIPT)
		${INSTALL} -F $(ETCPATH) $(TARGETS) amregister amdiag amstart
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amconfig amconfig
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amconvert amconvert
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amdiag amdiag
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amfilter amfilter
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amnotify amnotify
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amparse amparse
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amreport amreport
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amreceive amreceive
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amregister amregister
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amsend amsend
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amsysinfo amsysinfo
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amsyslog amsyslog
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amtickerd amtickerd
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amtime1970 amtime1970
		${INSTALL} -F $(AVAILPATH) -lns $(ETCPATH)/amuudecode amuudecode
		${INSTALL} -F $(CONFIGPATH) $(NOFLAGS) $(AMFLAGS)
		${INSTALL} -F $(CONFIGPATH) $(NOFLAGS) $(LOWFLAGS) shutdownreason
		${INSTALL} -F $(CONFIGPATH) $(NOFLAGS) $(HIGHFLAGS) -src shutdownreason.highend shutdownreason
		${INSTALL} -F $(CONFIGPATH) $(NOFLAGS) $(LOWFLAGS) livenotification
		${INSTALL} -F $(CONFIGPATH) $(NOFLAGS) $(HIGHFLAGS) -src livenotification.highend livenotification
		${INSTALL} -F $(CONFIGPATH) $(NOFLAGS) $(LOWFLAGS) tickerd
		${INSTALL} -F $(CONFIGPATH) $(NOFLAGS) $(HIGHFLAGS) -src tickerd.highend tickerd
		${INSTALL} -F $(CONFIGPATH) $(NOFLAGS) $(LOWFLAGS) hinvupdate
		${INSTALL} -F $(CONFIGPATH) $(NOFLAGS) $(HIGHFLAGS) -src hinvupdate.highend hinvupdate
		${INSTALL} -F $(AVAILPATH)  install.sh

amreport:	amreport.c
		$(CCF) amreport.c $(LDFLAGS) -o amreport -lcurses
