# 
#  Device independent graphics package GLIB.
#  NULL graphic device version
#

OBJS   = glib.o
TARGET = glib.a
INCLUDES = -I./glibdumb -I./glibps -I$(ROOT)/usr/include
CFLAGS = -O2 -cckr -mips3 -64 -woff 1116 -DNUMA $(INCLUDES)
LDFLAGS = -lm

default install: $(TARGET)

clobber:
	rm -f core *.o *~ $(TARGET)

$(TARGET): glib.o
	ar crv $(TARGET) glib.o

