#!smake
include ${ROOT}/usr/include/make/commondefs

CFILES=bloatview.c draw.c inode.c print.c process.c
# Needed for Redwood builds of bloatview
LCOPTS=-fullwarn -woff 1209,1506,1185
TARGETS=gmemusage pfdat_check
LLDLIBS=-lXext -lX11
LLDOPTS=-Wl,-T,7FC00000,-D,7FC40000

default: ${TARGETS}

include ${COMMONRULES}

install: ${TARGETS}
	${INSTALL} -F /usr/sbin -m 4755 gmemusage
	${INSTALL} -F /usr/lib/images -m 444 gmemusage.icon

# XXX  fix this!
# The dependency on bloatview.z is a hack to avoid a race condition
# here; the make of bloatview.z create a file named bloatview and then
# deletes it, so we'd better rebuild bloatview itself.

gmemusage : do_pfd_check ${OBJECTS}
	rm -f $@
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@
	${TAG} 0x00001043 $@

# 
# HACK:
#
# In order to make bloatview work on both 64-bit and 32-bit kernels regardless
# of whether bloatview is compiled 64-bit or 32-bit, we need to know the
# size of pfd_t in the kernel.  Unfortunately, there is no kernel call
# returns that value, and it isn't worth adding just for this binary.
# Currently, we have a pfd32_t and a pfd64_t defined in a local .h
# file, and we use the appropriate one based on the kernel type.  Again,
# we don't want to add pfd32_t and pfd64_t to <sys/pfdat.h> solely
# for this binary, so we have the following binary for checking to
# see if pfd_t has not changed.
#

do_pfd_check: pfdat_check
	./pfdat_check

pfdat_check: pfdat_check.c
	${CCF} pfdat_check.c -o $@
