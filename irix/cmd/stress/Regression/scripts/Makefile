#
# Makefile for test scripts
#

OBJECTS = IO Ipc Istress Misc Mmap Pigs Vm X acceptance disks

all:	$(OBJECTS)

$(OBJECTS): $$@.sh
	cp $@.sh $@; chmod +x $@

install: all
	/etc/install -F /usr/stress/scripts $(OBJECTS)

clean:

clobber:
	rm -f $(OBJECTS)
