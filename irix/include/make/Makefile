#
# Makefile to install include/make files
#
# IMPORTANT: Add new productdefs filenames to the DEFS list below.
# Developers get all productdefs files, but customers get none.

include $(ROOT)/usr/include/make/commondefs

INST= $(INSTALL) -F /usr/include/make

DEFS= *defs

#
# for internal use, all productdefs files are shipped as follows
#
NOSHIP_IDBTAG =-idb noship

default: install

include $(COMMONRULES)

headers install: $(_FORCE)
	$(INST) -m 444 $(NOSHIP_IDBTAG) $(DEFS)
	$(INST) $(NOSHIP_IDBTAG) -lns EVERESTdefs	IP19defs
	$(INST) $(NOSHIP_IDBTAG) -lns 4D40defs		IP20defs
	$(INST) $(NOSHIP_IDBTAG) -lns 4DACE1defs	IP22defs
	$(INST) $(NOSHIP_IDBTAG) -lns T5_EVERESTdefs	IP25defs
	$(INST) $(NOSHIP_IDBTAG) -lns TETONdefs		IP26defs
	$(INST) $(NOSHIP_IDBTAG) -lns SN0defs		IP27defs
	$(INST) $(NOSHIP_IDBTAG) -lns T5I2defs		IP28defs
	$(INST) $(NOSHIP_IDBTAG) -lns RACERdefs		IP30defs
	$(INST) $(NOSHIP_IDBTAG) -lns 4DMHdefs		IP32defs

