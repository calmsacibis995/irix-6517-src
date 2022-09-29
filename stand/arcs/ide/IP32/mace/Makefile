DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

HFILES= sio.h TL16550.h mace_i2c_diags.h
ASFILES= 
CFILES= cntrs.c sio_loop.c rt_clock.c timer.c mace_i2c_diag.c enet.c pcidiags.c flash.c nic.c
SUBDIRS=plp
LLCOPTS= -triton_ucode -mips3

include ${DIAGDEPTH}/Makerules

_lib: ${CFILES} ${SUBDIRS}

${SUBDIRS}::
	@for d in ${SUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE} _lib"; \
		cd $$d; ${MAKE} _lib; cd ..; \
	done


