#	XPerform
#

CC		=	cc
CFLAGS	=	-O
LD		=	-lXtst -lXext -lX11
OBJ		=	xperform.o parse.o xtools.o icon.o util.o
INST	=	/usr/people/matth/Projects/xperform/files
#DEF		=	-DDEBUG
#DEF		=	-DDEBUG -DDUMMY

all:	xperform

install:
	make 
	cp xperform $(INST)
	cp script $(INST)
	cp README $(INST)
	cp sgisession $(INST)

clean:
	rm -f *.o xperform

.c.o:	$(CFILES)
	@echo '-- $@ --'
	$(CC) $(CFLAGS) -c $(DEF) $<

parse.o:	xperform.h
xperform.o:	parse.h xtools.h icon.h util.h
xtools.o:	xperform.h
icon.o:		xperform.h

xperform:	$(OBJ) 
	@echo '-- $@ --'
	$(CC) $(CFLAGS) $(OBJ) $(LD) -o xperform

