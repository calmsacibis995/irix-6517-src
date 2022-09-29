#!smake

include $(ROOT)/usr/include/make/commondefs

C++FILES = nfsunhang.c++

TARGETS = nfsunhang

default : $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -idb EOE -F /usr/lib $(TARGETS)
