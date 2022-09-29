#!smake
#	"$Revision: 1.16 $"

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS	= include lib libtcl xlv_plexd \
	xlv_assemble xlv_label xlv_make xlv_labd xlv_shutdown \
	lv_to_xlv xlv_set_primary xlv_mgr

default lint install debug $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(_FORCE):

