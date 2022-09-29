
include $(ROOT)/usr/include/make/commondefs

all: make_adpcm_array

adpcm.o: adpcm.h

LLDLIBS = -laudiofile -lm

default: make_adpcm_array


make_adpcm_array: make_adpcm_array.o adpcm.o
	$(CC) -o $@ make_adpcm_array.o adpcm.o $(LDFLAGS)

clean:
	-rm -f make_adpcm_array.o adpcm.o

clobber: clean
	-rm -f make_adpcm_array
