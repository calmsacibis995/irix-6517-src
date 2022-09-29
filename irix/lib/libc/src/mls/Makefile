#!smake

#ident "$Revision: 1.8 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES=
#else
CFILES=	acl_posix.c \
	cap_acquire.c \
	cap_env.c \
	cap_posix.c \
	getcapabilitybyname.c \
	mac_cleared.c \
	mac_dup.c \
	mac_label.c \
	mac_lbldevs.c \
	mac_posix.c \
	mac_util.c \
	sat_call.c \
	sat_event.c \
	sat_filehdr.c \
	sat_record.c \
	sgi_eag_mount.c
#endif


include $(LIBLEAFRULES)
