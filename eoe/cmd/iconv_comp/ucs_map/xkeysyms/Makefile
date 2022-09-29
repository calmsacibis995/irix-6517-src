#!smake -J 1
#

include $(ROOT)/usr/include/make/commondefs

GENXLTXKEYS = ../../genxlt/genxlt_xkeys2ucs4
GENXLTUCSKS = ../../genxlt/genxlt_ucs42xkeys

TOUCS4FILES = \
		XKEYS_UCS-4.ucs_map

TOXKEYSFILES = \
		UCS-4_XKEYS.ucs_map


TOUCS4MAPFILES = $(TOUCS4FILES:.ucs_map=.map)
TOXKEYSMAPFILES = $(TOXKEYSFILES:.ucs_map=.map)
MAPFILES  = $(TOUCS4MAPFILES) $(TOXKEYSMAPFILES)


default all targets:
	$(GENXLTXKEYS)  -v  -f XKEYS  -t UCS-4  -i XKEYS_UCS-4.ucs_map  >  iconv.spec.xkeys
	$(GENXLTUCSKS)  -v  -f UCS-4  -t XKEYS  -i UCS-4_XKEYS.ucs_map  >>  iconv.spec.xkeys


clobber: clean

clean:
	rm -f Makedepend
	rm -f *.map *.log iconv.spec.xkeys

install: default
	$(INSTALL) -m 444 -F /usr/lib/iconv/tables -idb EOE $(MAPFILES)
	$(INSTALL) -m 444 -F /usr/lib/iconv/spec -idb EOE iconv.spec.xkeys
