# ide common makefile.

DIAGDEPTH= ..
include ${DIAGDEPTH}/Makedefs

LIBNAME=libidecmn.a

CFILES= ide_run.c ide_sym.c ide_main.c ide_lex.c ide_alloc.c ide_cmds.c \
	ide_sets.c ide_prims.c ide_mp.c ide_pmgr.c $(TARGETDIR)/ide_gram.c \
	resetcons.c ide_malloc.c
ASFILES=ide_slave.s

LINCLUDES=-I $(TARGETDIR)

YFLAGS=-d

default: $(OBJECTDIR) $(OBJECTS)

LLDIRT=$(TARGETDIR)/ide_gram.h $(TARGETDIR)ide_gram.c

$(TARGETDIR)/ide_gram.h $(TARGETDIR)/ide_gram.c: ide_gram.y
	(cd $(TARGETDIR) && $(YACC) $(YFLAGS) ../common/ide_gram.y && \
	mv y.tab.c ide_gram.c && \
	mv y.tab.h ide_gram.h)

ide_lex.o: ide_gram.h

include ${DIAGDEPTH}/Makerules
