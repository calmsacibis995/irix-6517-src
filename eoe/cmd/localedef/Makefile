#ident "$Revision: 1.8 $"

include $(ROOT)/usr/include/make/commondefs

SUBDIRS= chrtbl montbl colltbl datetbl locdef \
	 en_US da de de_AT de_CH el en en_AU en_CA es fi \
	 fr fr_BE fr_CA fr_CH hu is it it_CH nl nl_BE no \
	 pl pt ru sk sv tr cs es_AR es_MX pt_BR \
	 ISO8859-15

COMMONPREF=localedef

default install $(COMMONTARGS): $(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)

include $(COMMONRULES)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(COMMONPREF)default:

$(COMMONPREF)install: $(COMMONPREF)default
	$(INSTALL) -F /usr/lib/locale -lns C POSIX

