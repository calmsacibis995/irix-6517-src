#
#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=LC_COLLATE colltbl
CFILES=collfcns.c colltbl.c diag.c lex.c
YFILES=parse.y
HOSTOBJECTS=$(CFILES:.c=.ho) $(YFILES:.y=.ho)
YFLAGS=-d
LDIRT=hostcolltbl y.tab.h $(HOSTOBJECTS) parse.c
.SUFFIXES:.ho

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin colltbl
	${INSTALL} -F /usr/lib/locale/C colltbl_C
	${INSTALL} -F /usr/lib/locale/C LC_COLLATE

LC_COLLATE:hostcolltbl
	./hostcolltbl colltbl_C

hostcolltbl:$(HOSTOBJECTS)
	$(HOST_CC) $(MKDEPOPT) -o $@ $(HOSTOBJECTS)

colltbl:$(OBJECTS)
	$(CCF) -o $@ $(OBJECTS) $(LDFLAGS)

lex.ho lex.o:y.tab.h

y.tab.h:parse.c

.c.ho:
	$(HOST_CC) $(MKDEPOPT) -c $< -o $*.ho
