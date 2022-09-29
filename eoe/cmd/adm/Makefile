# "$Revision: 1.104 $"
#
# Makefile for /usr/adm (/var/adm) and miscellaneous administration stuff.
#
include	$(ROOT)/usr/include/make/commondefs

# Various crontab files
CRON_TABS= root

# Various cronlib files
CRON_LIBS= .proto at.cfg cron.cfg queuedefs

# Various files from sadmin
TIME_ETC=TIMEZONE
PROF_ETC=stdcshrc stdlogin stdprofile profile cshrc

# Mandatory Access Control (MAC) files
MAC_ETC= irix.mac mac trix.config
MAC_CFG= trix.config

# Capability files
CAP_ETC= irix.cap

MRNO_ETC=motd ttytype ioctl.syscon
SG_ETC	=passwd project projid capability clearance
SHADOW_ETC = shadow
NO_ETC	=sys_id
UP_ETC	=passwd.sgi gettydefs mipsabiversion group
ETC_SCRIPTS= $(MRNO_ETC) $(SG_ETC) $(NO_ETC) $(UP_ETC) \
	$(TIME_ETC) $(PROF_ETC)
#ETC_SCRIPTS= $(MRNO_ETC) $(SG_ETC) $(NO_ETC) $(UP_ETC) \
#	$(TIME_ETC) $(PROF_ETC) $(SHADOW_ETC)

WTMP	=wtmp

ROOT_SCRIPTS= root.profile root.cshrc root.login
MR_SCRIPTS= mr.profile mr.cshrc mr.login
GUEST_SCRIPTS= guest.profile guest.cshrc guest.login
TROOT_SCRIPTS=fixlibdate

FILES	=$(CRON_TABS) $(CRON_LIBS) $(ETC_SCRIPTS) $(WTMP) $(ROOT_SCRIPTS) \
	$(MR_SCRIPTS) $(GUEST_SCRIPTS) $(TROOT_SCRIPTS)

TARGETS=$(TROOT_SCRIPTS)

default:$(FILES)

include ${COMMONRULES}

SHADOWFLAGS = -m 400 -idb "config(suggest)"
SGFLAGS	= -m 644 -idb "config(suggest)"
UPFLAGS	= -m 644 -idb "config(update)"
NOFLAGS	= -m 644 -idb "config(noupdate)"
CMWFLAGS = -m 644 -idb "config(noupdate)"
MRNOFLAGS= -m 644 -idb "config(noupdate)"
MAC_ETC_FLAGS= -m 644 -idb "config(noupdate)"
MAC_CFG_FLAGS= -m 755 -idb "config(noupdate)"
CAP_ETC_FLAGS= -m 644 -idb "config(noupdate)"
G_INST	=$(INSTALL) -m 644 -u guest -g guest \
		-idb "config(noupdate)" -F /usr/people/guest

install:default
	$(INSTALL) -dir \
		/etc /etc/opt /opt/bin opt/include opt/man /lib /usr /var /var/opt /usr/bin /usr/lib /usr/lib/lint /usr/sbin /usr/var
	$(INSTALL) -F /  -idb "nohist" -idb 'postop("$$rbase/.bin.mv.sh")' -src bin.mv.sh .bin.mv.sh
	$(INSTALL) -F / -lns usr/bin bin
	$(INSTALL) -dir -u adm -g adm /var/adm
	$(INSTALL) -dir /usr/CMW

	$(INSTALL) -idb "delhist" -dir /var
	$(INSTALL) -idb "delhist" -dir /opt
	$(INSTALL) -idb "delhist" -F /var  -lns /XYZ tmp
	$(INSTALL) -idb "noshare" -F /etc  -lns ../var/adm/utmp utmp
	$(INSTALL) -idb "noshare" -F /etc  -lns ../var/adm/utmpx utmpx
	$(INSTALL) -idb "noshare" -F /etc  -lns ../var/adm/wtmp wtmp
	$(INSTALL) -idb "noshare" -F /etc  -lns ../var/adm/wtmpx wtmpx
	#
	# Need to attach a postop somewhere for /tmp so that postops/exitops
	# that chroot can have a /tmp
	# For non-clean installs, this is a nop
	#
	$(INSTALL) -F /usr  -idb 'postop("mkdir $$rbase/tmp >/dev/null 2>&1 || exit 0")' \
		-lns ../var/tmp_rex tmp_rex

	$(INSTALL) -dir \
		/var/spool /usr/people \
		/etc/cron.d /var/spool/cron/crontabs \
		/var/spool/cron/atjobs
	$(INSTALL) -idb "delhist" -dir /usr/spool/cron /usr/spool/cron/atjobs /usr/spool/cron/crontabs
	$(INSTALL) -idb "noshare" -F /usr -lns ../var/mail mail
	$(INSTALL) -idb "noshare" -F /usr -lns ../var/spool spool
	$(INSTALL) -idb "noshare" -F /usr -lns ../var/tmp tmp
	$(INSTALL) -idb "noshare" -F /usr -lns ../var/adm adm

	$(INSTALL) -dir -m 700 /var/spool/cron
	$(INSTALL) $(SGFLAGS) -F /etc $(SG_ETC)
