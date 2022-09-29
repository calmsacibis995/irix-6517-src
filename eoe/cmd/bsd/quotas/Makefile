#!smake
#
# Makefile for disk quota utilities
#
# ident "$Revision: 1.10 $"

include $(ROOT)/usr/include/make/commondefs

USER_CFILES = quota.c
ADM_CFILES  = edquota.c quot.c quotacheck.c quotaon.c repquota.c
CFILES	= ${USER_CFILES} ${ADM_CFILES}

USER_TARGETS = ${USER_CFILES:.c=}
ADM_TARGETS  = ${ADM_CFILES:.c=}
TARGETS	= ${USER_TARGETS} ${ADM_TARGETS}
LCDEFS+=-DSTATIC=static -DVERS=V_62 -D_BSD_SIGNALS -D_BSD_TIME -D_NOPROJQUOTAS
LCOPTS+=-fullwarn -use_readonly_const
LDIRT	= quotaoff

all default: ${TARGETS} quotaoff

include $(COMMONRULES)

quota: $$@.c
	${CCF} $@.c ${LDOPTS} -lrpcsvc ${LDLIBS} -o $@ $(LDFLAGS) 

quot quotacheck: $$@.c
	${CCF} $@.c ${LDOPTS} -ldisk ${LDLIBS} -o $@ $(LDFLAGS)

quotaoff: quotaon
	rm -f quotaoff
	ln quotaon quotaoff

install: default
	${INSTALL} -F /usr/etc ${ADM_TARGETS}
	${INSTALL} -F /usr/bsd ${USER_TARGETS}
	${INSTALL} -F /usr/etc -ln quotaon quotaoff 

purify: default
	purify -leaks-at-exit=yes $(CCF) quota.c ${LDOPTS} -lrpcsvc ${LDLIBS} \
		-o quota
	purify -leaks-at-exit=yes $(CCF) quot.c ${LDOPTS} -ldisk ${LDLIBS} \
		-o quot
	purify -leaks-at-exit=yes $(CCF) quotacheck.c ${LDOPTS} -ldisk ${LDLIBS} \
		-o quotacheck
	purify -leaks-at-exit=yes $(CCF) quotaon.c ${LDOPTS} -lrpcsvc ${LDLIBS} \
		-o quotaon
	purify -leaks-at-exit=yes $(CCF) edquota.c ${LDOPTS} ${LDLIBS} \
		-o edquota
	purify -leaks-at-exit=yes $(CCF) repquota.c ${LDOPTS} ${LDLIBS} \
		-o repquota

