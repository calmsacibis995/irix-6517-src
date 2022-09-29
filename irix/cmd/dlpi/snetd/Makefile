#! make
#
# Makefile for DLPI daemon: snetd
#
#	"$Revision: 1.11 $"
#
# Include before any of your own definitions.
include $(ROOT)/usr/include/make/commondefs

#
#
LCDEFS  = -DSGI -DMFCURSES
LIB	= 

CFILES	= llsnid.c x25snid.c snidtox25.c snid.c frs_snid.c frsctl.c \
          build.c config.c control.c debug.c echo.c function.c \
          graph.c ipnet.c netd.c shell.c streams.c utils.c
LCINCS=-I..

TARGETS	= snetd
CFGDIR  =/etc/config
INSTDIR =/var/opt/snet/instdir
IDB_NUDT = -idb "DLPI config(noupdate)"

# You must define at least one rule before including commonrules.
default all: $(TARGETS)

include $(COMMONRULES)

# Install target installs the Makefile's results.
install: default
	$(INSTALL) -idb DLPI -m 755 -F /usr/etc $(TARGETS)
	$(INSTALL) -idb DLPI -idb "config(noupdate)" -m 644 -F $(CFGDIR) \
		-src snetd.config snetd
	$(INSTALL) -idb DLPI -idb "config(noupdate)" -m 644 -F $(CFGDIR) snetd.options

# 
# Finally the rule to make $TARGETS of this Makefile.
$(TARGETS): $(OBJECTS)
	$(CCF) $(OBJECTS) $(LIB) -o $(TARGETS) $(LDFLAGS)
