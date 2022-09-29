

CFILES	= stest.c
OFILES	= ${CFILES:.C=.o}
CINCS	= -I/usr/include
SHDLIBC	=
CFLAGS	= $(CINCS) -g

TARGETS	= stest

default:	$(TARGETS)

stest:		$(OFILES)
	CC $(CFLAGS) -o stest $(OFILES)
