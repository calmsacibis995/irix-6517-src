#ident "$Revision: 1.8 $"

AM_SRC_DEPTH = ..

include $(ROOT)/usr/include/make/commondefs
include $(AM_SRC_DEPTH)/include/amcommondefs

TARGETS = amregister amsend amsysinfo amreport \
	  amnotify amtickerd

MISSING_LOWENDTARGETS = hinvupdate autoemail autoemail.list \
		  livenotification shutdownreason statusinterval \
		  tickerd

MISSING_HIGHENDTARGETS = hinvupdate.highend livenotification.highend \
			 shutdownreason.highend tickerd.highend

LOWFLAGS = -idb "mach(CPUBOARD=IP20 CPUBOARD=IP22 CPUBOARD=IP26 CPUBOARD=IP28 CPUBOARD=IP30 CPUBOARD=IP32)"
HIGHFLAGS = -idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25 CPUBOARD=IP27)"
NOFLAGS =  -idb "config(noupdate)"

MISSING_PATH    = /var/adm/avail/config

IDB_TAG = -idb EOE

default: $(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} $(IDB_TAG) -F $(ETCPATH) $(TARGETS) 
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH) -lns $(ETCPATH)/amregister amregister
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH) -lns $(ETCPATH)/amreport amreport
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH) -lns $(ETCPATH)/amsend amsend
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH) -lns $(ETCPATH)/amsysinfo amsysinfo
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH) -lns $(ETCPATH)/amnotify amnotify
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH) -lns $(ETCPATH)/amtickerd amtickerd
	${INSTALL} $(NOFLAGS) $(LOWFLAGS) -F $(MISSING_PATH) -m 644 $(MISSING_LOWENDTARGETS) 
	${INSTALL} -F $(MISSING_PATH) $(NOFLAGS) $(HIGHFLAGS) -src shutdownreason.highend shutdownreason
	${INSTALL} -F $(MISSING_PATH) $(NOFLAGS) $(HIGHFLAGS) -src livenotification.highend livenotification
	${INSTALL} -F $(MISSING_PATH) $(NOFLAGS) $(HIGHFLAGS) -src tickerd.highend tickerd
	${INSTALL} -F $(MISSING_PATH) $(NOFLAGS) $(HIGHFLAGS) -src hinvupdate.highend hinvupdate



