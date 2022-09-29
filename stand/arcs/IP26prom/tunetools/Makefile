#!smake

include ${ROOT}/usr/include/make/commondefs

C++FILES= calcpluck.c++ \
	$(NULL)

CFILES= plucksim.c \
	$(NULL)

#
# Should set these to the locations of the MIDI file(s):
#	MF_INC_LOC for location of MIDI file include files
#	MF_LIB_LOC for location of MIDI file library
#
MF_INC_LOC = .
MF_LIB_LOC = .

LC++INCS= -I$(MF_INC_LOC)
LC++OPTS = +a1 +w +p -float -Wf,-XNl10000

LCOPTS = 

LLDOPTS= -L$(MF_LIB_LOC)
LLDLIBS= -lmidifile -lmd -lC -lm -laudio -lfpe

OPTIMIZER= -g

TARGETS= calcpluck plucksim

default: $(TARGETS)

include $(COMMONRULES)

calcpluck:	calcpluck.o 
	$(C++F) $@.o -o $@ $(LDFLAGS)

plucksim:	plucksim.o 
	$(C++F) $@.o -o $@ $(LDFLAGS)

