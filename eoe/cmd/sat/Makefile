#
# Makefile for audit daemon and tools
#
#ident	"$Revision: 1.24 $"

include	$(ROOT)/usr/include/make/commondefs

# set up make depend for single-source-file programs
LMKDEPFLAGS = $(NULLSUFFIX_MKDEPFLAG)

## OPTIMIZER=-g
LCOPTS = -fullwarn

LIBSAT     = libsat.a
LIBSAT_SRC = sat_new_token.c sat_token.c sat_token_to_text.c sat_fetch_token.c
LIBSAT_OBJ = sat_new_token.o sat_token.o sat_token_to_text.o sat_fetch_token.o

CFILES = sat_echo.c	sat_select.c	sat_interpret.c	\
	 sat_reduce.c	sat_summarize.c	satd.c \
	 sat_interpret31.c sat_reduce31.c sat_summarize31.c

PROGRAMS= sat_echo sat_select sat_interpret sat_reduce sat_summarize satd
PROGRAMS31 = sat_interpret31 sat_reduce31 sat_summarize31

TARGETS= libsat.a ${PROGRAMS} ${PROGRAMS31}

DIRS   = gui
UPD    = "config(update)"
NOUPD  = "config(noupdate)"

default: $(TARGETS)
	@for i in $(DIRS); do \
		$(ECHO) "====\tcd $$i; make $@"; \
		cd $$i; $(MAKE) $@; cd ..; \
	done

COMMONPREF=sat_
include $(COMMONRULES)

${LIBSAT}: ${LIBSAT_OBJ}
	${AR} crs ${LIBSAT} ${LIBSAT_OBJ}

sat_interpret: $$@.c ${LIBSAT}
	${CCF} $@.c ${LDOPTS} ${LIBSAT} -o $@

sat_reduce: $$@.c ${LIBSAT}
	${CCF} $@.c ${LDOPTS} ${LIBSAT} -o $@

sat_summarize: $$@.c ${LIBSAT}
	${CCF} $@.c ${LDOPTS} ${LIBSAT} -o $@

sat_interpret31: $$@.c ${LIBSAT}
	${CCF} $@.c ${LDOPTS} -o $@

sat_reduce31: $$@.c ${LIBSAT}
	${CCF} $@.c ${LDOPTS} -lgen -o $@

sat_summarize31: $$@.c ${LIBSAT}
	${CCF} $@.c ${LDOPTS} ${LIBSAT} -o $@

install $(COMMONTARGS): $(COMMONPREF)$$@
	@for i in $(DIRS); do \
		$(ECHO) "====\tcd $$i; make $@"; \
		cd $$i; $(MAKE) $@; cd ..; \
	done

$(COMMONPREF)install: default
	$(INSTALL) -m 700 -u auditor -dir /var/adm/sat
	$(INSTALL) -m 755 -u auditor -dir /sat
	$(INSTALL) -m 700 -u auditor -idb "delhist" -dir /usr/adm/sat
	$(INSTALL) -F /usr/bin $(PROGRAMS)
	$(INSTALL) -F /usr/bin $(PROGRAMS31)
	$(INSTALL) -F /etc/init.d -m 755 -idb $(UPD) audit
	$(INSTALL) -F /etc/rc0.d -lns ../init.d/audit K27audit
	$(INSTALL) -F /etc/rc2.d -lns ../init.d/audit S35audit
	$(INSTALL) -F /etc/config -m 644 -idb $(NOUPD) -src audit.config audit
