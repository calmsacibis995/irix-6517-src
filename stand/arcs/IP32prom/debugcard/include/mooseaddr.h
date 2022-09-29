/*
 * =========================================================
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:41:38 $    
 * $Author: philw $   
 * =========================================================
 *  Moosehead address space.
 */

#define PCIMEMbase          0x2                 /* bits 35:32           */
#define PCIIObase           0x1                 /* bits 35:32           */
#define prom_base           0x1fc00000          /* prome  address space */
#define sio_base            0x1f800000          /* SIO register addr    */
#define av_base             0x1f400000          /* A/V register addr    */
#define io_base             0x1f000000          /* IO  registers        */
#define pci_config          0x1c000000          /* PCI configurations.  */
#define pci_membase         0x1a000000          /* PCI low mem base.    */
#define pci_iobase          0x18000000          /* PCI low io  base.    */
#define vice_regbase        0x17000000          /* VICE registers,      */
#define gbe_regbase         0x16000000          /* GBE  registers,      */
#define crime_regbase       0x14000000          /* crime registers,     */
#define low_membase         0x00000000          /* first 256M memory.   */


#define ECC_base			0x20000000          /* the addr which check ecc */
#define no_ECC_base			0x30000000          /* the addr which check ecc */
#define kseg1               0xa0000000          /* memory address space */
#define kseg0               0x80000000          /* memory address space */
#define mem_base            kseg1               /* memory address space */

/*
 * ========================================
 *
 * $Log: mooseaddr.h,v $
 * Revision 1.1  1997/08/18 20:41:38  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.2  1996/10/31  21:51:31  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.1  1996/03/25  22:17:44  kuang
 * These should fix the PHYS_TO_K1 problem
 *
 * Revision 1.4  1995/07/07  22:47:42  kuang
 * Fixed the Triton C model's mem_base define - use kseg1 address space.
 *
 * Revision 1.3  1995/06/29  02:52:01  kuang
 * Added support for kseg1 address space.
 *
 * Revision 1.3  1995/06/29  02:48:49  kuang
 * *** empty log message ***
 *
 * Revision 1.2  1995/05/13  00:42:44  kuang
 * add model dependency for mem_base.
 *
 * Revision 1.1  1995/05/03  19:12:29  kuang
 * Initial revision
 *
 *
 * ========================================
 */

/* END OF FILE */
