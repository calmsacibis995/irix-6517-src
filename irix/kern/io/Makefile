#!smake
# Copyright 1986, Silicon Graphics Inc., Mountain View, CA. 
#
# Master makefile for device driver subsystems.
#
#ident	"$Revision: 3.259 $"

DEPTH?=..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io
.PATH:$(KPATH)
SUBDIRS=vme streams tpi lio dmi xlv graph dlpi grio

KDIRT=$(GENERATED_READMES)

#ifdef CPUBOARD
#if $(CPUBOARD) == "IP27"
SUBDIRS+=rad1
#endif
#endif /* CPUBOARD */

#
# Add machine specific Fibrechannel SUBDIRS
#
#ifdef CPUBOARD
#if $(CPUBOARD) == "IP27" || $(CPUBOARD) == "IP30" || $(CPUBOARD) == "IP33"
SUBDIRS+=adpfc
#endif
#endif /* CPUBOARD */

# don't want -fullwarn complaints about anonymous functions (i.e., calls
# through function pointers) without prototypes.
#if defined(VCC) && $(VCC) == "CFE"
KWOFF=,835
#elif $(VCC) == "MONGOOSE" && $(CPUBOARD) == "IP32"
# avoid bug(?) in 7.1 compiler.  See incident #479469 -wsm4/11/97.
KWOFF=,1413
#endif

#
# Generic io library
#
IO_NAME=io.a
IO_CSRCS=alenlist.c ddi.c devsupport.c disksub.c dma.c sgset.c lan.c eisa.c \
	vme.c vdma.c \
	dbg.c idbgentry.c idbgactlog.c
IO_ASRCS=hwcopy.s
IO_OBJS=${IO_CSRCS:.c=.o} ${IO_ASRCS:.s=.o} 
$(IO_NAME):$(IO_NAME)($(IO_OBJS)) MAKELIB

#
# alp
#
ALP_NAME=alp.o
ALP_CSRCS=alp.c

#
# connld
#
CONNLD_NAME=connld.o
CONNLD_CSRCS=connld.c

#
# /dev/tty
#
GENTTY_NAME=gentty.o
GENTTY_CSRCS=gentty.c

#
# SYSV Messages
#
MSG_NAME=msg.o
MSG_CSRCS=msg.c

#
# SYSV Semaphores
#
SEM_NAME=sem.o
SEM_CSRCS=sem.c

#
# SYSV IPC
#
IPC_NAME=ipc.o
IPC_CSRCS=ipc.c

#
# SYSV IPC MAC
#
MAC_IPC_NAME=mac_ipc.o
MAC_IPC_CSRCS=mac_ipc.c

#
# /dev/zero
#
ZERO_NAME=zero.o
ZERO_CSRCS=zero.c

#
# /dev/{vme,eisa}
#
UBUS_NAME=usrbus.o
UBUS_CSRCS=usrbus.c

#
# /dev/vme/dma*
#
UDMA_NAME=usrdma.o
UDMA_CSRCS=usrdma.c

#
# Connection/Driver List support
#
CDL_NAME=cdl.o
CDL_CSRCS=cdl.c

#
# Old PCI infrastructure
#
PCI_NAME=pci.o pci_intf.o
PCI_CSRCS=pci.c pci_intf.c

#
# XTalk/PCI Bridge ASIC
#
PCIBR_NAME=pcibr.o
PCIBR_CSRCS=pcibr.c

#
#
# Moosehead PCI Bridge ASIC
#
PCIMH_NAME=pcimh.o
PCIMH_CSRCS=pcimh.c

# PCI infrastructure
#
PCIIO_NAME=pciio.o
PCIIO_CSRCS=pciio.c

#
# PCI Shoebox
#
PCIIOX_NAME=pciiox.o
PCIIOX_CSRCS=pciiox.c

#
# Generic usrpci interface
#
USRPCI_NAME=usrpci.o
USRPCI_CSRCS=usrpci.c

#
# GIO-mode Bridge ASIC
#
GIOBR_NAME=giobr.o
GIOBR_CSRCS=giobr.c

#
# XTalk generic GIO infrastructure
#
GIOIO_NAME=gioio.o
GIOIO_CSRCS=gioio.c

#
# /dev/ccsync
#
CCSYNC_NAME=ccsync.o
CCSYNC_CSRCS=ccsync.c

#
# Streams clone device
#
CLONE_NAME=clone.o
CLONE_CSRCS=clone.c

#
# /dev/kmem
#
MEM_NAME=mem.o
MEM_CSRCS=mem.c

#
# Kernel Logging
#
KLOG_NAME=klog.o
KLOG_CSRCS=klog.c

