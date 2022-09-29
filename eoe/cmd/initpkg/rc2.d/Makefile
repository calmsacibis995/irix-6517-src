#ident	"$Revision: 1.85 $"

# make script for rc2.d stuff

#
# Sequence number utilization
#
# These are all the known users of sequence numbers in the /etc/rc2.d
# directory.  Please keep this list up to date...
#
#	Name		Description
#
#	S00announce	- display startup message
#	S00disk_patch	- warn about downrev stiction-prone ibm drives
#	S04usr		- mount /usr
#  *	S06volmgr	- start up IRIS Volume Manager volumes
#  *	S08CMWchkconfig	- note and repair CMW configuration errors
#  *	S10epoch	- start up EPOCH daemons
#	S12filesystems	- mount the rest of the filesystems in /etc/fstab
#	S14quotas	- initialize file space quotas
#	S16postinst	- clean up any failed installation operations
#	S20sysetup	- set hostname, start syslogd, etc
#	S21perf		-
#	S22acct		-
#	S23autoconfig	- check if we need a new kernel built
#	S30network	- start up network (NFS, automount, etc)
#	S32aliases-ip	-
#	S33sysctlr	- start up Everest system controller daemon
#	S33sn0start	- Log messages on the SN0 system, start up the
#			  system controller, etc.
#	S33partcfg    	- start up the Partition Config Daemon
#	S34snmp		- start up SNMP agents
#  *	S35audit	- start up C2 auditing
#  *	S37failsafe	- start up IRIS FailSafe
#  *	S38BDSpro	- start up Bulk Data Service server
#	S40nck		-
#	S45netls	-
#	S48savecore	- check for coredumps
#	S49swap		- add swap files
#	S50mail		- start up mail processing
#	S58rmtmpfiles	- clean up /tmp and leftover editor files
#	S60lp		- start SysV printer support (in print ISM >= IRIX 5.3)
#	S61bsdlpr	- start BSD printer support  (in print ISM >= IRIX 5.3)
#       S62array        - start array services
#  *	S65nsr		- start IRIS NetWorker daemons
#  *	S66nqs		- Network Queueing System
#	S70uucp		- start UUCP support
#	S72callsvr	- ???
#	S75cron		- start cron support
#  *	S77atria	-
#  * S80lt		-  ???
#	S83audio	- installed in dmedia ism
#	S85svideo	- ???
#	S88configmsg	- print "files have changed" message for inst
#	S90chkdev	- check that tape and floppy devices exist; must follow
#			- autoconfig, and should precede mediad
#	S94grio		- start guarantee rate i/o daemon
#  *	S95avail	- start availability monitor
#	S96jserver	- ???
#	S96fontserver	- start the X11 font server daemon
#  *	S97cadmin	- start Cadmin daemons
#	S97mediad	-  removable media; installed from cmd/mediad
#  *	S98CMW		- Compartmented Mode Window Management
#	S98rfindd	-
#	S98xdm		- start the screen manager (unused in the CMW case)
#	S99cachefs	- mount cachefs file systems
#				- may be depndent upon removable media
#
# *   - unbundled products that must fit into this sequence
# ??? - these may no longer be active
#

include	$(ROOT)/usr/include/make/commondefs

STARTMR=S12filesystems S20sysetup S58rmtmpfiles S90chkdev
STARTSTD=S04usr S16postinst S23autoconfig S30network S32aliases-ip \
	S34snmp S48savecore S49swap S50mail S75cron \
	S88configmsg S94grio S96fontserver S98CMW S98xdm S00announce \
	S00disk_patch S08CMWchkconfig

# XXX if you uncomment this you must uncomment the for loop under install:
#STOPLST =

INSDIR	=/etc/rc2.d
FRMDIR	=../init.d


default:$(FRC)

clean:

clobber:clean

FRC:

install:default
	$(INSTALL) -idb "std.sw.unix mr" -dir $(INSDIR)
	for f in $(STARTMR); do \
		fn=$(FRMDIR)/`expr $$f : '...\(.*\)'` ; \
		$(INSTALL) -idb "std.sw.unix mr" -F $(INSDIR) -lns $$fn $$f; \
	done
	for f in $(STARTSTD); do \
		fn=$(FRMDIR)/`expr $$f : '...\(.*\)'` ; \
		$(INSTALL) -idb "std.sw.unix" -F $(INSDIR) -lns $$fn $$f; \
	done
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" -F $(INSDIR) \
		-lns $(FRMDIR)/sysctlr S33sysctlr
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP27)" -F $(INSDIR) \
		-lns $(FRMDIR)/sn0start S33sn0start
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP27)" -F $(INSDIR) \
		-lns $(FRMDIR)/partcfg S33partcfg
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP30)" -F $(INSDIR) \
		-lns $(FRMDIR)/machdepstart.ip30 S48machdepstart
	$(INSTALL) -idb "eoe2.sw.uucp" -F $(INSDIR) -lns $(FRMDIR)/uucp S70uucp
	$(INSTALL) -idb "std.sw.quotas" -F $(INSDIR) -lns $(FRMDIR)/quotas S14quotas
#	for f in $(STOPLST); do \
#		fn=$(FRMDIR)/`expr $$f : '...\(.*\)'` ; \
#		$(INSTALL) -idb "std.sw.unix" -F $(INSDIR) -lns $$fn $$f; \
#	done
