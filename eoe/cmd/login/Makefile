#ident	"$Revision: 1.49 $"

include	$(ROOT)/usr/include/make/commondefs

# Compile Time Options
#
#	-DNO_MAIL	if defined, the MAIL environment variable will
#			not be set.
#	-DNOSHELL	if defined, non-standard shell names are not
#			placed into the appropriate environment variable.
#	-DMAXTRYS=XX	XX is the number of attempts permitted.
#			0 is "no limit" and if not defined the default is 3.
#	-DMAXTIME=XX	XX is the number of seconds before giving up.
#			0 is "no limit" and if not defined the default is 60.

TARGETS	= login 
CFILES	= login.c
LCDEFS	= -DEXECSH -DAFS -DDCE -DAUX_SECURITY -D_SHAREII
LCOPTS	= -fullwarn
LLDLIBS	= -lcmd -lgen -lcrypt -liaf -lpthread
LLDOPTS	= -Wl,-woff,85,-woff,134,-ignore_unresolved
SUGG	= "config(suggest)"
UPD	= "config(update)"

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -u root -g sys -m 755 -dir /var/adm/lastlog
	$(INSTALL) -u root -g sys -idb "delhist" -dir /usr/adm/lastlog
	$(INSTALL) -u root -g sys -m 755 -dir /usr/lib/iaf
	$(INSTALL) -u root -g sys -m 755 -dir /etc/default
	$(INSTALL) -F /usr/lib/iaf -m 4755 -u root -g sys -src login -O scheme
	$(INSTALL) -F /etc/default -m 644 -idb $(SUGG) -src login.dfl -O login
	$(INSTALL) -F /etc/config -m 644 -idb $(UPD) login.options