#
# Logging
#
LOG_NAME=log.o
LOG_CSRCS=log.c

#
# RAM disk
#
RAM_NAME=ram.o
RAM_CSRCS=ram.c


#
# User semaphore device
#
USEMA_NAME=usema.o
USEMA_CSRCS=usema.c

#
# usync module
#
USYNC_NAME=usync.o
USYNC_CSRCS=usync.c

#
# Posix semaphore module
#
PSEMA_NAME=psema.o
PSEMA_CSRCS=psema.c


#
# SN0 "Craylink" devices
#
CL_NAME=cl.o
CL_CSRCS=cl.c

#
# SN0 "Craylink shared-memory" devices
#
CL_SHM_NAME=clshm.o
CL_SHM_CSRCS=clshm.c

#
# Oracle postwait device
#
#if defined(CPUBOARD) && ($(CPUBOARD) == "IP19" || $(CPUBOARD) == "IP25" || $(CPUBOARD) == "IP27") || $(CPUBOARD) == "IP33"
PW_NAME=postwait.o
PW_CSRCS=postwait.c
#endif

#if defined(CPUBOARD) && ($(CPUBOARD) == "IP19" || $(CPUBOARD) == "IP25" || $(CPUBOARD) == "IP27") || $(CPUBOARD) == "IP33"
KAIO_NAME=kaio.o
KAIO_CSRCS=kaio.c
#endif


#
# File reorganizer
#
FSCTL_NAME=fsctl.o
FSCTL_CSRCS=fsctl.c

#
# Ptys
#
PTC_NAME=ptc.a
PTC_CSRCS=spty.c
PTC_OBJS=${PTC_CSRCS:.c=.o}
$(PTC_NAME):$(PTC_NAME)($(PTC_OBJS)) MAKELIB
PTY_ALTS=pts.a
$(PTY_ALTS):$(PTC_NAME) MAKEALTERNATE

#
# Console
#
CONSOLE_NAME=cn.o
CONSOLE_CSRCS=cn.c


#
# Standard serialio upper modules (serialio.c is
# included in sduart.a)
#
CSERIALIO_NAME=cserialio.o
CSERIALIO_CSRCS=cserialio.c
USERIALIO_NAME=userialio.o
USERIALIO_CSRCS=userialio.c
TSERIALIO_NAME=tserialio.o
TSERIALIO_CSRCS=tserialio.c

# EVEREST kbd mouse driver
EV_KBMS_NAME = ev_kbdms.o
EV_KBMS_CSRCS = ev_kbdms.c

#
# On-board Duart
#
#ifdef CPUBOARD
DUART_NAME=sduart.a
#if $(CPUBOARD) == "IP19" || $(CPUBOARD) == "IP21" || $(CPUBOARD) == "IP25"
DUART_CSRCS=serialio.c sio_8530.c
#elif $(CPUBOARD) == "IP20" || $(CPUBOARD) == "IP22" || $(CPUBOARD) == "IP26" || $(CPUBOARD) == "IP28"|| $(CPUBOARD) == "IPMHSIM"
DUART_CSRCS=zduart.c
#elif $(CPUBOARD) == "IP27"
#if $(SABLE) == "1"
DUART_CSRCS=hubuart.c
#else
#if $(IOC3_PIO_MODE) == "1"
DUART_CSRCS=serialio.c sio_16550.c 
#else
DUART_CSRCS=serialio.c sio_ioc3.c 
#endif
#endif
#elif $(CPUBOARD) == "IP30"
#if $(IOC3_PIO_MODE) == "1"
DUART_CSRCS=serialio.c sio_16550.c
#else
DUART_CSRCS=serialio.c sio_ioc3.c
#endif
#elif $(CPUBOARD) == "IP32"
DUART_CSRCS=serialio.c sio_ti16550.c
#elif $(CPUBOARD) == "IP32SIM"
DUART_CSRCS=uart16550.c
#elif $(CPUBOARD) == "IP33"
#if $(SABLE) == "1"
DUART_CSRCS=hubuart.c
#else
#if $(IOC3_PIO_MODE) == "1"
DUART_CSRCS=serialio.c sio_16550.c 
#else
DUART_CSRCS=serialio.c sio_ioc3.c 
#endif /* IOC3_PIO_MODE */
#endif /* SABLE */
#endif
DUART_OBJS=${DUART_CSRCS:.c=.o}
$(DUART_NAME):$(DUART_NAME)($(DUART_OBJS)) MAKELIB
#endif /* CPUBOARD */

#
# stty_ld Line Discipline
#
STTY_NAME=stty_ld.o
STTY_CSRCS=stty_ld.c

