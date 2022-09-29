#!smake
#
# Make the netstat command
#
# $Revision: 1.44 $

#
# There is still only one source base for netstat ... just
# that this single source base for netstat is compiled in
# several different ways.  
#
# This is because netstat uses NLIST to seek into the kernel 
# for key information.  There are now several different verisons 
# of netstat due to differences in our kernel (ie. elf and 32/64 
# bit'ness) types.
#
# Various netstat binaries are by current working directory 
#	(assumed to be irix/cmd/bsd/netstat) :
#
#	netstat - For 32bit elf kernels (using nlist)
#	64bit/netstat - For 64bit elf kernels (using nlist64)
#	64NS/netstat - For 64bit elf kernels (using nlist64)
#		(nonshared version for mini-root only - to 
#		reduce the size of mini-root, by removing 64bit rld
#		and libc)
#

BASEVERSION=n32bit
ALTVERSIONS=64bit 64NS
WANTPARALLEL=yes-please

LCINCS += -I../../../kern

include	$(ROOT)/usr/include/make/cmdcommondefs

CFILES	=host.c if.c inet.c main.c mbuf.c mroute.c ns.c route.c stream.c unix.c

COMMANDS=netstat
SYSTEMCFILE = system.c
SYSTEMOBJS = r4k.o r4kip19.o tfp.o r10k.o

LCOPTS +=-signed -D_BSD_TIME
GLDOPTS =
LLDLIBS+=-lcurses
I_FLAGS=-m 02555 -u root -g sys
SH_FLAGS=-m 755 -u root -g sys
LDIRT+= $(SYSTEMOBJS)

default: $(TARGETS)

include $(CMDCOMMONRULES)

n32bitinstall: default
	$(INSTALL) $(I_FLAGS) -F /usr/etc $(IDB_TAG32) $(COMMANDS)
	$(INSTALL) $(I_FLAGS) -F /usr/etc -src $(COMMANDS) netstat_32

64bitinstall: default
	$(INSTALL) $(I_FLAGS) -F /usr/etc $(IDB_TAG64) $(COMMANDS)

64NSinstall: default
	$(INSTALL) $(I_FLAGS) -F /usr/etc -src $(COMMANDS) netstat_64
	$(INSTALL) $(SH_FLAGS) -F /usr/etc -src netstat.sh netstat

netstat: $(OBJECTS) $(SYSTEMOBJS)
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@ $(SYSTEMOBJS)

$(SYSTEMOBJS):	system.c
	${CC} ${CFLAGS} -DSYSTEM_$(@:S/.o//) -c -o $@ system.c

