#!smake
#

CMDS	= bin/newhelp bin/pmnscomp bin/pmnsmerge bin/pminfo bin/pmieconf
WRAPPERS= $(CMDS:S/bin\///g)

IDBTAG = NOSHIP_EOE
IDBTAG_ROOT = PCP_EOE_ROOT
BUILDTOOLS = /usr/pcp/buildtools

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

#if $(ISM_NAME) == "pcp"
LIBPCP_PATH = `pwd`/$(PCP_SRC_DEPTH)/libpcp/BUILD
LIBPCP_DEV_PATH = `pwd`/$(PCP_SRC_DEPTH)/libpcp_dev/BUILD
LIBPCP_PMDA_PATH = `pwd`/$(PCP_SRC_DEPTH)/libpcp_pmda/BUILD
#elif $(ISM_NAME) == "irix"
LIBPCP_PATH = $(ROOT)/usr/lib
LIBPCP_DEV_PATH = $(ROOT)/usr/lib
LIBPCP_PMDA_PATH = $(ROOT)/usr/lib
#else
eh?
#endif

.NOTPARALLEL:

default: $(CMDS) $(WRAPPERS) check_help_src esp_events

install: default exports

$(CMDS) $(WRAPPERS) : Make
	./Make $@ $(LIBPCP_PATH) $(LIBPCP_DEV_PATH) $(LIBPCP_PMDA_PATH)

clean clobber:
	rm -rf $(CMDS) $(WRAPPERS) bin

headers:

exports: default
#if $(BEFORE_IRIX6_5) || $(ISM_NAME) == "irix"
	#
	# pcp_eoe_root.sw.root
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" check_help_src
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" esp_events
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" -src bin/pmieconf pmieconf.exe
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" -src bin/newhelp newhelp.exe
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" -src bin/pmnscomp pmnscomp.exe
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" -src bin/pmnsmerge pmnsmerge.exe
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" -src bin/pminfo pminfo.exe
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" -src pmieconf.root pmieconf
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" -src newhelp.root newhelp
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" -src pminfo.root pminfo
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" -src pmnscomp.root pmnscomp
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG_ROOT)" -src pmnsmerge.root pmnsmerge
#endif
#if $(ISM_NAME) == "pcp"
	#
	# pcp_noship.sw.eoe
	$(INSTALL) -idb $(IDBTAG) -m 755 -dir $(BUILDTOOLS)
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" check_help_src
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" esp_events
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" -src bin/pmieconf pmieconf.exe
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" -src bin/newhelp newhelp.exe
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" -src bin/pmnscomp pmnscomp.exe
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" -src bin/pmnsmerge pmnsmerge.exe
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" -src bin/pminfo pminfo.exe
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" -src pmieconf.exports pmieconf
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" -src newhelp.exports newhelp
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" -src pminfo.exports pminfo
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" -src pmnscomp.exports pmnscomp
	$(INSTALL) -F $(BUILDTOOLS) -m 755 -idb "$(IDBTAG)" -src pmnsmerge.exports pmnsmerge
#endif