#
# ldterm Line Discipline (actually an alias for stty_ld)
#
LDTERM_NAME=ldterm.o
LDTERM_CSRCS=ldterm.c

#
# PTEM (Pseudo-Tty Emulation) module
#
PTEM_NAME=ptem.o
PTEM_CSRCS=ptem.c

#
# Generic Module To Flush STREAMS-based pipes and FIFOs.
#
PIPEMOD_NAME=pipemod.o
PIPEMOD_CSRCS=pipemod.c

#
# Autopush driver
#
SAD_NAME=sad.o
SAD_CSRCS=sad.c

#
# PPP framer
#
PPP_FRAME_NAME=ppp_fram.o
PPP_FRAME_CSRCS=ppp_fram.c

#
# PI Parallel Port
#
MCIOPLP_NAME=mcioplp.o
MCIOPLP_CSRCS=mcioplp.c

#
# Everest IO4 EPC Parallel Port
#
EPCPLP_NAME=epcplp.o
EPCPLP_CSRCS=epcplp.c

#
# Everest IO4 EPC External Interrupts
#
EPCEI_NAME=epcei.o
EPCEI_CSRCS=epcei.c

#
# Everest System Controller Driver
#
SYSCTLR_NAME=sysctlr.o
SYSCTLR_CSRCS=sysctlr.c

#
# Flashprom driver
#
#if $(CPUBOARD) == "IP30"
FLASH_NAME=sflash.o
FLASH_CSRCS=sflash.c
#elif CPUBOARD
FLASH_NAME=flash.o
FLASH_CSRCS=flash.c
#endif

#
# Generic SCSI
#
SCSI_NAME=scsi.o failover.o
SCSI_CSRCS=scsi.c failover.c

#
# Tape utility routines
#
TPSCUTIL_NAME=tpscutil.o
TPSCUTIL_CSRCS=tpscutil.c

#
# TPU driver
#
TPU_NAME=tpu.o tpusim.o
TPU_CSRCS=tpu.c tpusim.c
TPUDBG_NAME=tpuidbg.o
TPUDBG_CSRCS=tpuidbg.c

#
# Western Digital WD93 SCSI controller
#
WD93_NAME=wd93.o
WD93_CSRCS=wd93.c

#
# Western Digital WD95A SCSI controller
#
WD95_NAME=wd95.a
WD95_CSRCS=wd95.c wd95a_wcs.c
WD95_OBJS=${WD95_CSRCS:.c=.o}
$(WD95_NAME):$(WD95_NAME)($(WD95_OBJS)) MAKELIB

#
# Adaptec 7800 family of SCSI controllers (7880 single channel,
# 7890 (excalibur) dual channel)
#
ADP78_NAME=adp78.o
ADP78_CSRCS=adp78.c
ADP78DBG_NAME=adpidbg.o
ADP78DBG_CSRCS=adpidbg.c

# Mace driver for ithreading of MACE ISA drivers
#
MACE_NAME=mace.o
MACE_CSRCS=mace.c

#
# SCIP controller
#
SCIP_NAME=scip.o
SCIP_CSRCS=scip.c

#
# Adaptec PCI Fibrechannel controller
#
FCADP_NAME=fcadp.a
FCADP_CSRCS=fcadp.c
FCADP_OBJS=${FCADP_CSRCS:.c=.o}
$(FCADP_NAME):$(FCADP_NAME)($(FCADP_OBJS)) MAKELIB

FCADPDBG_NAME=fcidbg.o
FCADPDBG_CSRCS=fcidbg.c

#
# QLogic PCI SCSI controllers
#
QL_NAME=ql.o
QL_CSRCS=ql.c
QLDBG_NAME=qlidbg.o
QLDBG_CSRCS=qlidbg.c

#
# qLogic PCI Fibre Channel controllers
#

#QLFC_NAME=qlfc.o ql2100_fw.o ql2200_fw.o
QLFC_NAME=qlfc.a
QLFC_CSRCS=qlfc.c ql2100_fw.c ql2200_fw.c
QLFC_DBG_NAME=qlfc_idbg.o
QLFC_DBG_CSRCS=qlfc_idbg.c
QLFC_OBJS=${QLFC_CSRCS:.c=.o}
$(QLFC_NAME):$(QLFC_NAME)($(QLFC_OBJS)) MAKELIB

#
# SCSI Disk
#
DKSC_NAME=dksc.o
DKSC_CSRCS=dksc.c

#
# bpqueue
#
BPQUEUE_NAME=bpqueue.o
BPQUEUE_CSRCS=bpqueue.c

