
#include "io6inst.h"

/* pointers to various device install functions */
/* To make changes to this setup:
   add the install routine as extern int in io6inst.h.
   Add an entry to this struct of the form
   vendor_id, device_id, revision_id, install routine address
   Do not change the last entry.
   The vendor_id cannot be NULL. If it is then change the 
   find_pci_driver routine in klconfig.c.
*/

sa_pci_ident_t prom_pci_info [] = {
0x3, 0x10a9, 10, kl_ioc3_install,	  /* IOC3 driver */
0x1020 ,0x1077, 10, kl_qlogic_install,      /* QLOGIC SCSI 11 */
0x009, 0x3d3d, 10, kl_rad1_install,     /* PSITECH RAD1 */
#ifdef SABLE
0x0300, 0xfafb, 10, kl_hubuart_install,     /* HUBUART */
0x0300, 0xfafe, 10, sdsk_install,         /* SABLE DISK */
#endif

NULL, NULL, NULL, NULL
} ;

