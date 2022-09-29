#!smake
BASEVERSION=n32bit
ALTVERSIONS=64bit
include	$(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=autopush
GLDOPTS=

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall n32bitinstall: default
	$(INSTALL) $(I_FLAGS) -F /sbin $(IDB_TAG32) $(COMMANDS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(COMMANDS) $(COMMANDS)

64bitinstall: default
	$(INSTALL) $(I_FLAGS) -F /sbin $(IDB_TAG64) $(COMMANDS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(COMMANDS) $(COMMANDS)