#
# User level SCSI Interface
#
DS_NAME=ds.a
DS_CSRCS=dsreq.c dsglue.c
DS_OBJS=${DS_CSRCS:.c=.o}
$(DS_NAME):$(DS_NAME)($(DS_OBJS)) MAKELIB

#
# SCSI host adapter operations (parallel and Fibrechannel)
#
SCSIHA_NAME=scsiha.o
SCSIHA_CSRCS=scsiha.c

#
# Internal Kernel Debugger
#
IDBG_NAME=idbg.o
IDBG_CSRCS=idbg.c

#
# Profiler
#
PRF_NAME=prf.o
PRF_CSRCS=prf.c

#
# VME 8 Port Serial
#
CDSIO_NAME=cdsio.o
CDSIO_CSRCS=cdsio.c

#
# VME user level interrupt support
#
VMEULI_NAME=vmeuli.o

#
# Kernel Telnetd (Telnet Streams Driver)
#
TSD_NAME=tsd.o
TSD_CSRCS=tsd.c

#
# PC keyboard and mouse driver -- some flavor for all newer systems.
#
#if defined(CPUBOARD) && ($(CPUBOARD) == "IP32" || $(CPUBOARD) == "IP32SIM")
PCKM_NAME=mhpckm.o
PCKM_CSRCS=mhpckm.c
#elif ($(CPUBOARD) == "IP30" || $(CPUBOARD) == "IP27")  || $(CPUBOARD) == "IP33"
PCKM_NAME=ioc3_pckm.o
PCKM_CSRCS=ioc3_pckm.c
#else
PCKM_NAME=pckm.o
PCKM_CSRCS=pckm.c
#endif

#
# SD sable disk (fake disk controller)
#
SD_NAME=sabledsk.a
SD_CSRCS=sabledsk.c
SD_OBJS=${SD_CSRCS:.c=.o}
$(SD_NAME):$(SD_NAME)($(SD_OBJS)) MAKELIB

#
# xtalk IO generic support module
#
XTALK_NAME=xbow.o xbmon.o xswitch.o xtalk.o
XTALK_CSRCS=xbow.c xbmon.c xswitch.c xtalk.c

#
# IP30 hooks to XTALK infrastructure
#
HEART_NAME=heart.o
HEART_CSRCS=heart.c

#
# xsamp "sample xtalk client device"
#
XSAMP_NAME=sample_xtalk.o
XSAMP_CSRCS=sample_xtalk.c

#
# Drivers for various xtalk cards
#
XCARDS_NAME=menet.o io6.o mio.o
XCARDS_CSRCS=menet.c io6.c mio.c

#
# psamp "sample pciio client device"
#
PSAMP_NAME=sample_pciio.o
PSAMP_CSRCS=sample_pciio.c

#
# gsamp "sample gioio client device"
#
GSAMP_NAME=sample_gioio.o
GSAMP_CSRCS=sample_gioio.c

#
# kitchen_sync driver
#
KITCHEN_SYNC_NAME=kitchen_sync.o
KITCHEN_SYNC_CSRC=kitchen_sync.c


#
# IOC3 PCI device
#
IOC3_NAME=ioc3.o
IOC3EI_NAME=ioc3ei.o
IOC3_CSRCS=ioc3.c
IOC3EI_CSRCS=ioc3ei.c


#
# ELSC support
#
ELSC_NAME= i2c.o elsc.o sio_i2c.o
ELSC_CSRCS= i2c.c elsc.c sio_i2c.c

#
# kernel nic interface
#
NIC_NAME=nic.o
NIC_CSRCS=nic.c

#
# /dev/hubspc
#
HUBSPC_NAME=hubspc.o
HUBSPC_CSRCS=hubspc.c

#
# Sharena interface
#
SHA_NAME=sharena.o
SHA_CSRCS=sharena.c

#
# README files that get installed
#

GENERATED_READMES=$(KPATH)/Makefile.static $(KPATH)/Makefile.loadable


#
# Specify system specific modules
# Split up into 2 groups - those shipped with the system STD_TARGETS
# and those that are seperate products OPT_TARGETS
# Each of these groups is split into base and alternate names:
# {STD,OPT}_ALTTARGETS
#
IP19INST=\#
IP20INST=\#
IP21INST=\#
IP22INST=\#
IP25INST=\#
IP26INST=\#
IP27INST=\#
IP28INST=\#
IP30INST=\#
IP32INST=\#
IP33INST=\#

#ifdef CPUBOARD

