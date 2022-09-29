#! make
#
# Makefile for padconf
#
#	"$Revision: 1.7 $"
#
# Include before any of your own definitions.
include $(ROOT)/usr/include/make/commondefs

#
#
LCDEFS  = -DSGI -DMFCURSES
LCINCS=-I..
LIB	= 

CFILES	=   x25file.c lltune.c snidtox25.c
DEF_FILES = def.tokenring def.ether def.fddi

TARGETS	= lltune

# You must define at least one rule before including commonrules.
default all: $(TARGETS)

include $(COMMONRULES)

# Install target installs the Makefile's results.
install: default
	$(INSTALL) -idb DLPI -m 755 -F /usr/etc -u bin -g bin $(TARGETS)
	$(INSTALL) -idb DLPI -m 444 -F /var/opt/snet/template $(DEF_FILES)

# 
# Finally the rule to make $TARGETS of this Makefile.
$(TARGETS): $(OBJECTS)
	$(CCF) $(OBJECTS) $(LIB) -o $(TARGETS) $(LDFLAGS)
