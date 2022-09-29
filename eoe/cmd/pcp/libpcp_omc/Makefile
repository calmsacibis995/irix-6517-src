#!smake
#
# libpcp_omc.a - noship Old Metrics Class
#
# $Revision: 1.10 $

LIBRARY	= libpcp_omc.a
LLDIRT	= *.log *.dir *.pag $(LIBRARY)

PCP_SRC_DEPTH = ..
include $(PCP_SRC_DEPTH)/include/commondefs
STYLES_TO_BUILD = $(DEF_OBJECT_STYLE)
include $(PCP_SRC_DEPTH)/include/isacommondefs

#if !empty(DEF_OBJECT_STYLE:M32*)
I_32=
I_N32=\#
#endif
#if !empty(DEF_OBJECT_STYLE:MN32*)
I_32=\#
I_N32=
#endif

IDBTAG	= NOSHIP_EOE
IDBTAG_ROOT = PCP_EOE_ROOT
INCDIR	= /usr/include/pcp/omc

install: headers exports default
#if $(ISM_NAME) == "pcp"
	$(I_32) $(INSTALL) -F /usr/lib -idb "$(IDBTAG) $(STRIPBIN)" -m 555 -src 32/$(LIBRARY) $(LIBRARY)
	$(I_N32) $(INSTALL) -F /usr/lib32 -idb "$(IDBTAG) $(STRIPBIN)" -m 555 -src N32/$(LIBRARY) $(LIBRARY)
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Args.h Args.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Bool.h Bool.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Context.h Context.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Desc.h Desc.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Hash.h Hash.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Indom.h Indom.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/List.h List.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Metric.h Metric.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Source.h Source.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/String.h String.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Vector.h Vector.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Hash.C Hash.C
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/List.C List.C
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Vector.C Vector.C
#endif

exports: headers default
#if $(BEFORE_IRIX6_5) || $(ISM_NAME) == "irix"
	#
	# pcp_eoe_root.sw.root
	#
	$(I_32) $(INSTALL) -F /usr/lib -idb "$(IDBTAG_ROOT) $(STRIPBIN)" -m 555 -src 32/$(LIBRARY) $(LIBRARY)
	$(I_N32) $(INSTALL) -F /usr/lib32 -idb "$(IDBTAG_ROOT) $(STRIPBIN)" -m 555 -src N32/$(LIBRARY) $(LIBRARY)
#endif

headers:
#if $(BEFORE_IRIX6_5) || $(ISM_NAME) == "irix"
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Args.h Args.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Bool.h Bool.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Context.h Context.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Desc.h Desc.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Hash.h Hash.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Indom.h Indom.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/List.h List.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Metric.h Metric.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Source.h Source.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/String.h String.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Vector.h Vector.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Hash.C Hash.C
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/List.C List.C
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Vector.C Vector.C
#endif

