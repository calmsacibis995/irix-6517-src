#!smake
#
# Makefile for ISMNAME/books
#
# This makefile passes all targets to the lower level makefiles
#
include $(ROOT)/usr/include/make/commondefs

COMMONPREF=books
SUBDIRS= NIS_AG IRIX_NetPG UI_Glines Diskless_AG \
	IIDsktp_IG T_IRIX_Prog ONC3NFS_AG \
	DevDriver_PG REACT_PG IA_BakSecAcc IA_DiskFiles \
	IA_NetwkMail IA_ConfigOps IA_InstLicns IA_Periphrls \
	XFSMHelp WorkGrp_AG CPR_OG Combiner_UG TCMW_AG TCMW_UG \
	CrayMsg_PG OViewHelp PmRunHelp PmTimeHelp FlatPanel_Help \
	ESP_UG ESP_OV PCP_IRIX IA_Resource

default headers exports : $(_FORCE)

$(COMMONTARGS) : $(_FORCE)
#ifmake clobber || rmtargets
	@for d in $(SUBDIRS); do \
		(cd $$d ; echo "    (cd $$d)" ; make clean ) ;\
	done
#else
	$(SUBDIRS_MAKERULE)
#endif


install::
	-@for d in $(SUBDIRS); do (cd $$d; mkdir xxxtmpdir 1> /dev/null 2>&1; /bin/mv -f *.doc xxxtmpdir 1> /dev/null 2>&1); done
	@OLDIDB=$$RAWIDB; RAWIDB=$$RAWIDB.books; export OLDIDB RAWIDB; \
	if [ -f $$RAWIDB ] ; then rm -f $$RAWIDB ; fi ; \
	echo "RAWIDB="$$RAWIDB" SRC=$(SRC) $(MAKE) install" ; \
	for d in $(SUBDIRS); do $(SUBDIR_MAKERULE); done ; \
	cat $$RAWIDB >> $$OLDIDB
	-@for d in $(SUBDIRS); do (cd $$d; /bin/mv -f xxxtmpdir/*.doc . 1> /dev/null 2>&1; rmdir xxxtmpdir); done

include $(COMMONRULES)
