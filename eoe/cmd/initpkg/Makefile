#ident	"$Revision: 1.63 $"

include	$(ROOT)/usr/include/make/commondefs

FILES_STD=inittab
FILES_MR=inittab-mr
SCRIPT_STD=brc bcheckrc mountall umountfs   
SCRIPT_MR=brc-mr bcheckrc-mr restore_system-mr
SCRIPT_MRRC=mrcleanrc mrconfigrc mrcustomrc mrinitrc mrinitxlvrc mrinstrc \
	mrlogrc mrmkfsrc mrmountrc mrnetrc mrprofrc mrreorgrc mrvolhdrrc
OBJECTS_MR=gethostid inet_aton setsid mrinterface

# None of these are really configuration files that the user might change.
SCRIPT	=lnsyscon rc0 rc2 rc3 shutdown reboot halt single multi mntproc mnthwgfs

DIRS	=init.d rc0.d rc2.d rc3.d

TARGETS = $(SCRIPT) $(SCRIPT_MR) $(SCRIPT_MRRC) $(SCRIPT_STD) $(OBJECTS_MR)

INSDIR	=/etc
INSCMD	=$(INSTALL) -F $(INSDIR)

COMMONPREF = init_

default:$(FILES_STD) $(FILES_MR) $(TARGETS)

include $(COMMONRULES)

${COMMONTARGS} install: ${COMMONPREF}$$@
	@for d in ${DIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
	done

$(COMMONPREF)install: default
	$(INSCMD) -m 644 -idb "std.sw.unix config(update)" "$(FILES_STD)"
	$(INSCMD) -m 755 -idb "std.sw.unix config(update)" "$(SCRIPT_STD)"
	$(INSCMD) -m 755 -idb "std.sw.unix mr" "$(SCRIPT)"
	$(INSCMD) -m 755 $(SCRIPT_MRRC) $(OBJECTS_MR)
	for nm in $(SCRIPT_MR) $(FILES_MR); do \
	    $(INSCMD) -m 755 -idb "mr" -src $$nm `expr $$nm : '\\(.*\\)-mr'`; \
	done