#if $(CPUBOARD) == "IP19"
STD_TARGETS= $(EPCPLP_NAME) $(EPCEI_NAME) $(SYSCTLR_NAME) $(FLASH_NAME) \
	$(WD95_NAME) $(SCIP_NAME) $(CCSYNC_NAME) $(KAIO_NAME) \
	$(PW_NAME) $(CSERIALIO_NAME) $(TSERIALIO_NAME) $(EV_KBMS_NAME)
OPT_TARGETS= $(SD_NAME)
OPT_ALTTARGETS=
VME_TARGETS = $(CDSIO_NAME) $(UBUS_NAME) $(UDMA_NAME) $(VMEULI_NAME) 
IP19INST=
#endif

#if $(CPUBOARD) == "IP25"
STD_TARGETS= $(EPCPLP_NAME) $(EPCEI_NAME) $(SYSCTLR_NAME) $(FLASH_NAME) \
	$(WD95_NAME) $(SCIP_NAME) $(CCSYNC_NAME) $(KAIO_NAME) \
	$(PW_NAME) $(CSERIALIO_NAME) $(TSERIALIO_NAME) $(EV_KBMS_NAME)
OPT_TARGETS=$(SD_NAME)
OPT_ALTTARGETS=
VME_TARGETS = $(CDSIO_NAME) $(UBUS_NAME) $(UDMA_NAME) $(VMEULI_NAME)
IP25INST=
#endif

#if $(CPUBOARD) == "IP20"
STD_TARGETS=$(HDSP_NAME) $(WD93_NAME)
IP20INST=
#endif

#if $(CPUBOARD) == "IP21"
STD_TARGETS=$(EPCPLP_NAME) $(EPCEI_NAME) $(SYSCTLR_NAME) $(FLASH_NAME) \
	$(WD95_NAME) $(SD_NAME) $(CCSYNC_NAME) $(SCIP_NAME) \
	$(CSERIALIO_NAME) $(TSERIALIO_NAME) $(EV_KBMS_NAME)
OPT_TARGETS=
OPT_ALTTARGETS=
VME_TARGETS =$(CDSIO_NAME) $(UBUS_NAME) $(UDMA_NAME) $(VMEULI_NAME)
IP21INST=
#endif

#if $(CPUBOARD) == "IP22"
STD_TARGETS=$(PCKM_NAME) $(WD93_NAME) $(UBUS_NAME) $(WD95_NAME)
OPT_TARGETS=
#OPT_TARGETS= $(QL_NAME) $(PCI_NAME)
IP22INST=
#endif

#if $(CPUBOARD) == "IP26"
STD_TARGETS=$(PCKM_NAME) $(WD93_NAME) $(UBUS_NAME)
IP26INST=
#endif

#if $(CPUBOARD) == "IP27"
STD_TARGETS= $(QL_NAME) $(QLDBG_NAME) \
	     $(QLFC_NAME) $(QLFC_DBG_NAME) $(XTALK_NAME) $(PCKM_NAME) $(IOC3_NAME)	$(IOC3EI_NAME) \
	     $(CSERIALIO_NAME) $(TSERIALIO_NAME) $(USERIALIO_NAME) \
	     $(PCIIO_NAME) $(USRPCI_NAME)	\
	     $(NIC_NAME) $(PCIBR_NAME) $(PCIIOX_NAME) $(HUBSPC_NAME) \
	     $(CL_NAME) $(XCARDS_NAME) $(ELSC_NAME) $(FCADP_NAME) $(FCADPDBG_NAME) \
	     $(KAIO_NAME) $(KITCHEN_SYNC_NAME) $(PW_NAME) \
	     $(TPU_NAME) $(TPUDBG_NAME) \
	     $(CL_SHM_NAME)

OPT_TARGETS= 
VME_TARGETS= $(UBUS_NAME) $(UDMA_NAME) $(VMEULI_NAME)
IP27INST=
#if $(SABLE) == "1"
STD_TARGETS+=$(SD_NAME)
#endif
#endif

#if $(CPUBOARD) == "IP28"
STD_TARGETS=$(PCKM_NAME) $(WD93_NAME) $(UBUS_NAME)
IP28INST=
#endif

#if $(CPUBOARD) == "IP30"
STD_TARGETS=$(PCKM_NAME) $(UBUS_NAME) $(QL_NAME) $(QLDBG_NAME) \
	$(QLFC_NAME) $(QLFC_DBG_NAME) $(IOC3_NAME) $(XTALK_NAME) \
	$(PCIIO_NAME) $(PCIBR_NAME) $(PCIIOX_NAME) \
	$(HEART_NAME) $(GIOIO_NAME) $(GIOBR_NAME) $(NIC_NAME) $(USRPCI_NAME) \
	$(CSERIALIO_NAME) $(TSERIALIO_NAME) $(USERIALIO_NAME) \
	$(KITCHEN_SYNC_NAME) $(FLASH_NAME) \
	$(TPU_NAME) $(TPUDBG_NAME) \
	$(FCADP_NAME) $(FCADPDBG_NAME)
