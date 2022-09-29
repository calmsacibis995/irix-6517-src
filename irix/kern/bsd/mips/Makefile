#!smake
# Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
#
# Makefile for mips-specific network interfaces.
#
# $Revision: 3.135 $

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/mips
.PATH:$(KPATH)

# the list of subdirectories is extended for those products that care
#if $(CPUBOARD) == "IP27" || $(CPUBOARD) == "IP30" || $(CPUBOARD) == "IP32" || $(CPUBOARD) == "IP32SIM"
SUBDIRS=loadable
#else
SUBDIRS=
#endif

# lets get some dates
#
VERS    !=date '+-DVERS_Y=%y -DVERS_M=%m -DVERS_D=%d -DVERS_H=%H -DVERS_MM=%M'\
		| sed -e 's/=0/=/g'

KCINCS	=-I$(TOP)/bsd -I$(TOP)/bsd/misc
KCOPTS	=-common ${VERS}

#
#  IP12 Enet
#
EC2_NAME=if_ec2.o
EC2_CSRCS=if_ec2.c

#
#  IP24 GIO Enet
#
EC2HP1_NAME=if_eg2.o
EC2HP1_CSRCS=if_ec2.c
DEC2HP1=-DIP24_GIO

#
#  IP32 PCI Enet
#
ECF_NAME=if_ecf.o
ECF_CSRCS=if_ecf.c

#
#  IP32 MACE Enet
#
ME_NAME=if_me.o
ME_CSRCS=if_me.c

#
#  Slip
#
SLIP_NAME=if_sl.o
SLIP_CSRCS=if_sl.c

#
#  PPP
#
PPP_NAME=if_ppp.o
PPP_CSRCS=if_ppp.c

# hardware independent interfaces
MI_NAMES=$(SLIP_NAME) $(PPP_NAME)

#
#  FXP VME
#
FXP_NAME=if_fxp.o
FXP_CSRCS=if_fxp.c

#
#  FDDI VME
#
IPG_NAME=if_ipg.o
IPG_CSRCS=if_ipg.c

#
#  FDDI Indigo and Everest
#
XPI_NAME=if_xpi.o
XPI_CSRCS=if_xpi.c

#
#  Lego PCI FDDI
#
RNS_NAME=if_rns.o
RNS_CSRCS=if_rns.c

#
#  TokenRing EISA 
#
MTR_NAME=if_mtr.o
MTR_CSRCS=if_mtr.c

#
#  EVEREST IO4 EPC SEEQ/EDLC Enet
#
EE_NAME=if_ee.o
EE_CSRCS=if_ee.c

#
#  EVEREST 8-port E-Plex Ethernet board
#
EP_NAME=if_ep.o
EP_CSRCS=if_ep.c

#
#  SN0/Speedracer IOC3 10/100 Mbps Enet
#
EF_NAME=if_ef.o 
EF_CSRCS=if_ef.c 

#
#  SN0/Speedracer Gigabit Ethernet
#
EG_NAME=if_eg.o 
EG_CSRCS=if_eg.c

#
# GIO Fast Ethernet (INDY)
#
GFE_NAME=if_gfe.o
GFE_CSRCS=if_gfe.c

#
# VME 10/100 BT Fast Ethernet (IP19/IP21/IP25)
#
VFE_NAME=if_vfe.o
VFE_CSRCS=if_vfe.c

TARGETS=$(MI_NAMES)

#if $(CPUBOARD) == "IP19" || $(CPUBOARD) == "IP21" || $(CPUBOARD) == "IP25"
TARGETS+=$(EE_NAME) $(EP_NAME) $(FXP_NAME) $(IPG_NAME) $(XPI_NAME) $(VFE_NAME)
SUBDIRS+=ipg
#endif
#if $(CPUBOARD) == "IP20"
TARGETS+=$(EC2_NAME) $(XPI_NAME)
#endif
#if $(CPUBOARD) == "IP22"
TARGETS+=$(EC2_NAME) $(EC2HP1_NAME) $(XPI_NAME) $(GFE_NAME)
#endif
#if $(CPUBOARD) == "IP26"
TARGETS+=$(EC2_NAME) $(XPI_NAME)
#endif
#if $(CPUBOARD) == "IP27"
TARGETS+=$(EF_NAME) $(RNS_NAME) $(EG_NAME)
#endif
#if $(CPUBOARD) == "IP28"
TARGETS+=$(EC2_NAME) $(XPI_NAME)
#endif
#if $(CPUBOARD) == "IP30"
TARGETS+=$(EF_NAME) $(RNS_NAME) $(EG_NAME)
#endif
#if $(CPUBOARD) == "IP32" || $(CPUBOARD) == "IP32SIM"
TARGETS+=$(EC2_NAME) $(EC2HP1_NAME) $(ECF_NAME) $(ME_NAME) $(RNS_NAME)
#endif
#if $(CPUBOARD) == "IPMHSIM"
TARGETS+=$(EC2_NAME) $(EC2HP1_NAME) $(XPI_NAME)
#endif

LINTNAME=bsdmips
CFILES=$(EC2_CSRCS) $(ECF_CSRCS) $(SLIP_CSRCS) $(PPP_CSRCS) $(FXP_CSRCS) \
	$(IPG_CSRCS) $(XPI_CSRCS) $(RNS_CSRCS) $(EF_CSRCS) $(ME_CSRCS) \
	$(GFE_CSRCS) $(VFE_CSRCS) $(EG_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${LBOOTPOSTOP} ${TARGETS}
#	$(KINSTALL) $(IDBNOSHIP) $(LBOOTPOSTOP) $(XXX_NAME)
	${KDEVINSTALL} ${TARGETS}

# build IPG firmware
if_ipg.o: $(KPATH)/ipg/ipg.firm

# IP24 GIO Enet
if_eg2.o: $(KPATH)/if_ec2.c
	$(CC) $(CFLAGS) $(DEC2HP1) -c $(KPATH)/if_ec2.c -o if_eg2.o
