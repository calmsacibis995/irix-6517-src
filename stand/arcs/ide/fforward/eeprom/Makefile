DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

ASFILES= 

CFILES= nvrm_addr.c \
	nvrm_data.c \
	nvrm_id.c \
	nvrm_pins.c \
	nvrm_rdwr.c

HFILES= eeprom.h

# overide common COBJECTS here for core IDE
CCFILES= nvrm_rdwr.c
COBJECTS= ${CCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules
