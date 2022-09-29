include $(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

TARGETS=latenscope
CFILES=latenscope.c elfsubr.c prfsubr.c
LLDLIBS=-ldwarf -lelf

default: $(TARGETS)

include $(COMMONRULES)

latenscope: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
