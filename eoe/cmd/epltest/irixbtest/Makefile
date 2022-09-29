#!smake
#
# Makefile for irixbtest(1).
#

include	$(ROOT)/usr/include/make/commondefs

CVERSION=-cckr

# To remove (smake clean) unwanted file and directories
LDIRT = ./ibt*

# Optimization or debugging?
#OPT=-g
OPT=-O

# flags for normal, shared version
LLDLIBS = -nostdlib -L$(ROOT)/lib -L$(ROOT)/usr/lib
LCOPTS= $(OPT)

CFILES =errormsg.c irixbtest.c queuetests.c menus.c bsdgetpgrp_dac.c \
	bsdsetpgrp_dac.c setpgid_dac.c setpgid_mac.c \
	rdname_dac.c kill_dac.c chown_dac.c \
	setreuid_proc.c setregid_proc.c setpgrp_proc.c setsid_proc.c \
	setgroups_sup.c bsdsetgroups_sup.c bsdsetpgrp_proc.c \
	getlabel_dac.c setplabel_proc.c openr_mac.c \
	openw_mac.c access_mac.c chmod_mac.c exece_mac.c \
	bsdgetpgrp_mac.c kill_mac.c sproc_obr.c \
	bsdsetpgrp_mac.c msgget_mac.c msgctl_mac.c \
	getlabel_mac.c setlabel_mac.c getlabel_lbl.c setlabel_lbl.c \
	getplabel_lbl.c setplabel_lbl.c mac_dom_mls.c mac_equ_mls.c \
	socket_lbl.c socket_ioctl_lbl.c agent_mac.c \
	transport_mac.c blockproc_mac.c blockproc_dac.c \
	brk_obr.c creat_obr.c open_obr.c mkdir_obr.c mknod_obr.c \
	pipe_obr.c symlink_mac.c symlink_obr.c msgget_obr.c fork_obr.c \
	semget_obr.c shmget_obr.c socket_obr.c socketpair_obr.c cap.c


OBSOLETE =	agent_dac.c \
		procsz_dac.c procsz_mac.c \
		ptracer_dac.c ptracer_mac.c ptracew_dac.c ptracew_mac.c \
		rdublk_dac.c rdublk_mac.c \
		send_lbl.c socket_ioctl_ifuid.c socket_ioctl_soacl.c \
		socket_ioctl_souid.c socket_soacl.c socket_ioctl_iflabel.c 

HFILES = dac.h lbl.h mls.h soc.h irixbtest.h mac.h queuetests.h cap.h \
	config_bits.h debug.h
MFILES = Makefile
SOURCES =$(CFILES) $(HFILES) $(MFILES)

OBJECTS = $(CFILES:.c=.o)
IDB_TAG = -idb "mac(dblow) noship.sw.epltest"
TARGETS = irixbtest

default: $(TARGETS)
_default: default

include $(COMMONRULES)

irixbtest: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
#	$(CCF) -o $@ $(OBJECTS)

install: default
	$(INSTALL) $(IDB_TAG) -dir -u root -m 777 /usr/epltest
	$(INSTALL) $(IDB_TAG) -dir -u root -m 777 /usr/epltest/bin
	$(INSTALL) -F /usr/epltest/bin $(IDB_TAG) $(TARGETS)
	$(INSTALL) $(IDB_TAG) -dir -u root -m 777 /usr/epltest/src
	$(INSTALL) $(IDB_TAG) -dir -u root -m 777 /usr/epltest/src/irixbtest
	$(INSTALL) -F /usr/epltest/bin/src/irixbtest $(IDB_TAG) $(SOURCES)

# DO NOT DELETE

