# 
#  CS348C  Radiosity     Device independent graphics package GLIB.
#                        SUN/Xview version makefile
#
#

OBJS   = glibps.o
TARGET = glibps.a
INCLUDES = -I./glibdumb -I./glibps -I$(ROOT)/usr/include
CFLAGS = -O2 -cckr -mips3 -64 -woff 1116 -DNUMA $(INCLUDES)
LDFLAGS = -lm



default install: $(TARGET)

clobber:
	rm -f core *.o *~ $(TARGET)

$(TARGET): $(OBJS)
	ar crv $(TARGET) $(OBJS)

