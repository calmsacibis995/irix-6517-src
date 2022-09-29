#ident	"$Revision: 1.119 $"

include	$(ROOT)/usr/include/make/commondefs


# simple files not changed by the user
MRSIMP	=usr filesystems postinst
STDSIMP	=configmsg snmp swap uucp xlv
SPCL	=quotas 
SIMP	=$(STDSIMP) $(SPCL) $(MRSIMP)

# configuration files that SGI knows best
MRUPDATE=rmtmpfiles network 
STDUPDATE=autoconfig chkdev cron grio mail savecore sysetup xdm fontserver \
	announce disk_patch aliases-ip CMW CMWchkconfig failover
UPDATE	=$(STDUPDATE) $(MRUPDATE) 

#
# Turn on savecore by default ... not all systems will want this
# feature.
SAVECORE_DEFAULT ="mach(CPUBOARD=IP19 CPUBOARD=IP20 CPUBOARD=IP21 CPUBOARD=IP22 CPUBOARD=IP25 CPUBOARD=IP26 CPUBOARD=IP27 CPUBOARD=IP28 CPUBOARD=IP30 CPUBOARD=IP32) config(noupdate)"

# config files for miniroot only
MRONLYFILES=sysetup.mr filesystems.mr

FILES	=$(SIMP) $(UPDATE) $(MRONLYFILES) nostickytmp

INSDIR	=/etc/init.d
CFGDIR	=/etc/config
ETCDIR	=/etc
INSCMD	=$(INSTALL) -m 755 -F $(INSDIR)

default:$(FILES)

clean clobber:

FRC:

install: default
	$(INSTALL) -dir $(INSDIR) $(CFGDIR)
	$(INSTALL) -idb "noshare" -F /var -lns /etc/config config
	$(INSTALL) -m 644 -F $(INSDIR) README
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		-src verbose.config verbose
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		-src vswap.config vswap 
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		-src network.config network
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		-src sendmail.config sendmail
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		-src xlv.config xlv
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		netif.options
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		static-route.options
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		vswap.options
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		autoconfig.options
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		-src ipaliases.config ipaliases
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		ipaliases.options
	$(INSTALL) -idb $(SAVECORE_DEFAULT) -m 644 -F $(CFGDIR) \
		 -src savecore.config savecore
	$(INSTALL) -idb "config(noupdate)" -m 644 -F $(ETCDIR) \
		-src failover.config failover.conf
	$(INSCMD) $(MRSIMP)
	$(INSCMD) $(STDSIMP)
	$(INSCMD) -idb "config(update)" $(STDUPDATE)
	$(INSCMD) quotas
	$(INSCMD) -idb "config(update) mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" sysctlr
	$(INSCMD) -idb "config(update) mach(CPUBOARD=IP27)" sn0start
	$(INSCMD) -idb "config(update) mach(CPUBOARD=IP27)" partcfg
	$(INSCMD) -idb "config(update) mach(CPUBOARD=IP30)" machdepstart.ip30
	# make autoconfig a real command
	$(INSTALL) -F /etc -lns init.d/autoconfig autoconfig
	# All of the following file should be install at least in the mini-root
	$(INSCMD) -idb "config(update)" $(MRUPDATE)
	$(INSCMD) -src sysetup.mr sysetup
	$(INSCMD) -src filesystems.mr filesystems
	$(INSTALL) -F $(CFGDIR) -idb 'config(noupdate)' -u root -g sys -m 644 \
		nostickytmp
