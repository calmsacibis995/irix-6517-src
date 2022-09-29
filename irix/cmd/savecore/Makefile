#!smake
#ident "$Revision: 1.17 $"

BASEVERSION=32bit
ALTVERSIONS=64bit
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

#if $(VERSION) == "32bit"
COMMANDS=savecore uncompvm
LLDLIBS=-lelf -ldisk
#endif

#if $(VERSION) == "64bit"
COMMANDS=savecore
LLDLIBS=-lelf -ldisk
#endif

LCOPTS+=-fullwarn
DIR=/var/adm/crash
OLDDIR=/usr/adm/crash
UPDATE_FILES=README
NOUPDATE_FILES=bounds minfree

default:$(TARGETS)

include $(CMDCOMMONRULES)

savecore: savecore.o getstrings.o
	$(CCF) -o savecore savecore.o getstrings.o $(LDFLAGS)

uncompvm: uncompvm.o getstrings.o
	$(CCF) -o uncompvm uncompvm.o getstrings.o $(LDFLAGS)

32bitinstall: default
	${INSTALL} -m 755 -u root -g sys -F /usr/etc $(IDB_TAG32) savecore
	${INSTALL} -m 755 -u root -g sys -F /usr/etc uncompvm
	for d in $(COMMANDS); do\
		$(INSTALL) -F /etc -lns ../usr/etc/$$d $$d;\
	done
	$(INSTALL) -u root -g sys -dir $(DIR)
	$(INSTALL) -u root -g sys -idb "delhist" -dir $(OLDDIR)
	$(INSTALL) -m 644 -u root -g sys -F $(DIR) "$(UPDATE_FILES)"
	$(INSTALL) -m 644 -u root -g sys -F $(DIR) -idb "config(noupdate)" \
		"$(NOUPDATE_FILES)"
	$(INSTALL) -m 644 -u root -g sys -idb "delhist"  -dir $(OLDDIR)/bounds
	$(INSTALL) -m 644 -u root -g sys -idb "delhist"  -dir $(OLDDIR)/minfree

64bitinstall: default
	${INSTALL} -m 755 -u root -g sys -F /usr/etc $(IDB_TAG64) $(COMMANDS)
