#!smake
#
# "$Revision: 1.5 $"

include ../Makedefs

SUBDIRS =	iconlib
TARGETS_FTR =	xfs.ftr


default lint debug $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

install:
	$(SUBDIRS_MAKERULE)
	${INSTALL} $(FTR_EXITOP_IDB) -u root -g sys -m 0644 \
		-F ${INSTDIR_FTR} ${TARGETS_FTR}

$(_FORCE):
