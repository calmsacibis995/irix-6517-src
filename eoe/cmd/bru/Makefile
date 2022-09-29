#ident "$Revision: 1.5 $"

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/releasedefs

#	add -DDBUG to the options to turn on the built-in 
#	debugging mode and enable the tracing options.
LCDEFS	= -DRMT -DNFS -signed -DSIGTYPEVOID -D'ID="SGI Release $(RELEASE)"'

# Don't bother with the dbug dir stuff, it isn't worth the hassle
# and the longer builds
#DBUG DBGLIB = dbug/libdbug.a
#DBUG COMMONPREF =	bru

LLDLIBS	= $(DBGLIB) -lrmt

TARGETS=bru
# don't bother # DIRS=dbug
CVERSION=-cckr

CFILES = access.c blocks.c bru.c brutab.c chksum.c create.c compress.c date.c \
	dblib.c devices.c diff.c done.c errors.c estimate.c execute.c \
	extract.c fmode.c globals.c group.c headers.c hex.c info.c \
	init.c inspect.c links.c passwd.c readinfo.c scan.c signals.c \
	symlinks.c sys.c sys2.c sys4.c table.c trees.c tty.c \
	usage.c utils.c wild.c stubs.c

ASFILES = asm.s


default:$(TARGETS)

include $(COMMONRULES)

bru:	$(OBJECTS) $(DBGLIB)
	$(CCF) $(LCOPTS) $(OBJECTS) -o bru $(LDFLAGS)

dbug/libdbug.a: ${_FORCE}
	cd dbug;$(MAKE) OPTIMIZER=$(OPTIMIZER)

#DBUG ${COMMONTARGS}: ${COMMONPREF}$$@
#DBUG 	@for d in ${DIRS}; do \
#DBUG 		${ECHO} "\tcd $$d; ${MAKE} $@"; \
#DBUG 		cd $$d; ${MAKE} $@; cd ..; \
#DBUG 	done

# brutab isn't really needed in the miniroot, but without it,
# bru returns the 'informative' exit status of 2
# and that breaks the system recovery script
install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)
	$(INSTALL) -idb "config(suggest)" -F /etc \
		-u root -g sys -m 644 brutab
