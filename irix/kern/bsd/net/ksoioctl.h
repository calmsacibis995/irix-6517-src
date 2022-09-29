#ifndef __net_ksoioctl__
#define __net_ksoioctl__
#ifdef __cplusplus
extern "C" { 
#endif
/*
 * Kernel-only 32-bit compatibility ioctl definitions and structs
 * (whole file)
 */
#if _MIPS_SIM == _ABI64 && defined( _KERNEL)

#include <sys/socket.h>
#include <sys/ioctl.h>

/* get ifnet list */
#define	IRIX5_SIOCGIFCONF _IOWR('i',20,struct irix5_ifconf) 

struct irix5_ifconf {
	app32_int_t	ifc_len;
	union {
		app32_ptr_t ifcu_buf;
		app32_ptr_t ifcu_req;
	} ifc_ifcu;
};


/* temp sysctl */
#define	IRIX5_SIOCRTSYSCTL _IOWR('r', 99, struct irix5_rtsysctl)	

struct irix5_rtsysctl {
	app32_ptr_t	name;
	app32_uint_t	namelen;
	app32_ptr_t	oldp;
	app32_int_t	oldlen;
	app32_ptr_t	newp;
	app32_int_t	newlen;
};

#endif

#ifdef __cplusplus
}
#endif
#endif	/* __net_ksoioctl__ */
