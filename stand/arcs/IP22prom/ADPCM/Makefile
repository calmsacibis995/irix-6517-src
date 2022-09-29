all: make_adpcm_array

adpcm.o: adpcm.h
CFLAGS=-o32

LIBS=-laudiofile -laudioutil -lm

default: make_adpcm_array

make_adpcm_array: make_adpcm_array.o adpcm.o
	cc make_adpcm_array.o adpcm.o $(LIBS) -o $@

clean:
	-rm -f make_adpcm_array.o adpcm.o

clobber:	clean
	-rm -f make_adpcm_array
