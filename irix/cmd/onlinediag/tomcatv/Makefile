include $(ROOT)/usr/include/make/commondefs

TARGETS=tomcatv runtest

INSTALLDIR = /usr/diags/online_diags/tomcatv
IDBFLAGS = -idb "std.sw.unix" -idb "mach(CPUBOARD=IP25 CPUBOARD=IP27)"
default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} $(IDBFLAGS) -F $(INSTALLDIR) $(TARGETS)
	${INSTALL} ${IDBFLAGS} -F $(INSTALLDIR)/test/input -src test/input/TOMCATV.MODEL TOMCATV.MODEL
	${INSTALL} ${IDBFLAGS} -F $(INSTALLDIR)/test/input -src test/input/tomcatv.in tomcatv.in
	${INSTALL} ${IDBFLAGS} -F $(INSTALLDIR)/test/output -src test/output/tomcatv.out tomcatv.out
	${INSTALL} ${IDBFLAGS} -F $(INSTALLDIR)/train/input -src train/input/TOMCATV.MODEL TOMCATV.MODEL
	${INSTALL} ${IDBFLAGS} -F $(INSTALLDIR)/train/input -src train/input/tomcatv.in tomcatv.in
	${INSTALL} ${IDBFLAGS} -F $(INSTALLDIR)/train/output -src train/output/tomcatv.out tomcatv.out
	${INSTALL} ${IDBFLAGS} -F $(INSTALLDIR)/ref/input -src ref/input/TOMCATV.MODEL TOMCATV.MODEL
	${INSTALL} ${IDBFLAGS} -F $(INSTALLDIR)/ref/input -src ref/input/tomcatv.in tomcatv.in
	${INSTALL} ${IDBFLAGS} -F $(INSTALLDIR)/ref/output -src ref/output/tomcatv.out tomcatv.out
