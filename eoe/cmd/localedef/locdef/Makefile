#!smake

include $(ROOT)/usr/include/make/commondefs

TARGETS	= localedef localedef.cat localedef.cat.m
LDIRT	= localedefl.c localedefy.c y.tab.h localedef.str mkinclmsg \
	  localedef_msg.h
CFILES	= localedefy.c localedefl.c collate.c localedef.c tables.c

CHARMAP	= POSIX ISO8859-1 ISO8859-15
LCDEFS	= -Xcpluscomm
LLDLIBS = -ll -lgen
YFLAGS	= -d

#ifdef DEBUG
OPTIMIZER=-g
LCDEFS += -DYYDEBUG=1 -DLEXDEBUG=1 -DLEX_DEBUG=1
#endif


GENCAT  = $(TOOLROOTSAFE) $(TOOLROOT)/usr/bin/gencat

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin localedef
	$(INSTALL) -F /usr/lib/locale/charmap $(CHARMAP)
	$(INSTALL) -F /usr/lib/locale/C/LC_MESSAGES localedef.cat
	$(INSTALL) -F /usr/lib/locale/C/LC_MESSAGES localedef.cat.m

localedef: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

localedef.c: y.tab.h

# Both y.tab.h and localedefy.c depend on loc.y
# but they need to be serialized so that yacc does
# not get run twice.
y.tab.h: localedefy.c
localedefy.c: loc.y
	$(YACC) $(YFLAGS) $?
	mv y.tab.c localedefy.c

localedefl.c: loc.l y.tab.h
	$(LEX) $(LFLAGS) loc.l
	mv lex.yy.c localedefl.c

# Warnings turned off:
# 1110	statement is unreachable
# 1167	pointer points outside of underlying object
localedefl.o: localedefl.c
	$(CCF) -woff 1110,1167 -c localedefl.c

$(OBJECTS): localedef_msg.h

localedef_msg.h: localedef.str
localedef.str: localedef.msg mkinclmsg
	$(TOOLROOTSAFE) ./mkinclmsg -x \
		localedef.msg localedef_msg.h localedef.str localedef

localedef.cat: localedef.cat.m
localedef.cat.m: localedef.str
	$(GENCAT) localedef.cat localedef.str

mkinclmsg: mkinclmsg.c
	$(CCF) mkinclmsg.c $(LDFLAGS) -o $(.TARGET)