OPT_TARGETS= $(GSAMP_NAME) $(XSAMP_NAME) $(PSAMP_NAME) \
	$(XCARDS_NAME)
IP30INST=
#endif

#if $(CPUBOARD) == "IPMHSIM"
STD_TARGETS=$(SD_NAME)
OPT_TARGETS=
IPMHSIMINST=
#endif

#if $(CPUBOARD) == "IP32"
STD_TARGETS=$(PCIIO_NAME) $(PCIMH_NAME) $(SD_NAME) $(ADP78_NAME) \
	$(MACE_NAME) $(PCKM_NAME) $(TSERIALIO_NAME) $(ADP78DBG_NAME) \
	$(QLFC_NAME) $(QLFC_DBG_NAME) 
OPT_TARGETS=
SUBDIRS+=adphim
IP32INST=
#endif

#if $(CPUBOARD) == "IP32SIM"
STD_TARGETS=$(PCI_NAME) $(SD_NAME) $(PCKM_NAME)
OPT_TARGETS=
IP32INST=
#endif

#if $(CPUBOARD) == "IP33"
STD_TARGETS= $(QL_NAME) $(QLDBG_NAME) \
	$(QLFC_NAME) $(QLFC_DBG_NAME) $(XTALK_NAME) $(PCKM_NAME) $(IOC3_NAME)	\
	     $(SERIALIO_NAME) $(PCIIO_NAME) $(USRPCI_NAME)	\
	     $(NIC_NAME) $(PCIBR_NAME) $(PCIIOX_NAME)  \
	     $(XCARDS_NAME) $(FCADP_NAME) $(FCADPDBG_NAME) $(KAIO_NAME) \
	     $(KITCHEN_SYNC_NAME) $(PW_NAME) 

OPT_TARGETS= 
VME_TARGETS= $(UBUS_NAME) $(UDMA_NAME) $(VMEULI_NAME)
IP33INST=
#if $(SABLE) == "1"
STD_TARGETS+=$(SD_NAME)
#endif
#endif

#endif /* CPUBOARD */

#
# For installation reasons, split TARGETS up into real and alternate names
#
NONALTTARGETS=$(IO_NAME) $(DDI_NAME) \
	$(GENTTY_NAME) $(MSG_NAME) $(SEM_NAME) $(IPC_NAME) \
        $(ALP_NAME) $(CONNLD_NAME) \
	$(MAC_IPC_NAME) \
	$(ZERO_NAME) $(CLONE_NAME) \
	$(KLOG_NAME) $(LV_NAME) $(USEMA_NAME) $(FSCTL_NAME) $(MEM_NAME)\
	$(LOG_NAME) $(USYNC_NAME) $(PSEMA_NAME) $(HEART_BEAT_NAME)\
	$(PTC_NAME) \
	$(CONSOLE_NAME) $(DUART_NAME) $(STTY_NAME)\
	$(LDTERM_NAME) \
	$(PTEM_NAME) \
	$(PIPEMOD_NAME) \
	$(SAD_NAME)\
	$(PPP_FRAME_NAME) \
	$(CDL_NAME) $(SCSI_NAME) $(TPSCUTIL_NAME) $(DKSC_NAME) $(BPQUEUE_NAME) \
	$(SMFD_NAME) $(DS_NAME) $(SCSIHA_NAME) \
	$(IDBG_NAME) $(PRF_NAME) $(DATAPIPE_NAME)\
	$(TSD_NAME) \
	$(SHA_NAME) \
	$(STD_TARGETS) $(OPT_TARGETS)

ALTTARGETS=$(PTY_ALTS) $(STD_ALTTARGETS) $(OPT_ALTTARGETS)

TARGETS=$(NONALTTARGETS) $(ALTTARGETS) $(VME_TARGETS)

