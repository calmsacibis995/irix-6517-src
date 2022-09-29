#	A new Makefile for the PromICE commands.
#	The old one sucked dead bunnies through a bent straw.
#

CC = cc
CFLAGS = $(OPTIMIZER) $(LCDEFS) $(LCOPTS)

OPTIMIZER= -g
LCDEFS= -DANSI -DUNIX -DFASTP

TARGETS= loadice

OBJECTS= \
                piglob.o  \
                loadice.o \
                pishell.o \
                pisyn.o \
                pisynf.o \
                piedit.o \
                picore.o \
                pifile.o \
                piconfig.o \
                pierror.o \
                pidriver.o \
                piunix.o \
                piutil.o \
                pimap.o

default: $(TARGETS)

loadice: $(OBJECTS)
	echo $(OBJECTS)
	$(CC) -v -o $@ $(OBJECTS) -L/usr/lib -lc
