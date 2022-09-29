#ident "$Revision: 1.2 $"

AM_SRC_DEPTH = ..

include $(ROOT)/usr/include/make/commondefs
include $(AM_SRC_DEPTH)/include/amcommondefs


#LCDEFS    = -DYYDEBUG=1 -DDEBUG
LDLIBS   = -lgen -ll -lssdb 

CFILES = amformat.c reportutils.c utils.c y.tab.c \
	 lex.yy.c structUtils.c eventutils.c dbutils.c \
	 notifyutils.c errorutils.c

TARGETS = amformat

default: $(TARGETS)

include $(COMMONRULES)

amformat: $(OBJECTS) Makefile
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

y.tab.c: parseryacc.y Makefile
	$(YACC) parseryacc.y

lex.yy.c: parserlex.l Makefile
	$(LEX) parserlex.l

install: default
	${INSTALL} $(IDB_TAG) -F $(ETCPATH) $(TARGETS)
	${INSTALL} $(IDB_TAG) -F $(AVAILPATH) -lns $(ETCPATH)/amformat amformat
