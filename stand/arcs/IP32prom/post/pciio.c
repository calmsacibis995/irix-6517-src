/*
 * Basic PCI functions for use during post.
 *
 * This is basically copied over from libsk/io/pci_intf.c
 *
 * $Id: pciio.c,v 1.1 1997/08/18 20:48:19 philw Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mips_addrspace.h>
#include <sys/edt.h>
#include <arcs/hinv.h>
#include <sys/mace.h>
#include <pci/PCI_defs.h>
#include <pci_intf.h>


#if defined(IP32)
#define PCI_CONFIG_ADDR_PTR	((volatile u_int32_t *) (PHYS_TO_K1(PCI_CONFIG_ADDR)))
#define PCI_CONFIG_DATA_PTR	((volatile u_int32_t *) (PHYS_TO_K1(PCI_CONFIG_DATA)))
#define PCI_ERROR_FLAGS_PTR	((volatile u_int32_t *) (PHYS_TO_K1(PCI_ERROR_FLAGS)))
#endif

/*
 * Read a value from a configuration register.
 * For Moosehead, we assume the system is big endien, the system PCI bridge
 * does not do address or data swizzling, and the device is little endien.
 */
u_int32_t
pci_get_cfg(volatile unsigned char *bus_addr, int cfg_reg)
{
	u_int32_t	tmp=0;
	volatile unsigned char *cp;

#if defined(IP32)
	u_int32_t	pci_err_flags;

	/*
	 * Read the 32 bit data now.  Decide which part of it to return
	 * in the switch stmt. XXX This function should really return a seperate
	 * return value.
	 */
	cp = bus_addr + (cfg_reg & 0xfc);
	*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
	tmp = *(PCI_CONFIG_DATA_PTR);
	pci_err_flags = *PCI_ERROR_FLAGS_PTR;
	if (pci_err_flags & PERR_MASTER_ABORT) {
		*PCI_ERROR_FLAGS_PTR &= ~(PERR_MASTER_ABORT);	/* clear bit */
		return -1;
	}

#else	/* not IP32 */
	cp = bus_addr + cfg_reg;
#endif

	switch (cfg_reg) {
	case PCI_CFG_VENDOR_ID:
	case PCI_CFG_DEVICE_ID:
	case PCI_CFG_COMMAND:
	case PCI_CFG_STATUS:
		/*
		 * Reads of these registers will return 16 bits in the lower
		 * 16 bits of the 32 bit return value.
		 */
#if defined(IP32)
		if ((cfg_reg & 0x3) == 0)      /* even addr: take lower 16 bits */
			tmp = tmp & 0xffff;
		else				/* odd addr: take upper 16 bits */
			tmp = tmp >> 16;
		break;
#else
		tmp = (u_int32_t)*(u_int16_t *)cp;
		tmp = SWAP_BYTES(tmp);

		cp = (volatile unsigned char *)((unsigned int)cp ^ 2);
		tmp = (u_int32_t)*(u_int16_t *)cp;
#endif
		break;

	case PCI_CFG_REV_ID:
	case PCI_CFG_BASE_CLASS:
	case PCI_CFG_SUB_CLASS:
	case PCI_CFG_PROG_IF:
	case PCI_CFG_CACHE_LINE:
	case PCI_CFG_LATENCY_TIMER:
	case PCI_CFG_HEADER_TYPE:
	case PCI_CFG_BIST:
		/*
		 * Reads of these registers will return 8 bits in the lower
		 * 8 bits of the 32 bit return value.
		 */
#if defined(IP32)
		if ((cfg_reg & 0x3) == 0) {	/* bits 0-7 */
			tmp = tmp & 0xff;
		} else if ((cfg_reg & 0x3) == 1) { /* bits 8-15 */
			tmp = (tmp & 0xff00) >> 8;
		} else if ((cfg_reg & 0x3) == 2) { /* bits 16-32 */
			tmp = (tmp & 0xff0000) >> 16;
		} else {			   /* bits 24-31 */
			tmp = (tmp & 0xff000000) >> 24;
		}
#else
		cp = (volatile unsigned char *)
			((__psunsigned_t)cp + SWIZZLE_BYTEPTR(cfg_reg));
		tmp = *cp;
#endif
		break;
		   
	case PCI_CFG_BASE_ADDR_0:
	case PCI_CFG_BASE_ADDR_1:
	case PCI_CFG_BASE_ADDR_2:
	case PCI_CFG_BASE_ADDR_3:
	case PCI_CFG_BASE_ADDR_4:
	case PCI_CFG_BASE_ADDR_5:
		/* reads of these registers will return 32 bits */
#if defined(IP32)
		/* done.  no swapping of data */
#else
		tmp = *(u_int32_t *)cp;
		tmp = SWAP_WORD(tmp);
#endif
		break;

	default:
#if defined(IP32)
		/* done.  no swapping of data */
		break;
#else
		tmp = *(u_int32_t *)cp;
#endif
	}

	return tmp;
}

