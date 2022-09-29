#!smake
#
# Makefile for libdplace
#

DEPTH=.

include $(ROOT)/usr/include/make/libleafdefs

CFILES= y.tab.c lex.yy.c

include $(LIBLEAFRULES)


y.tab.c: place.y libdplace.c place_defs.h fix_yy_names.h
	yacc -d place.y
	mv y.tab.c y.tab.orig
	cat fix_yy_names.h > y.tab.c
	sed -e 's/yy[_a-z]*/__dplace_&/g' y.tab.orig >> y.tab.c
	rm y.tab.orig

lex.yy.c: place.l y.tab.h place_defs.h fix_yy_names.h
	cat fix_yy_names.h > lex.yy.c
	lex -t place.l | sed -e 's/yy[_a-z]*/__dplace_&/g' >> lex.yy.c

y.tab.h: y.tab.c

#
#Here was the simple world before yacc/lex library symbol conflicts
#	were hit by FIDAP
#
#y.tab.c: place.y libdplace.c place_defs.h 
#	yacc -d place.y
#
#lex.yy.c: place.l y.tab.h place_defs.h 
#	lex place.l 
#
#y.tab.h: y.tab.c
