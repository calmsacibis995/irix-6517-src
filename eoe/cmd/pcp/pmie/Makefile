#!smake
# 
# $Revision: 1.1 $

PCP_SRC_DEPTH	= ..
include $(PCP_SRC_DEPTH)/include/commondefs
include $(PCP_SRC_DEPTH)/include/isacommondefs

#if $(ISM_NAME) == "pcp"
SUBDIRS	+= examples
#endif

CONFIG	= /var/pcp/config/pmie
IDBTAG	= PMIE

install!	default
	$(INSTALL) -F $(CONFIG) -m 444 -idb "$(IDBTAG) config(suggest)" control
	$(INSTALL) -F $(CONFIG) -m 444 -idb "$(IDBTAG) config(suggest)" crontab
	$(INSTALL) -F /etc/config -m 444 -idb "$(IDBTAG) config(suggest)" -src chkconfig.pmie pmie
	$(INSTALL) -F /etc/init.d -src etc_init.d_pmie -m 555 -idb "$(IDBTAG)" pmie
	$(INSTALL) -F /etc/rc2.d -lns ../init.d/pmie -idb "$(IDBTAG)" S98pmie
	$(INSTALL) -F /etc/rc0.d -lns ../init.d/pmie -idb "$(IDBTAG)" K02pmie
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb $(IDBTAG) -src pmie_check.sh pmie_check
#if $(BEFORE_IRIX6_5)
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" -src 32/pmie pmie
#else
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" -src N32/pmie pmie
#endif
#if $(ISM_NAME) == "pcp"
	cd examples; make install
#endif

#if $(ISM_NAME) == "pcp"
default!
	cd examples; make default
#endif
