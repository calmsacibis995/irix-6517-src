#!smake
#
# Makefile for Nexus DP-Node distributed service IDL compiler.
#
include $(ROOT)/usr/include/make/commondefs
DEPTH=../..
include ${DEPTH}/kcommondefs
TOP=$(DEPTH)
KPATH=$(TOP)/cell/kernidl

TARGETS = $(BOOTAREA)/kernidl

default: $(TARGETS)

include $(COMMONRULES)

LEX = $(TOOLROOTSAFE) $(TOOLROOT)/usr/bin/flex
YACC = $(TOOLROOTSAFE) $(TOOLROOT)/usr/bin/bison --yacc

LEXMKDEPFLAGS += -woff 3203
CXX=/usr/bin/CC
CXXFLAGS=-mips3 -n32 -O -MDupdate $(MKDEPFILE) -c
LDFLAGS=

C++FILES = main.c++ emit_files.c++ node.c++ lexer.c++ parser.c++
LDIRT = lexer.c++ parser.c++ parser.h

$(BOOTAREA)/kernidl: $(OBJECTS)
	$(C++F) $(OBJECTS) $(LDFLAGS) -o $@


parser.h: parser.o
emit_files.o node.o: parser.h

# Special rule to generate a C++ lexical analyzer from the lex source
# (and depend on the parser generated header file).
#
lexer.o: lexer.l parser.h
	$(LEX) $(LFLAGS) lexer.l
	mv lex.yy.c lexer.c++
	$(CXX) $(CXXFLAGS) $(LEXMKDEPFLAGS) -c lexer.c++
	rm lexer.c++

# Special rule to generate a C++ parser from the yacc source and move *both*
# the generated ``.c'' and ``.h'' files to the appropriate base names.
#
parser.o: parser.y
	$(YACC) $(YFLAGS) -d parser.y
	mv y.tab.c parser.c++
	mv y.tab.h parser.h
	$(CXX) $(CXXFLAGS) $(YACCMKDEPFLAGS) -c parser.c++
	rm parser.c++

