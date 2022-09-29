#!smake

include $(ROOT)/usr/include/make/commondefs

        TARGETS = $(EXECS) $(LIBS)

          EXECS = $(TOPWAIT) $(TOPFUNC)
           LIBS = $(LIBTOPGEN)
            MAN = $(TOPWAIT).1

         CFILES = $(TOPGEN_CFILES) $(TOPWAIT_CFILES) $(TOPFUNC_CFILES)

      LIBTOPGEN = lib$(TOPGEN).a
         TOPGEN = topgen
  TOPGEN_CFILES = top.c data.c util.c htab.c term.c
    TOPGEN_OBJS = $(TOPGEN_CFILES:.c=.o)

        TOPWAIT = topwait
 TOPWAIT_CFILES = topwait.c
   TOPWAIT_OBJS = $(TOPWAIT_CFILES:.c=.o)
   TOPWAIT_LIBS = -l$(TOPGEN) -lrtmon -lcurses

        TOPFUNC = topfunc
 TOPFUNC_CFILES = topfunc.c prfsubr.c elfsubr.c
   TOPFUNC_OBJS = $(TOPFUNC_CFILES:.c=.o)
   TOPFUNC_LIBS = -l$(TOPGEN) -ldwarf -lelf -lrtmon -lcurses

        LCDEFS += -D_KMEMUSER -DNDEBUG
        LCOPTS += -fullwarn
       LLDOPTS += -L.
        LLDLIBS = $(TOPWAIT_LIBS)

default: $(TARGETS)

$(TOPWAIT): $(TOPWAIT_OBJS) $(LIBTOPGEN)
	$(CCF) -o $@ $(TOPWAIT_OBJS) $(LDOPTS) $(TOPWAIT_LIBS)

$(TOPFUNC): $(TOPFUNC_OBJS) $(LIBTOPGEN)
	$(CCF) -o $@ $(TOPFUNC_OBJS) $(LDOPTS) $(TOPFUNC_LIBS)

$(LIBTOPGEN): $(TOPGEN_OBJS)
	$(AR) ruvs $@ $(TOPGEN_OBJS)

install: default
	$(INSTALL) -F /usr/sbin $(EXECS)

include $(COMMONRULES)
