#!smake
# Copyright 1993, Silicon Graphics Inc., Mountain View, CA. 
#
# Master makefile for loadable device driver subsystems.
#
#ident	"$Revision: 1.36 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/lio
.PATH:$(KPATH)

#
# loadable kernel modules must be built with these options.
# GNUMVAL is necessary so the ld uses -G 0, rather than -G 8.
#
GNUMVAL=0
LCOPTS=$(GKOPTS) $(GKCOPTS) $(JALR) -G 0

#
# IP20 Parallel Port
#
#if $(CPUBOARD) == "IP20"
HPCPLP_NAME=hpcplp.o
#endif

#
# IP22/IP26/IP28 Parallel Port
#
#if $(CPUBOARD) == "IP22" || $(CPUBOARD) == "IP26" || $(CPUBOARD) == "IP28"
HPCPLP_NAME=hpc3plp.o
#endif

#if $(CPUBOARD) == "IP27" || $(CPUBOARD) == "IP30"
HPCPLP_NAME=ecpplp.o
#endif

#
# PCIIO-based PCI Bus Access
#
#if $(CPUBOARD) == "IP27" || $(CPUBOARD) == "IP30" || $(CPUBOARD) == "IP32"
PCIBA_NAME=pciba.o
#endif

#
# IP32 Parallel Port
#
#if $(CPUBOARD) == "IP32"
ECPLP_NAME=ecplp.o
#endif

#
# XTALK-based XIO Bus Access
#
#if $(CPUBOARD) == "IP27" || $(CPUBOARD) == "IP30"
XIOBA_NAME=xioba.o
#endif

#
# Inode Monitor
#
IMON_NAME=imon.o

#
# SVR4 kbd driver
#
KBD_NAME=kbd.o

#
# scsi tape driver
#
TPSC_NAME=tpsc.o

TPI_SRCS= ticlts.c
TPI_OBJS= $(TPI_SRCS:.c=.o)

#
# SCSI Floppy
#
SMFD_NAME=smfd.o

#
# Specify system specific modules
# Split up into 2 groups - those shipped with the system STD_TARGETS
# and those that are seperate products OPT_TARGETS
# Each of these groups is split into base and alternate names:
# {STD,OPT}_ALTTARGETS
#
IP20INST=\#
IP22INST=\#
IP26INST=\#
IP28INST=\#
IP30INST=\#
IP32INST=\#

#if $(CPUBOARD) == "IP19"
STD_TARGETS=
OPT_TARGETS=mldtest.o
STD_ALTTARGETS=
OPT_ALTTARGETS=
IP19INST=
#endif

#if $(CPUBOARD) == "IP20"
STD_TARGETS=$(HPCPLP_NAME)
OPT_TARGETS=
STD_ALTTARGETS=
OPT_ALTTARGETS=
IP20INST=
#endif

#if $(CPUBOARD) == "IP21"
STD_TARGETS=
OPT_TARGETS=
STD_ALTTARGETS=
OPT_ALTTARGETS=
IP21INST=
#endif

#if $(CPUBOARD) == "IP25"
STD_TARGETS=
OPT_TARGETS=
STD_ALTTARGETS=
OPT_ALTTARGETS=
IP25INST=
#endif

#if $(CPUBOARD) == "IP22"
STD_TARGETS=$(HPCPLP_NAME)
OPT_TARGETS=
STD_ALTTARGETS=
OPT_ALTTARGETS=
IP22INST=
#endif

#if $(CPUBOARD) == "IP26"
STD_TARGETS=$(HPCPLP_NAME)
OPT_TARGETS=
STD_ALTTARGETS=
OPT_ALTTARGETS=
IP26INST=
#endif

#if $(CPUBOARD) == "IP28"
STD_TARGETS=$(HPCPLP_NAME)
OPT_TARGETS=
STD_ALTTARGETS=
OPT_ALTTARGETS=
IP28INST=
#endif

#if $(CPUBOARD) == "IP27"
STD_TARGETS=$(HPCPLP_NAME) $(XIOBA_NAME) $(PCIBA_NAME)
OPT_TARGETS=
STD_ALTTARGETS=
OPT_ALTTARGETS=
IP27INST=
#endif

#if $(CPUBOARD) == "IP30"
STD_TARGETS=$(HPCPLP_NAME) $(FLASH_NAME) $(XIOBA_NAME) $(PCIBA_NAME)
OPT_TARGETS=
STD_ALTTARGETS=
OPT_ALTTARGETS=
IP30INST=
#endif

#if $(CPUBOARD) == "IP32" || $(CPUBOARD) == "IP32SIM"
STD_TARGETS=$(ECPLP_NAME) $(PCIBA_NAME)
OPT_TARGETS=
STD_ALTTARGETS=
OPT_ALTTARGETS=
IP32INST=
#endif

# lint
LINTNAME=lio
CFILES= \
	hpcplp.c \
	hpc3plp.c \
	ecpplp.c \
	ecplp.c \
	ticlts.c \
	kbd.c \
	tpsc.c \
	smfd.c \
	sflash.c

#
# Rules
#
include ${DEPTH}/kcommonrules

#
# For installation reasons, split TARGETS up into real and alternate names
#
# NOTE: For this makefile, it is possible to have none of STD_TARGETS,
# OPT_TARGETS, STD_ALTTARGETS or OPT_ALTTARGETS defined for a particular
# product. Because of this, for the install rules, we must check to see if 
# these are set to anything, otherwise install will be passed nothing
# and it will fail. You can not use  NONALTTARGETS, ALTTARGETS and TARGETS
# for the test because they are set to "  ", rather than "". These must
# be set though for clobber, etc.
#

NONALTTARGETS=$(STD_TARGETS) $(OPT_TARGETS)

ALTTARGETS=$(STD_ALTTARGETS) $(OPT_ALTTARGETS)

TARGETS=$(NONALTTARGETS) $(ALTTARGETS)

$(KCOMMONPREF)default:$(TARGETS) $(TPI_OBJS) $(IMON_NAME) $(KBD_NAME) \
	$(TPSC_NAME) $(SMFD_NAME)
	if [ -n "${STD_TARGETS}" -o -n "${OPT_TARGETS}" -o -n "${STD_ALTTARGETS}" -o -n "${OPT_ALTTARGETS}" ] ; then\
		${KLINKINSTALL} ${TARGETS};\
	fi
	${KLINKINSTALL} $(TPI_OBJS) $(IMON_NAME) $(KBD_NAME) \
		$(TPSC_NAME) $(SMFD_NAME)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	if test -n "${STD_TARGETS}"; then\
		${KINSTALL} ${STD_TARGETS};\
	fi
	${KINSTALL} -idb svr4net $(TPI_OBJS)
	${KINSTALL} $(IMON_NAME) $(KBD_NAME) $(TPSC_NAME) $(SMFD_NAME)
	@#
	@# Machine/system specific installs here
	@#
	@#
	@# Install everything for developers
	@#
	if [ -n "${STD_TARGETS}" -o -n "${OPT_TARGETS}" ] ; then\
		${KDEVINSTALL} ${NONALTTARGETS};\
	fi
	${KDEVINSTALL} $(TPI_OBJS) $(IMON_NAME) $(KBD_NAME) \
		$(TPSC_NAME) $(SMFD_NAME)

