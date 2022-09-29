#ident "$Revision: 1.21 $"

include $(ROOT)/usr/include/make/commondefs

AWKFILE=awk.g.o
LEXFILE=awk.lx.o
CFILES= b.c main.c Token.c tran.c lib.c run.c parse.c proctab.c \
	freeze.c
TARGETS= oawk
LLDLIBS=-lm
YFLAGS=-d
LDIRT=proctab.c makeprctab temp Token.c awk.h y.tab.*

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin $(TARGETS)

oawk: $(AWKFILE) $(LEXFILE) $(OBJECTS)
	$(CCF) $(AWKFILE) $(LEXFILE) $(OBJECTS) $(LDFLAGS) -o $@

y.tab.h: awk.g.o
# touch y.tab.h so it will be newer than awk.g.o
	touch y.tab.h

awk.h: y.tab.h
	cp y.tab.h awk.h

$(LEXFILE) $(OBJECTS): awk.h awk.def

Token.c: awk.h tokenscript
	cp token.c $@
	chmod u+w $@
	ed - <tokenscript

proctab.c: ./makeprctab
	./makeprctab > proctab.c

./makeprctab: awk.h Token.c
	if [ "$(HOSTENV)" != "" ]; then                 \
		ROOTDIR=/usr cc -O -c makeprctab.c;     \
		ROOTDIR=/usr cc -O -c Token.c -o HostToken.o;          \
		ROOTDIR=/usr cc -o ./makeprctab makeprctab.o HostToken.o;\
	else                                            \
		$(HOST_CC) -O -c makeprctab.c;                  \
		$(HOST_CC) -O -c Token.c -o HostToken.o;                       \
		$(HOST_CC) -o ./makeprctab makeprctab.o HostToken.o;\
	fi

