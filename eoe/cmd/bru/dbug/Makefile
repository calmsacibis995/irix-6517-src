#
#	Makefile for the DBUG support library
#

include $(ROOT)/usr/include/make/commondefs

TARGET	= libdbug.a
TARGETS	= $(TARGET)

CVERSION=-cckr

CFILES	= dbug.c

default:	$(TARGET)

include $(ROOT)/usr/include/make/commonrules

$(TARGET):	$(OBJECTS)
	rm -f $@
	$(AR) cr $@ $(OBJECTS)

install: default
	@echo "libdbug.a is currently a local library; not installed"
#	$(INSTALL) -idb "std.sw.noship" -F /usr/lib $(TARGET)
