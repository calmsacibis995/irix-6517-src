# Makefile for installing configuration files.
#
#	"$Revision: 1.15 $"

include	${ROOT}/usr/include/make/commondefs

NFILES	=named rwhod timed timeslave gated mrouted timed.options inetd.options rsvpd
NFILES_MR=routed	# belongs in std. location and miniroot, too.
NSFILES	=routed.options
QFILES	=quotas quotacheck
FILES	=${NFILES} ${NFILES_MR} ${QFILES} ${NSFILES}
T6FILES	=iflabel-1.options

NIDBTAG	=-idb "config(noupdate)"
NIDBTAG_MR= ${NIDBTAG} -idb mr
NSIDBTAG=-idb "config(noupdate)"
QIDBTAG	=-idb "config(noupdate)"

INST	= ${INSTALL} -F /etc/config -m 644

default: ${FILES}

include ${COMMONRULES}

install: default
	${INST} ${NIDBTAG}    ${NFILES}
	${INST} ${NIDBTAG_MR} ${NFILES_MR}
	${INST} ${QIDBTAG}    ${QFILES}
	${INST} ${NSIDBTAG}   ${NSFILES}
	${INST} -idb mr -src routed.options.mr routed.options
	${INST} ${NIDBTAG}    ${T6FILES}