/*
 * See comments at pci_get_cfg.
 */		   
int
pci_put_cfg(volatile unsigned char *bus_addr, int cfg_reg, __int32_t val)
{
	u_int32_t	tmp = (u_int32_t)val;
	volatile unsigned char *cp;

#if defined(IP32)
	/*
	 * Read the 32 bit data now.  Merge it with the correct bits
	 * passed in by val.  If the read succeeds, then the device is there.
	 * XXX what about errors from the read? 
	 */
	cp = bus_addr + (cfg_reg & 0xfc);
	*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
	tmp = *(PCI_CONFIG_DATA_PTR);
#else   /* not IP32 */
	cp = bus_addr + cfg_reg;
#endif

	switch (cfg_reg) {
	case PCI_CFG_VENDOR_ID:
	case PCI_CFG_DEVICE_ID:
	case PCI_CFG_REV_ID:
	case PCI_CFG_BASE_CLASS:
	case PCI_CFG_SUB_CLASS:
	case PCI_CFG_PROG_IF:
	case PCI_CFG_HEADER_TYPE:
		break;

	case PCI_CFG_COMMAND:
	case PCI_CFG_STATUS:
		/*
		 * The lower 16 bits of val will be written to the appropriate
		 * place in the config register.
		 */
#if defined(IP32)
		if ((cfg_reg & 0x3) == 0)	/* even addr: put in lower 16 bits */
			tmp = (tmp & 0xffff0000) | (val & 0xffff);
		else				/* odd addr: put in upper 16 bits */
			tmp = (tmp & 0xffff) | (val << 16);
		*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
		*(PCI_CONFIG_DATA_PTR) = tmp;
#else
		tmp = SWAP_BYTES(tmp);
		cp = (volatile unsigned char *)((__psunsigned_t) cp ^ 2);
		*(u_int16_t *) cp = (u_int16_t) tmp;	
#endif
		break;

	case PCI_CFG_CACHE_LINE:
	case PCI_CFG_LATENCY_TIMER:
	case PCI_CFG_BIST:
		/*
		 * The lower 8 bits of val will be written to the appropriate
		 * place in the config register.
		 */
#if defined(IP32)
		if ((cfg_reg & 0x3) == 0)	/* put in bits 0-7 */
			tmp = (tmp & 0xffffff00) | (val & 0xff);
		else if ((cfg_reg & 0x3) == 1)	/* put in bits 8-15 */
			tmp = (tmp & 0xffff00ff) | ((val & 0xff) << 8);
		else if ((cfg_reg & 0x3) == 2)	/* put in bits 16-23 */
			tmp = (tmp & 0xff00ffff) | ((val & 0xff) << 16);
		else				/* put in bits 8-15 */
			tmp = (tmp & 0x00ffffff) | ((val & 0xff) << 24);
		*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
		*(PCI_CONFIG_DATA_PTR) = tmp;
#else
		cp = (volatile unsigned char *)
			((__psunsigned_t) cp + SWIZZLE_BYTEPTR(cfg_reg));
		*cp = (u_char) tmp;
#endif
		break;
		   
	case PCI_CFG_BASE_ADDR_0:
	case PCI_CFG_BASE_ADDR_1:
	case PCI_CFG_BASE_ADDR_2:
	case PCI_CFG_BASE_ADDR_3:
	case PCI_CFG_BASE_ADDR_4:
	case PCI_CFG_BASE_ADDR_5:

#if defined(IP32)
		/*
		 * All 32 bits of val are put into the register.  Wasted
		 * the read above...hmmm
		 */
		*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
		*(PCI_CONFIG_DATA_PTR) = val;
#else	/* not IP32 */
		tmp = SWAP_WORD(tmp);
		*(u_int32_t *)cp = tmp;
#endif
		break;
		   
	default:
#if defined(IP32)
		*(PCI_CONFIG_ADDR_PTR) = (u_int32_t) cp;
		*(PCI_CONFIG_DATA_PTR) = val;
#else
		*(u_int32_t *)cp = tmp;
#endif
	}

	return 0;
}
