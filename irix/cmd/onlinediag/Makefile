include $(ROOT)/usr/include/make/commondefs
IN_DIR = $(ROOT)/cmd/onlinediag
TARGETS= onlinediag diagsetup
SUBDIRS = tomcatv

default:$(TARGETS)
	$(SUBDIRS_MAKERULE)

COMMONPREF=onlinediag_
include $(COMMONRULES)

PKGNAME = -idb "std.sw.unix"
IDBTAG_ON=-idb "mach(CPUBOARD=IP25 CPUBOARD=IP27)"
IDBTAG_OFF=-idb "mach(CPUBOARD=IP28 CPUBOARD=IP30 CPUBOARD=IP32)"

install: default
	${INSTALL} $(PKGNAME) -idb "mach(CPUARCH=R10000)" -F /usr/diags/bin $(TARGETS)
	${INSTALL} $(PKGNAME) -idb "mach(CPUARCH=R10000)" -m 644 -F /etc/config onlinediag.config
	$(INSTALL) $(PKGNAME) $(IDBTAG_ON) -m 644 -F /var/spool/cron/crontabs -src diag  diag
	$(INSTALL) $(PKGNAME) $(IDBTAG_OFF) -m 644 -F /var/spool/cron/crontabs -src diag.off diag
	$(SUBDIRS_MAKERULE)

clean: 
	(rm -rf *.[oue] a.out core lex.yy.[co] y.tab.[cho] _force ar.tmp.*)
	$(SUBDIRS_MAKERULE)
clobber:
	(rm -f *.o $(TARGETS))
	(rm -rf *.[oue] a.out core lex.yy.[co] y.tab.[cho] _force ar.tmp.*)
	(rm -rf Makedepend)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	@$(ECHO) "====\t$@"; cd $@; $(MAKE)