# lint
LINTNAME=io
CFILES= \
	alp.c \
	adp78.c \
	bpqueue.c \
	ccsync.c \
	cdsio.c \
	clone.c \
	cn.c \
	ddi.c \
	disksub.c \
	dksc.c \
	dma.c \
	dsglue.c \
	dsreq.c \
	eisa.c \
	fsctl.c \
	gentty.c \
	idbg.c \
	ikc.c \
	ipc.c \
	klog.c \
	log.c \
	lan.c \
	mac_ipc.c \
	mem.c \
	msg.c \
	pci_intf.c \
	ppp_fram.c \
	prf.c \
	ram.c \
	rd.c \
	sad.c \
	scsi.c \
	sem.c \
	sgset.c \
	sharena.c \
	smfd.c \
	spty.c \
	stty_ld.c \
	tsd.c \
	usema.c \
	usrvme.c \
	vdma.c \
	vme.c \
	zduart.c \
	zero.c

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS) $(GENERATED_READMES)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} $(IO_NAME) \
		$(GENTTY_NAME) $(MSG_NAME) $(SEM_NAME) $(IPC_NAME) \
		$(MAC_IPC_NAME) \
		$(ZERO_NAME) $(CLONE_NAME) \
		$(KLOG_NAME) $(LV_NAME) $(USEMA_NAME) $(FSCTL_NAME) $(MEM_NAME)\
		$(LOG_NAME) $(ALP_NAME) $(CONNLD_NAME)  $(HEART_BEAT_NAME)\
		$(PTC_NAME) $(CONSOLE_NAME) $(DUART_NAME) $(STTY_NAME)\
		$(LDTERM_NAME)  $(PTEM_NAME) \
		$(PIPEMOD_NAME) $(SAD_NAME)\
		$(CDL_NAME) $(SCSI_NAME) $(TPSCUTIL_NAME) $(DKSC_NAME) $(BPQUEUE_NAME) \
		$(SMFD_NAME) $(DS_NAME) $(SCSIHA_NAME) \
		$(DATAPIPE_NAME) $(SHA_NAME) \
		$(TSD_NAME) $(USYNC_NAME) $(PSEMA_NAME)\
		$(STD_TARGETS)
	${KINSTALL} -idb std.sw.debug $(IDBG_NAME)
	${KINSTALL} -idb std.sw.perf $(PRF_NAME)
	${KINSTALL} -idb std.sw.ppp $(PPP_FRAME_NAME)
	$(KNOMODEINSTALL) -lns ptc.a $(PTY_ALTS)
	@#
	@# Machine/system specific installs here
	@#
	$(IP19INST) ${KINSTALL} $(WD95_NAME)
	$(IP19INST) ${KINSTALL} $(SCIP_NAME)
	$(IP19INST) ${KINSTALL} $(SD_NAME)
	$(IP21INST) ${KINSTALL} $(WD95_NAME)
	$(IP21INST) ${KINSTALL} $(SCIP_NAME)
	$(IP21INST) ${KINSTALL} $(SD_NAME)
	$(IP25INST) ${KINSTALL} $(WD95_NAME)
	$(IP25INST) ${KINSTALL} $(SCIP_NAME)
	$(IP25INST) ${KINSTALL} $(SD_NAME)
	$(IP20INST) ${KINSTALL} $(WD93_NAME)
	$(IP22INST) ${KINSTALL} $(WD93_NAME)
	$(IP22INST) ${KINSTALL} $(WD95_NAME)
	$(IP26INST) ${KINSTALL} $(WD93_NAME)
	$(IP27INST) $(KINSTALL) $(CL_SHM_NAME)
	$(IP27INST) $(KINSTALL) $(NIC_NAME)
	$(IP27INST) $(KINSTALL) $(PCIBR_NAME)
	$(IP27INST) $(KINSTALL) $(PCIIOX_NAME)
	$(IP27INST) $(KINSTALL) $(QL_NAME)
	$(IP27INST) $(KINSTALL) $(QLDBG_NAME)
	$(IP27INST) $(KINSTALL) $(QLFC_NAME)
	$(IP27INST) $(KINSTALL) $(QLFC_DBG_NAME)
	$(IP27INST) ${KINSTALL} $(FCADP_NAME)
	$(IP27INST) ${KINSTALL} $(FCADPDBG_NAME)
	$(IP27INST) ${KINSTALL} $(TPU_NAME)
	$(IP27INST) ${KINSTALL} $(TPUDBG_NAME)
	$(IP27INST) ${KINSTALL} $(XCARDS_NAME)
	$(IP28INST) ${KINSTALL} $(WD93_NAME)
	$(IP30INST) ${KINSTALL} $(FCADP_NAME)
	$(IP30INST) ${KINSTALL} $(FCADPDBG_NAME)
	$(IP30INST) ${KINSTALL} $(XCARDS_NAME)
	$(IP30INST) ${KINSTALL} $(TPU_NAME)
	$(IP30INST) ${KINSTALL} $(TPUDBG_NAME)
	$(IP32INST) $(KINSTALL) $(ADP78DBG_NAME)
	$(IP32INST) $(KINSTALL) $(QLFC_NAME)
	$(IP32INST) $(KINSTALL) $(QLFC_DBG_NAME)
	$(IP33INST) $(KINSTALL) $(NIC_NAME)
	$(IP33INST) $(KINSTALL) $(PCIBR_NAME)
	$(IP33INST) $(KINSTALL) $(PCIIOX_NAME)
	$(IP33INST) $(KINSTALL) $(QL_NAME)
	$(IP33INST) $(KINSTALL) $(QLDBG_NAME)
	$(IP33INST) $(KINSTALL) $(QLFC_NAME)
	$(IP33INST) $(KINSTALL) $(QLFC_DBG_NAME)
	$(IP33INST) ${KINSTALL} $(FCADP_NAME)
	$(IP33INST) ${KINSTALL} $(FCADPDBG_NAME)
	$(IP33INST) ${KINSTALL} $(XCARDS_NAME)

	@#
	@# VME stuff; somewhat ugly since we have to repeat...
	@#
	$(IP19INST) ${KINSTALL} -idb std.sw.cdsio $(CDSIO_NAME)
	$(IP19INST) ${KINSTALL} $(UBUS_NAME)
	$(IP19INST) ${KINSTALL} $(UDMA_NAME)
	$(IP19INST) ${KINSTALL} $(CCSYNC_NAME)
	$(IP19INST) ${KINSTALL} $(VMEULI_NAME)
	$(IP19INST) ${KINSTALL} $(KAIO_NAME)
	$(IP19INST) ${KINSTALL} $(PW_NAME)
	@#
	$(IP21INST) ${KINSTALL} -idb std.sw.cdsio $(CDSIO_NAME)
	$(IP21INST) ${KINSTALL} $(UBUS_NAME)
	$(IP21INST) ${KINSTALL} $(UDMA_NAME)
	$(IP21INST) ${KINSTALL} $(CCSYNC_NAME)
	$(IP21INST) ${KINSTALL} $(VMEULI_NAME)
	@#
	$(IP25INST) ${KINSTALL} -idb std.sw.cdsio $(CDSIO_NAME)
	$(IP25INST) ${KINSTALL} $(UBUS_NAME)
	$(IP25INST) ${KINSTALL} $(UDMA_NAME)
	$(IP25INST) ${KINSTALL} $(CCSYNC_NAME)
	$(IP25INST) ${KINSTALL} $(VMEULI_NAME)
	$(IP25INST) ${KINSTALL} $(KAIO_NAME)
	$(IP25INST) ${KINSTALL} $(PW_NAME)
	@#
	$(IP27INST) ${KINSTALL} $(UBUS_NAME)
	$(IP27INST) ${KINSTALL} $(UDMA_NAME)
	$(IP27INST) ${KINSTALL} $(VMEULI_NAME)
	$(IP27INST) ${KINSTALL} $(KAIO_NAME)
	$(IP27INST) ${KINSTALL} $(PW_NAME)
	@#
	$(IP33INST) ${KINSTALL} $(UBUS_NAME)
	$(IP33INST) ${KINSTALL} $(UDMA_NAME)
	$(IP33INST) ${KINSTALL} $(VMEULI_NAME)
	$(IP33INST) ${KINSTALL} $(KAIO_NAME)
	$(IP33INST) ${KINSTALL} $(PW_NAME)
	@#
	@# Install everything for developers
	@#
	${KDEVINSTALL} ${NONALTTARGETS} ${VME_TARGETS}
	$(KNOMODEDEVINSTALL) -lns ptc.a $(PTY_ALTS)


# idbg is special since needs headers and things from all subsystems
# don't optimize idbg, it takes too long... unless we're using sable.

#if $(SABLE) == "1"
IDBG_OPT=
#else
IDBG_OPT=-g
#endif

idbg.o: idbg.c
	$(CCF) -I$(DEPTH)/bsd -c $(IDBG_OPT) \
		$(JALR) -G 0 $(KPATH)/idbg.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)


# Remove the comment lines and select only the appropriate conditional lines.
# Really it would have been easier to just have a single file and explain,
# the difference between loadable and static in the text.  But we didn't...
# [BTW, the install commands for these files are in irix/kern/Makefile.]

$(KPATH)/Makefile.static: $(KPATH)/Makefile.kernio
	sed -e /_REM/d -e /_LOAD/d -e s/_STAT// $(KPATH)/Makefile.kernio >$@

$(KPATH)/Makefile.loadable: $(KPATH)/Makefile.kernio
	sed -e /_REM/d -e /_STAT/d -e s/_LOAD// $(KPATH)/Makefile.kernio >$@
