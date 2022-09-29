#!smake
#
# $Revision: 1.15 $
#
include ${ROOT}/usr/include/make/commondefs

SUBDIRS=lib nlib clri fsck fsdb fsr fsstat fstyp growfs labelit mkfs ncheck \
	resymlink tools

all install ${COMMONTARGS}:
	@for d in ${SUBDIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
	done

default:
	@for d in ${SUBDIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE}"; \
		cd $$d; ${MAKE}; cd ..; \
	done
