#!smake
#
# "$Revision: 1.3 $"

include ../../Makedefs

TARGETS_FTI =	disk.app.fti disk.fti disk.templ.fti \
		filesystem.app.fti filesystem.fti filesystem.remote.fti \
		filesystem.remote.templ.fti filesystem.templ.fti \
		plex.fti plex.templ.fti \
		toplevel.app.fti \
		ve.fti ve.templ.fti \
		volume.app.fti volume.fti volume.templ.fti

default lint debug $(COMMONTARGS): $(_FORCE)

install:
	${INSTALL} -idb ${CLIENT_TAG} -u root -g sys -m 0644 \
		-F ${INSTDIR_FTI} ${TARGETS_FTI}

$(_FORCE):
