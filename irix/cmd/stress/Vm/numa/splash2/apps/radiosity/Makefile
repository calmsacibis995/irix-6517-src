#!smake
OBJECT_STYLE=64_M3
SPLASH_ROOT=../..
include $(ROOT)/usr/include/make/commondefs
include $(SPLASH_ROOT)/macros/defs

APPL = radiosity 
SUBDIRS= glibdumb glibps
CFILES = rad_main.C patchman.C elemman.C taskman.C modelman.C smallobj.C \
         display.C visible.C rad_tools.C room_model.C 
HFILES = model.H parallel.H patch.H radiosity.H task.H
IFILES = ${HFILES:.H=.h}
OBJS = ${CFILES:.C=.o}
SHFILES = runtests.sh 
DATFILES = 
FILES = $(CFILES) $(HFILES) $(SHFILES) $(DATFILES)
CFLAGS = -O2 -cckr -mips3 -64 -woff 1116 -DNUMA -I./glibdumb -I./glibps 
LDFLAGS = -lm glibdumb/glib.a glibps/glibps.a
LDIRT = *.c *.h *~ 

TARGETS= $(APPL) ${SHFILES:.sh=} ${DATFILES:.dat=}

default: $(TARGETS)
	$(SUBDIRS_MAKERULE)

$(APPL): $(OBJS) $(SUBDIRS)
	$(CCF) $(OBJS) -o $(APPL) $(GLDOPTS) $(LDFLAGS)

COMMONPREF=stress_
include $(SPLASH_ROOT)/macros/rules
include $(COMMONRULES)

install:$(TARGETS)
	$(INSTALL) -F /usr/stress/Vm/numa/splash2/apps/$(APPL) $(TARGETS)
	$(INSTALL) -F /usr/stress/Vm/numa/splash2/apps/$(APPL) README.radiosity

radiosity.h: patch.h model.h parallel.h task.h
rad_main.c: rad_main.C $(IFILES)
patchman.c: patchman.C $(IFILES)
elemman.c: elemman.C $(IFILES)
taskman.c: taskman.C $(IFILES)
modelman.c: modelman.C $(IFILES)
smallobj.c: smallobj.C $(IFILES)
display.c: display.C $(IFILES)
visible.c: visible.C $(IFILES)
rad_tools.c: rad_tools.C $(IFILES)
room_model.c: room_model.C $(IFILES)

$(COMMONTARGS): $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)
