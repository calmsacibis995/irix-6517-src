DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

CFILES= status.c busy.c msg_printf.c

# override common POBJECTS so only necessary files compile for PROM
PCFILES= status.c msg_printf.c
POBJECTS= ${PCFILES:.c=.o}

# overide common COBJECTS here for core IDE
CCFILES= busy.c msg_printf.c status.c
COBJECTS= ${CCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules
