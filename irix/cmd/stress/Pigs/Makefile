#!smake
#
# $Revision: 1.9 $"

BASEVERSION=32bit
ALTVERSIONS=64bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=pig.boing pig.quit duck pig.poly runtests mfgtests
OBJS=printf.o

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) $(IDB_TAG32) -F /usr/stress/Pigs $(COMMANDS)
	$(INSTALL) $(IDB_TAG32) -F /usr/diags/crash/Pigs $(COMMANDS)

64bitinstall: default
	$(INSTALL) $(IDB_TAG64) -F /usr/stress/Pigs $(COMMANDS)
	$(INSTALL) $(IDB_TAG64) -F /usr/diags/crash/Pigs $(COMMANDS)

pig.quit: pig.quit.o printf.o
	$(CCF) -o $@ pig.quit.o printf.o $(LDFLAGS)

pig.poly: pig.poly.o printf.o
	$(CCF) -o $@ pig.poly.o printf.o $(LDFLAGS)
