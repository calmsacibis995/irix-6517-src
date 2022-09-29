#!smake
#
# libpcp_inv.a - noship Inventor interface
#
# $Revision: 1.6 $

LIBRARY	= libpcp_inv.a
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
INCDIR  = /usr/include/pcp/inv

install: headers exports default
#if $(ISM_NAME) == "pcp"
	#
	# pcp_noship
	#
	$(I_32) $(INSTALL) -F /usr/lib -idb "$(IDBTAG) $(STRIPBIN)" -m 555 -src 32/$(LIBRARY) $(LIBRARY)
	$(I_N32) $(INSTALL) -F /usr/lib32 -idb "$(IDBTAG) $(STRIPBIN)" -m 555 -src N32/$(LIBRARY) $(LIBRARY)
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/App.h App.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/ColScaleMod.h ColScaleMod.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/ColorMod.h ColorMod.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/ColorScale.h ColorScale.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Form.h Form.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/FormUI.h FormUI.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Inv.h Inv.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Launch.h Launch.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/LaunchMenu.h LaunchMenu.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Menu.h Menu.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/MetricList.h MetricList.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/ModList.h ModList.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Modulate.h Modulate.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Record.h Record.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/ScaleMod.h ScaleMod.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/StackMod.h StackMod.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/ToggleMod.h ToggleMod.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/View.h View.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/Window.h Window.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG)" -m 444 -src src/YScaleMod.h YScaleMod.h
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
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/App.h App.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/ColScaleMod.h ColScaleMod.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/ColorMod.h ColorMod.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/ColorScale.h ColorScale.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Form.h Form.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/FormUI.h FormUI.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Inv.h Inv.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Launch.h Launch.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/LaunchMenu.h LaunchMenu.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Menu.h Menu.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/MetricList.h MetricList.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/ModList.h ModList.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Modulate.h Modulate.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Record.h Record.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/ScaleMod.h ScaleMod.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/StackMod.h StackMod.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/ToggleMod.h ToggleMod.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/View.h View.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/Window.h Window.h
	$(INSTALL) -F $(INCDIR) -idb "$(IDBTAG_ROOT)" -m 444 -src src/YScaleMod.h YScaleMod.h
#endif
