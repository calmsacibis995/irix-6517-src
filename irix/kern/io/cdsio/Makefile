# Makefile for 3608 firmware
# "$Revision: 1.2 $"

HOBJS	=3608self.obj 3608beg.obj 3608host.obj  3608main.obj
SHELL	=/bin/sh
AMSLOAD	=doscmd amsload
TASM	=doscmd tasm

3608firm.hex:3608firm.ams btohex
	btohex < 3608firm.ams >$@

3608firm.ams:3608firm.exe
	$(AMSLOAD) $? $@ 4000 fc00 0 0

3608firm.exe:$(HOBJS)
	printf "$(HOBJS)\n3608firm\n3608firm/MAP/LI" | doscmd link

$(HOBJS):3608equ.h

objects:$(HOBJS) btohex

btohex:	btohex.c
	cc -O -o $@ $?

clean:
	rm -f *.obj *.lst *.map 3608firm.exe 3608firm btohex make-out*

clobber:clean
	rm -f *.hex

.SUFFIXES:.asm .obj
.asm.obj:
	$(TASM) /l/m2/n/t/z/zi $*.asm
