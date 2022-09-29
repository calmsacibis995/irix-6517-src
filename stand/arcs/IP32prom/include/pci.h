#ifndef _pci_h_
#define _pci_h_

#include <sys/types.h>
#include <arcs/hinv.h>

/*
 * Although different platforms may implement PCI configuation differently,
 * the PCI configuration routines use a logical model with N pairs
 * of CONFIG_ADDR and CONFIG_DATA registers, with each pair corresponding
 * to one, top level "peer" PCI bus.
 *
 * CONFIG_ADDR   A 32 bit register interpreted as:
 *
 *  3       2        1     1
 *  1       4        6     1   8      2  0
 * +-+-------+--------+-----+---+------+--+
 * |e| peer  | bus    | dev |fun| reg  |00|
 * +-+-------+--------+-----+---+------+--+
 * 
 * e	Enables CONFIG_DATA access to config space.
 * peer Not used by PCI HW.  Used by SW to select one of N platform PCI busses
 * bus  Specifies PCI bus in a bridge/bus hierarchy.
 * dev  Specifies a particular device on a bus.
 * fun  Specifies a particular function on a device.
 * reg  Specifies a particular register on a function
 *
 * CONFIG_DATA  A 32 bit register used to read/write the config space register
 *              addressed by CONFIG_ADDR.
 */

/* Generate a config address from its components */
#define PCI_CFG_ADDR(p,b,d,f,r)  ((p)<<24) | ((b)<<16) | \
                                 ((d)<<11) | ((f)<<8)  | ((r)<<2)


/* Generate a config address from existing addr, setting only the reg field */
#define PCI_CFG_ADDR_R(a,r)      (a&0xffffff00) | ((r)<<2)


/* Access the various fields of an address */
#define PCI_PEER(a) ((a>>24)&0x7f)
#define PCI_BUS(a)  ((a>>16)&0xff)
#define PCI_DEV(a)  ((a>>11)&0x1f)
#define PCI_FUN(a)  ((a>>8)&0x7)
#define PCI_REG(a)  ((a>>2)&0x3f)


/* PCI configuration routines */
COMPONENT* pci_config(COMPONENT*,int peer);
void      cpu_pci_cfg_write_addr(int peer,__int32_t addr);
__int32_t cpu_pci_cfg_read_addr(int peer);
void      cpu_pci_cfg_write_data(int peer,__int32_t data);
__int32_t cpu_pci_cfg_read_data(int peer);


/* Prototype for PCI component installer routine */
typedef int (*installer)(COMPONENT*, __int32_t cfgaddr, int* nextBus);

/* PCI "allocate I/O space" routine */
int pci_alloc_io(__int32_t addr);

/* PCI slot and function maximums */
#define PCI_SLOT_MAX 32
#define PCI_FUNC_MAX 8


/* Structure containing supported product data */
typedef struct{
  __int32_t device_vendor;	/* Combined deviceID/VendorID */
  installer inst;		/* Ptr to PCI installer routine */
} PCI_Support_t;


/* List of supported options */
extern PCI_Support_t pci_supported[];

#endif
