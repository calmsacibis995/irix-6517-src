#ident	"$Revision: 1.51 $"

# make script for rc0.d stuff

#
# Sequence number utilization
#
# These are all the known users of sequence numbers in the /etc/rc0.d
# directory.  Please keep this list up to date...
#
#	Name		Description
#
#	K02CMW		- shut down Compartmented Mode Window management
#	K02xdm		- shut down xdm (unused in the CMW case)
#	K03announce	- display shut down message
#	K04cachefs	- unmount cachefs file systems
#  *	K10cadmin	- shut down Cadmin daemons
#	K15cron		- shut down cron
#	K18uucp		- shut down uucp
#	K20mail		- shut down mail
#	K22acct		- shut down process accounting
#	K25lp		- shut down SysV printer support (in print ISM >= 5.3)
#	K26bsdlpr	- shut down BSD printer support  (in print ISM >= 5.3)
#  *	K27audit	- shut down C2 auditing
#  *	K28nsr		- shut down IRIS NetWorker
#	K30cdromd	- stop CD rom access daemons
#	K30msdosd	- stop MSDOS filesystem access daemons
#	K30netls	-
#	K32rfindd	-
#  *	K35atria	-
#	K35lt		-
#	K35nck		-
#  *	K37failsafe	- shut down IRIS FailSafe and clean up
#  *	K38aliases-ip	- 
#  *	K38BDSpro	- shut down Bulk Data Service server
#	K40network	- shut down network (NFS, automount, etc)
#  *	K45sysctlr	- shut down Everest system controller daemon
#  *	K66nqs		- Network Queueing System
#	K72callsvr	- ???
#       K75array        - shut down array services
#	K78winattr	-
#	K80postinst	- clean up any failed installation operations
#	K82sysetup	- stop syslogd
#	K83swap		- remove all swap files except /dev/swap
#	K84filesystems	- unmount all filesystems other than / and /usr
#  *	K88epoch	- stop EPOCH daemons
#  *	K92volmgr	- stop IRIS Volume Manager volumes
#	K92lv		- stop all LV volumes
#	K98usr		- unmount /usr
#
# *   - unbundled products that must fit into this sequence
# ??? - these may no longer be active
#

include	$(ROOT)/usr/include/make/commondefs

# XXX you must uncomment for loop in install rule if you uncomment this
#STARTLST=
STOPLST1=K40network K82sysetup K84filesystems K38aliases-ip
STOPLST2=K02CMW K02xdm K03announce K15cron K20mail K80postinst K98usr K83swap \
	K99disk_patch
STOPLST	=$(STOPLST1) $(STOPLST2)

INSDIR	=/etc/rc0.d
FRMDIR	=../init.d


default:$(FRC)

clean:

clobber:clean

FRC:

install:default
	$(INSTALL) -idb "std.sw.unix mr" -dir $(INSDIR)
	for f in $(STOPLST1); do \
		fn=$(FRMDIR)/`expr $$f : '...\(.*\)'` ; \
		$(INSTALL) -idb "std.sw.unix mr" -F $(INSDIR) -lns $$fn $$f; \
	done
	for f in $(STOPLST2); do \
		fn=$(FRMDIR)/`expr $$f : '...\(.*\)'` ; \
		$(INSTALL) -idb "std.sw.unix" -F $(INSDIR) -lns $$fn $$f; \
	done
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" -F $(INSDIR) \
		-lns $(FRMDIR)/sysctlr K45sysctlr
	$(INSTALL) -idb "eoe2.sw.uucp" -F $(INSDIR) -lns $(FRMDIR)/uucp K18uucp

#	for f in $(STARTLST); do \
#		fn=$(FRMDIR)/`expr $$f : '...\(.*\)'` ; \
#		$(INSTALL) -idb "std.sw.unix" -F $(INSDIR) -lns $$fn $$f; \
#	done
