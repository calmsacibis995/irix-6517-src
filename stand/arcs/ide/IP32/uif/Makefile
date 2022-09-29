DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

CFILES= status.c busy.c msg_printf.c mfg_reboot.c

# override common POBJECTS so only necessary files compile for PROM
PCFILES= status.c msg_printf.c
POBJECTS= ${PCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules
