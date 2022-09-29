# Makefile for ssdb
#
#       "$Revisions$"
#
include $(ROOT)/usr/include/make/commondefs

HEADERS_SUBDIRS=
EXPORTS_SUBDIRS=
SUBDIRS = semd_config availability_plugin semui_plugin plugin_datafiles   \
	  webconfig_plugin subscribe_plugin config_plugin amreport_plugin \
	  qpage_plugin semlynxui_plugin sgm_plugin ssping_plugin \
          pmieconf_plugin diag_plugin

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