#	$(INSTALL) $(SHADOWFLAGS) -F /etc -u bin -g bin $(SHADOW_ETC)
	$(INSTALL) $(MAC_ETC_FLAGS) -F /etc -u root -g sys $(MAC_ETC)
	$(INSTALL) $(MAC_CFG_FLAGS) -F /etc -u root -g sys $(MAC_CFG)
	$(INSTALL) $(CAP_ETC_FLAGS) -F /etc -u root -g sys $(CAP_ETC)
	$(INSTALL) $(SGFLAGS) -F /var/spool/cron/crontabs $(CRON_TABS)
	$(INSTALL) -idb "delhist" -dir /usr/spool/cron/crontabs/root
	$(INSTALL) $(NOFLAGS) -F /etc/cron.d $(CRON_LIBS)
	$(INSTALL) -idb "delhist" -dir /usr/lib/cron/.proto
	$(INSTALL) -idb "delhist" -dir /usr/lib/cron/at.cfg
	$(INSTALL) -idb "delhist" -dir /usr/lib/cron/cron.cfg
	$(INSTALL) -idb "delhist" -dir /usr/lib/cron/queuedefs
	$(INSTALL)  -F /usr/lib -lns ../../etc/cron.d cron

	$(INSTALL) $(NOFLAGS) -F /etc sys_id
	$(INSTALL) $(MRNOFLAGS) -F /etc $(MRNO_ETC)
	$(INSTALL) $(NOFLAGS) -F /etc $(NO_ETC)
	$(INSTALL) $(UPFLAGS) -F /etc $(UP_ETC)
	$(INSTALL) $(NOFLAGS) -F /var/adm -u adm -g adm $(WTMP)
	$(INSTALL) $(NOFLAGS) -F /var/adm -u adm -g adm -src $(WTMP) wtmpx
	$(INSTALL) $(NOFLAGS) -F /var/adm -src $(WTMP) utmpx
	$(INSTALL) -idb "delhist" -dir /usr/adm/wtmp
	$(INSTALL) -idb "delhist" -dir /usr/adm/wtmpx
	$(INSTALL) -idb "delhist" -dir /usr/adm/utmpx
	$(INSTALL) $(NOFLAGS) -F / -u adm -g adm -src varupdate .varupdate
# Sadmin files
	$(INSTALL) $(NOFLAGS) -F /etc $(TIME_ETC)
	$(INSTALL) $(SGFLAGS) -F /etc $(PROF_ETC)

	for nm in $(ROOT_SCRIPTS); do \
	  $(INSTALL) $(SGFLAGS) -F / -src $$nm `expr $$nm : 'root\\(.*\\)'`; \
	done
	for nm in $(MR_SCRIPTS); do \
	  $(INSTALL) -F / -src $$nm `expr $$nm : 'mr\\(.*\\)'`; \
	done
	# this exitop makes sure that any left over vadmin stuff from 40x, which
	# was full of security holes, is made somewhat safe.
	# a lot of folks apparently didnt install the 5.2 or 5.3 replacements...
	$(INSTALL) -u guest -g guest -dir -idb 'exitop("2>/dev/null cd $$rbase/usr/lib/vadmin&&(for f in disks networking printers serial_ports users;do if [ -x $$f ];then chmod 700 $$f;chown 0.0 $$f;fi;done);2>/dev/null cd $$rbase/usr/sbin&&(for f in systemdown vadmin;do if [ -x $$f ];then chmod 700 $$f;chown 0.0 $$f;fi;done);exit 0")' /usr/people/guest 
	for nm in $(GUEST_SCRIPTS); do \
	  $(G_INST) -src $$nm `expr $$nm : 'guest\\(.*\\)'`; \
	done
	$(INSTALL) -F /usr/sbin $(TROOT_SCRIPTS)
	$(INSTALL) $(CMWFLAGS) -F /etc CMWdevice.conf
