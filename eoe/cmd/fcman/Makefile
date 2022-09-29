#!smake
#
# Makefile for fcman/src
#
# $Revision: 1.15 $

include $(ROOT)/usr/include/make/commondefs

#
# WAR and options
# 
# DG_WAR_1  - Hard Address is zero for all DISK element statuses
# DG_WAR_2  - Generation Code in Enclosure Status Page (0x2) never
#             leaves zero. 
# DG_WAR_3  - WWN is not valid in Enclosure Services Page (0x1).
# DG_WAR_4  - UPS count in config page is zero 
# DG_WAR_13 - Enclosure ID is always zero 
# DG_WAR_14 - status bits must be zero in control page (SEND DIAGS 2)
# DG_WAR_15 - on write control, drive element statuses are rotated
# SG_WAR_1  - ES bit not set in inquiry data 
# SG_WAR_2  - port and dualp are reversed in inquire data 
#

FCAGENT_REV=	1.1

LCOPTS = -g \
	 -DDG_WAR_1=0 \
	 -DDG_WAR_2=0 \
	 -DDG_WAR_3=1 \
	 -DDG_WAR_4=0 \
	 -DDG_WAR_13=0 \
	 -DDG_WAR_14=1 \
	 -DDG_WAR_15=0 \
	 -DSG_WAR_1=0 \
	 -DSG_WAR_2=0 \
	 -DPCV_BIT_REQUIRED=1 \
	 -DSCSIHA_AVAIL=1 \
	 -DHWG_AVAIL=1

CFILES=		fcagent.c fcagent_th.c \
		config.c cfg_scanner.c debug.c scsi.c dslib.c dstab.c \
		hash.c usrsignal.c event.c
HFILES=		fcagent.h config.h cfg_scanner.h debug.c fcagent_structs.h \
		fcagent_services.h esi.h fcal.h scsi.h dslib.h \
		hash.h usrsignal.h event.h
RPCFILES=	fcagent_rpc.h \
		fcagent_rpc_clnt_stubs.c fcagent_rpc_svc_stubs.c fcagent_rpc_xdr.c

OBJECTS= 	$(CFILES:.c=.o)

AGENT_OBJS=	fcagent.o fcagent_th.o config.o cfg_scanner.o \
		debug.o scsi.o dslib.o dstab.o hash.o usrsignal.o event.o \
		fcagent_rpc_svc_stubs.o fcagent_rpc_xdr.o

AGENT_TARGETS=	fcagent
AGENT_CO=	postinsert.CO preremove.CO postremove.CO statchanged.CO 
UI_TARGETS=	fccli
TARGETS=	$(AGENT_TARGETS) $(UI_TARGETS)
IDBTAGS=	"EOE mach(CPUBOARD=IP27 CPUBOARD=IP30 CPUBOARD=IP32 CPUBOARD=IP33 CPUBOARD=IP35)"
DIRT=		*.[ou] a.out core $(_FORCE) ar.tmp.* $(RPCFILES) Makedepend fcagent_vers.h

default: $(TARGETS) $(AGENT_CO) $(AGENT_CONFIG) 

include $(COMMONRULES)

install: default 
	$(INSTALL) -idb $(IDBTAGS) -F /usr/sbin $(TARGETS)
	$(INSTALL) -idb $(IDBTAGS) -F /usr/lib/fcagent $(AGENT_CO)
	$(INSTALL) -idb $(IDBTAGS) -F /etc/config fcagent.options
	$(INSTALL) -idb $(IDBTAGS) -F /etc/init.d -src fcagent.init.d fcagent
	$(INSTALL) -idb $(IDBTAGS) -F /etc/config -src fcagent.config fcagent
	$(INSTALL) -idb $(IDBTAGS) -F /etc/rc2.d -lns ../init.d/fcagent S92fcagent
	$(INSTALL) -idb $(IDBTAGS) -F /etc/rc0.d -lns ../init.d/fcagent K32fcagent

fcagent:	$(AGENT_OBJS) fcagent_rpc.h fcagent_vers.h
	$(CCF) $(AGENT_OBJS) $(LDFLAGS) -o $@
fccli.o fcagent_rpc_clnt_stubs.o fcagent_rpc_xdr.o debug.o: fcagent_rpc.h
fccli:	fccli.o fcagent_rpc_clnt_stubs.o fcagent_rpc_xdr.o debug.o
	$(CCF) fccli.o fcagent_rpc_clnt_stubs.o fcagent_rpc_xdr.o debug.o $(LDFLAGS) -o $@
fcagent_rpc_svc_stubs.c: fcagent_rpc.x fcagent_rpc_svc_stubs.awk
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -m -o fcagent_rpc_svc_stubs.c fcagent_rpc.x 
	mv $@ $@.pre
	awk -f fcagent_rpc_svc_stubs.awk <$@.pre >$@
	rm $@.pre
fcagent_rpc_clnt_stubs.c: fcagent_rpc.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -l -o fcagent_rpc_clnt_stubs.c fcagent_rpc.x 
fcagent_rpc_xdr.c: fcagent_rpc.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -c -o fcagent_rpc_xdr.c fcagent_rpc.x 
fcagent_rpc.h: fcagent_rpc.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -h -o fcagent_rpc.h fcagent_rpc.x 
fcagent_vers.h:
	@echo "#define VERS \"FC JBOD Agent version $(FCAGENT_REV), `date`\"" > $@

$(OBJECTS):	$(HFILES) fcagent_rpc.h Makefile

clean:

clobber:
