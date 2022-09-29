#
# Makefile for cmd/diskless, diskless miscellany.
#
include ${ROOT}/usr/include/make/commondefs
MKDEPFILE= Makedepend

LCINCS=
LLDLIBS=

CFILES= registrar.c yp_bootparam.c updbootparam.c yp_host.c
SHFILES= dl_inst.sh client_inst.sh share_inst.sh clone_client.sh

TARGETS= ${CFILES:.c=} ${SHFILES:.sh=}
DLOPTS_SRV= -F /var/boot -u root -g sys
DLOPTS_CLT= -F /var/boot -u root -g sys

default: ${TARGETS}

COMMONPREF=dl_
include ${COMMONRULES}

install $(COMMONTARGS): $(COMMONPREF)$$@

$(COMMONPREF)install: default
	${INSTALL} -F /usr/sbin -u root -g sys \
		registrar updbootparam yp_bootparam yp_host
	${INSTALL} ${DLOPTS_SRV} -m 444 share.dat client.dat
	${INSTALL} ${DLOPTS_SRV} -src dl_inst -O .dl_inst
	${INSTALL} ${DLOPTS_SRV} -m 444 -src dl_classes -O .dl_classes
	${INSTALL} ${DLOPTS_SRV} client_inst share_inst clone_client
	${INSTALL} ${DLOPTS_SRV} client_setup share_setup verify
	${INSTALL} ${DLOPTS_SRV} -m 755 -src dl_setup -O .dl_setup
	${INSTALL} ${DLOPTS_CLT} makedev	# Force /dev/MAKEDEV to build
