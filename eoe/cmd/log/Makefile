#!smake
#
# Make the strerr and strace commands
#
#ident $Revision: 1.3 $

BASEVERSION=32bit
ALTVERSIONS=64bit
WANTPARALLEL=yes-please

include	$(ROOT)/usr/include/make/cmdcommondefs

OPTIMIZER= -g
COMMANDS=strace strerr strclean

CFILES=	strace.c strerr.c strclean.c

LMKDEPFLAGS=${NULLSUFFIX_MKDEPFLAG}
MODEFLAGS=  -m 0100 -u root -g sys

USERBIN  = /usr/bin
USERSBIN = /usr/sbin

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) $(MODEFLAGS) -F $(USERSBIN) $(IDB_TAG32) $(COMMANDS)
	$(INSTALL) -F $(USERBIN) -lns ../sbin/strace strace
	$(INSTALL) -F $(USERBIN) -lns ../sbin/strerr strerr
	$(INSTALL) -F $(USERBIN) -lns ../sbin/strclean strclean

64bitinstall: default
	$(INSTALL) $(MODEFLAGS) -F /usr/sbin $(IDB_TAG64) $(COMMANDS)
	$(INSTALL) -F $(USERBIN) -lns ../sbin/strace strace
	$(INSTALL) -F $(USERBIN) -lns ../sbin/strerr strerr
	$(INSTALL) -F $(USERBIN) -lns ../sbin/strclean strclean
