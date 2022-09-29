#
# Makefile to make common IDE Module code
#

DIAGDEPTH=..
include $(DIAGDEPTH)/Makedefs

LDIRT= $(LLDIRT); \
if [ -d $(TARGETDIR) ]; then cd $(TARGETDIR); rm -f $(OBJECTS); fi

# overide the SHARE flags to set to shared
SHARE_COPT=
SHARE_ASOPT= -KPIC

CFILES= module.c
ASFILES= module_asm.s

default: $(OBJECTDIR) ${OBJECTS}

# more common rules
include $(DIAGDEPTH)/../commonrules
