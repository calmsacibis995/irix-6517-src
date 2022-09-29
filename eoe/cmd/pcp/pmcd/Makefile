#!smake
#
# pmcd - multiple object code formats must be made (one for each
# bootable kernel format)
#
# $Revision: 2.30 $

LLDIRT		= *.log *.dir *.pag so_locations

PCP_SRC_DEPTH = ..
include $(PCP_SRC_DEPTH)/include/commondefs
include $(PCP_SRC_DEPTH)/include/isacommondefs

IDBTAG		= PCP_EOE

# Note: pmcd is not stripped so the DSO PMDAs may be debugged while attached.

install!	default
	$(INSTALL) -F /etc -m 444 -idb "$(IDBTAG) config(suggest)" pmcd.conf
	$(INSTALL) -F /etc/config -src chkconfig.pmcd -m 444 -idb "$(IDBTAG) config(suggest)" pmcd
	$(INSTALL) -F /etc/config -m 444 -idb "$(IDBTAG) config(suggest)" pmcd.options
	$(INSTALL) -F /etc/init.d -src etc_init.d_pcp -m 555 -idb "$(IDBTAG)" pcp
	$(INSTALL) -F /etc/rc2.d -lns ../init.d/pcp -idb "$(IDBTAG)" S98pcp
	$(INSTALL) -F /etc/rc0.d -lns ../init.d/pcp -idb "$(IDBTAG)" K02pcp
	$(INSTALL) -F /usr/pcp/lib -m 444 -idb "$(IDBTAG)" pmdaproc.sh
#if $(PCP_EOE_NOSHIP)
	$(INSTALL) -F /usr/pcp/lib -m 444 -idb "PCP_EOE_NOSHIP" pmdaproc.sh
#endif
#if $(BEFORE_IRIX6_5)
	$(I_32) $(INSTALL) -F /usr/etc -g sys -m 2555 -idb "$(IDBTAG) $(MODE32) nostrip" -src 32/pmcd pmcd
	$(I_64) $(INSTALL) -F /usr/etc -g sys -m 2555 -idb "$(IDBTAG) $(MODE64) nostrip" -src 64/pmcd pmcd
#else
	$(I_N32) $(INSTALL) -F /usr/etc -m 555 -idb "$(IDBTAG) $(MODE32) nostrip" -src N32/pmcd pmcd
	$(I_64) $(INSTALL) -F /usr/etc -m 555 -idb "$(IDBTAG) $(MODE64) nostrip" -src 64/pmcd pmcd
#endif
	$(INSTALL) -idb "$(IDBTAG)" -m 755 -dir /var/pcp/etc
	$(INSTALL) -f /var/pcp/etc -src etc_init.d_pcplocal -m 555 -idb "$(IDBTAG) config(suggest)" local
