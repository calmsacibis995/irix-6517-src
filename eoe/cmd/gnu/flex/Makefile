#
include	$(ROOT)/usr/include/make/commondefs

CFILES=ccl.c dfa.c ecs.c gen.c main.c misc.c nfa.c parse.c \
	scan.c sym.c tblcmp.c yylex.c

TARGETS=flex
 
FLEX=./flex
FLEXLIB=libfl.a
SKELTON_FILE=flex.skel
FLEX_FLAGS=-ist8 -Sflex.skel

LDIRT=parse.h parse.c scan.c $(FLEXLIB)
LCOPTS= -D__STDC__ -DUSG -DDEFAULT_SKELETON_FILE=\"$(ROOT)/usr/lib/$(SKELTON_FILE)\"

default: $(TARGETS) $(FLEXLIB)

include $(COMMONRULES)

scan.c: initscan.c
	cp initscan.c scan.c

newscan.c : scan.l
	$(FLEX) $(FLEX_FLAGS) $(COMPRESSION) scan.l >scan.c

flex: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

$(FLEXLIB): libmain.o
	$(AR) cru $(FLEXLIB) libmain.o

parse.c parse.h: parse.y
	$(YACC) -d parse.y
	@mv y.tab.c parse.c
	@mv y.tab.h parse.h

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)
	$(INSTALL) -F /usr/lib $(SKELTON_FILE)
	$(INSTALL) -F /usr/lib $(FLEXLIB)

test : flex
	./flex $(FLEX_FLAGS) $(COMPRESSION) scan.l | diff scan.c -

bigtest :
	rm -f scan.c ; $(MAKE) COMPRESSION="-C" test
	rm -f scan.c ; $(MAKE) COMPRESSION="-Ce" test
	rm -f scan.c ; $(MAKE) COMPRESSION="-Cm" test
	rm -f scan.c ; $(MAKE) COMPRESSION="-Cfe" test
	rm -f scan.c ; $(MAKE) COMPRESSION="-CFe" test
	rm -f scan.c ; $(MAKE) COMPRESSION="-Cf" test
	rm -f scan.c ; $(MAKE) COMPRESSION="-CF" test
	rm -f scan.c ; $(MAKE)
