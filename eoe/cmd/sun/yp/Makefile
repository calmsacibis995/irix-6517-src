#!smake
# cmd/sun/yp sub-makefile.
#
include	${ROOT}/usr/include/make/commondefs

TOUCH= touch -cf
CVERSION=-cckr

LCDEFS=-D_BSD_TIME

PGMFILES= stdhosts.c ypmapname.c yppoll.c yppush.c \
	  ypset.c
CFILES= ${PGMFILES} ypdbname.c
UPDFILES= ypxfr_1pd.sh ypxfr_2pd.sh ypxfr_1ph.sh
SHFILES= ${UPDFILES}
TARGETS= ${PGMFILES:.c=} ${SHFILES:.sh=}

INSTDIR= /var/yp
INST= ${INSTALL} -F ${INSTDIR}

default: ${TARGETS}

#	@for i in ${DIRS}; do \
#		${ECHO} "=====\tcd $$i; ${MAKE} $@"; \
#		cd $$i; ${MAKE} $@; cd ..; \
#	done

COMMONPREF= yp
include	${COMMONRULES}

${COMMONTARGS} install: ${COMMONPREF}$$@

#	@for i in ${DIRS}; do \
#		${ECHO} "=====\tcd $$i; ${MAKE} $@"; \
#		cd $$i; ${MAKE} $@; cd ..; \
#	done

ypmapname ypxfr: $$@.o ypdbname.o
	${CCF} $@.o ypdbname.o ${LDFLAGS} -o $@

yppush: $$@.o ypdbname.o sem.o
	${CCF} $@.o ypdbname.o sem.o ${LDFLAGS} -o $@

${COMMONPREF}install: default
	${INST} -src yp.inst.sh \
		-idb "nohist" -idb 'postop($$rbase/var/yp/.yp.inst.sh)' .yp.inst.sh
	${INSTALL} -F /usr/sbin ${PGMFILES:.c=}
	${INST} updaters
	${INST} -idb "config(update)" ${UPDFILES:.sh=}
	${INST} -ln ypxfr_1pd ypxfr_1perday
	${INST} -ln ypxfr_2pd ypxfr_2perday
	${INST} -ln ypxfr_1ph ypxfr_1perhour
	${INSTALL} -F /usr/etc -lns ../../var/yp yp
