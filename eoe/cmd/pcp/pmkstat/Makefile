#!smake
#
# pmkstat - multiple object code formats must be made (one for each
# bootable kernel format) ... these sometimes use PM_CONTEXT_LOCAL
# rather than PM_CONTEXT_HOST
#
# $Revision: 2.14 $

LLDIRT		= *.log *.dir *.pag

PCP_SRC_DEPTH = ..
include $(PCP_SRC_DEPTH)/include/commondefs
include $(PCP_SRC_DEPTH)/include/isacommondefs

IDBTAG		= PCP_EOE

install!	default
#if $(BEFORE_IRIX6_5)
	$(I_32) $(INSTALL) -F /usr/sbin -idb "$(IDBTAG) $(STRIPBIN) $(MODE32)" -g sys -m 2555 -src 32/pmkstat pmkstat
#else
	$(I_N32) $(INSTALL) -F /usr/sbin -idb "$(IDBTAG) $(STRIPBIN) $(MODE32)" -g sys -m 2555 -src N32/pmkstat pmkstat
#endif
	$(I_64) $(INSTALL) -F /usr/sbin -idb "$(IDBTAG) $(STRIPBIN) $(MODE64)" -g sys -m 2555 -src 64/pmkstat pmkstat
	$(INSTALL) -F $(PMLOGGERCONFIG) -m 444 -idb $(IDBTAG) -src pmlogger.config config.pmkstat

