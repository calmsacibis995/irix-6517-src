#!smake -J 1
#

include $(ROOT)/usr/include/make/commondefs

GENXLT2UCS = ../../genxlt/genxlt_sbcs2ucs
GENXLT2SBCS = ../../genxlt/genxlt_2sbcs -s 2

TOUCSFILES = \
		ISO8859-2_UCS-2.ucs_map \
		ISO8859-3_UCS-2.ucs_map \
		ISO8859-4_UCS-2.ucs_map \
		ISO8859-5_UCS-2.ucs_map \
		ISO8859-6_UCS-2.ucs_map \
		ISO8859-7_UCS-2.ucs_map \
		ISO8859-8_UCS-2.ucs_map \
		ISO8859-9_UCS-2.ucs_map \
		ISO8859-15_UCS-2.ucs_map \
		KOI8_UCS-2.ucs_map \
		DOS855_UCS-2.ucs_map \
		WIN1251_UCS-2.ucs_map \
		SYMBOL_UCS-2.ucs_map \
		DINGBATS_UCS-2.ucs_map \
		BITSTREAM_UCS-2.ucs_map

TOSBCSFILES = \
		UCS-2_ISO8859-2.ucs_map \
		UCS-2_ISO8859-3.ucs_map \
		UCS-2_ISO8859-4.ucs_map \
		UCS-2_ISO8859-5.ucs_map \
		UCS-2_ISO8859-6.ucs_map \
		UCS-2_ISO8859-7.ucs_map \
		UCS-2_ISO8859-8.ucs_map \
		UCS-2_ISO8859-9.ucs_map \
		UCS-2_ISO8859-15.ucs_map \
		UCS-2_KOI8.ucs_map \
		UCS-2_DOS855.ucs_map \
		UCS-2_WIN1251.ucs_map \
		UCS-2_SYMBOL.ucs_map \
		UCS-2_DINGBATS.ucs_map \
		UCS-2_BITSTREAM.ucs_map

TOUCSMAPFILES = $(TOUCSFILES:.ucs_map=.map)
TOSBCSMAPFILES = $(TOSBCSFILES:.ucs_map=.map)
MAPFILES  = $(TOUCSMAPFILES) $(TOSBCSMAPFILES)

default all targets:
	$(GENXLT2UCS) -v -f ISO8859-2 -t UCS-2 -i ISO8859-2_UCS-2.ucs_map > iconv.spec.elm
	$(GENXLT2UCS) -v -f ISO8859-3 -t UCS-2 -i  ISO8859-3_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f ISO8859-4 -t UCS-2 -i  ISO8859-4_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f ISO8859-5 -t UCS-2 -i  ISO8859-5_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f ISO8859-6 -t UCS-2 -i  ISO8859-6_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f ISO8859-7 -t UCS-2 -i  ISO8859-7_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f ISO8859-8 -t UCS-2 -i  ISO8859-8_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f ISO8859-9 -t UCS-2 -i  ISO8859-9_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f ISO8859-15 -t UCS-2 -i  ISO8859-15_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f KOI8 -t UCS-2 -i  KOI8_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f DOS855 -t UCS-2 -i  DOS855_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f WIN1251 -t UCS-2 -i  WIN1251_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f SYMBOL -t UCS-2 -i  SYMBOL_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f DINGBATS -t UCS-2 -i  DINGBATS_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2UCS) -v -f BITSTREAM -t UCS-2 -i  BITSTREAM_UCS-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t ISO8859-2 -i UCS-2_ISO8859-2.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t ISO8859-3 -i UCS-2_ISO8859-3.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t ISO8859-4 -i UCS-2_ISO8859-4.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t ISO8859-5 -i UCS-2_ISO8859-5.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t ISO8859-6 -i UCS-2_ISO8859-6.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t ISO8859-7 -i UCS-2_ISO8859-7.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t ISO8859-8 -i UCS-2_ISO8859-8.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t ISO8859-9 -i UCS-2_ISO8859-9.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t ISO8859-15 -i UCS-2_ISO8859-15.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t KOI8 -i UCS-2_KOI8.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t DOS855 -i UCS-2_DOS855.ucs_map  >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t WIN1251 -i UCS-2_WIN1251.ucs_map >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t SYMBOL -i UCS-2_SYMBOL.ucs_map >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t DINGBATS -i UCS-2_DINGBATS.ucs_map >> iconv.spec.elm
	$(GENXLT2SBCS) -v -f UCS-2 -t BITSTREAM -i UCS-2_BITSTREAM.ucs_map >> iconv.spec.elm

clobber: clean

clean:
	rm -f Makedepend
	rm -f *.map *.log iconv.spec.elm

install: default
	$(INSTALL) -m 444 -F /usr/lib/iconv/tables -idb EOE $(MAPFILES)
	$(INSTALL) -m 444 -F /usr/lib/iconv/spec -idb EOE iconv.spec.elm
