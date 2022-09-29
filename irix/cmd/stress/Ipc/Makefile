#!smake
#
# $Id: Makefile,v 1.13 1996/12/06 05:26:34 ack Exp $

BASEVERSION=32bit
ALTVERSIONS=64bit n32bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=msg0 msg1 msg2 msgtime sem0 sem1 sem2 sem3 \
	shm0 shm1 shm3 shm4 shm7 runtests mfgtests

# -g is really useful for debugging ... (and -g3 doesn't help much...)
LCOPTS+=-g -fullwarn
OPTIMIZER=
#
# 84 - stop whining about not using anything from libstress.a
LLDOPTS=-Wl,-woff,84
WOFF=

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) -idb "nostrip" $(IDB_TAG32) -F /usr/stress/Ipc '$(COMMANDS)'
	$(INSTALL) -idb "nostrip" $(IDB_TAG32) -F /usr/diags/crash/Ipc '$(COMMANDS)'

64bitinstall: default
	$(INSTALL) -idb "nostrip" $(IDB_TAG64) -F /usr/stress/Ipc '$(COMMANDS)'
	$(INSTALL) -idb "nostrip" $(IDB_TAG64) -F /usr/diags/crash/Ipc '$(COMMANDS)'

n32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/Ipc/n32 $(COMMANDS)
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Ipc/n32 $(COMMANDS)
