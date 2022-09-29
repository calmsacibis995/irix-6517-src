#!smake
VDEPTH=..
include $(ROOT)/usr/include/make/commondefs
include ../semdefs
OBJECT_STYLE=N32
LD_QUICKSTART_INFO=
TARGETS= task_fake event_config
TASK_OFILES = $(TASK_CFILES:.c=.o)
TASK_CFILES = task_fake.c 
OTHER_OFILES= ../misc/alloc.o ../pthreads/sss_pthreads.o ../../semapi/libsemapi.a 
OPTIMIZER=-g
COMMONPREF=SSS

all: default

include $(COMMONRULES)

clean clobber: $(COMMONPREF)$$@

install: $(COMMONPREF)$$@

$(COMMONPREF)install default: $(TASK_OFILES) $(MESG_OFILES) event_config.o
	$(CCF) -o task_fake $(TASK_OFILES) $(OTHER_OFILES) \
		$(LDOPTS) $(LDLIBS) \
		-lpthread -lalloc
	$(CCF) -o event_config  event_config.o $(OTHER_OFILES) \
		$(LDOPTS) $(LDLIBS) \
		-lpthread -lalloc

