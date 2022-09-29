#ifndef __EISA_ROM_H__
#define __EISA_ROM_H__

#ident $Revision: 1.4 $

#include <arcs/types.h>

/* 
	Prototypes of EISA ROM access functions.
*/

ULONG	*EisaAddress(ULONG BusNumber, ULONG SlotNumber, ULONG Offset);

int	ReadRomFolder(	ULONG	BusNumber,
			ULONG	SlotNumber, 
			UCHAR 	*Buffer, 
			ULONG 	ByteCount, 
			ULONG 	Offset);

int	GetRomInfo(	ULONG 	BusNumber,
			ULONG	SlotNumber,
			ULONG	*Flags,
			ULONG	*EisaId);



void	Set_EISA_int_mode(int BusNumber, int irq, int level);



void	init_eisa_dma_channel(	ULONG BusNumber, 
				ULONG Channel,
				ULONG Timing,
				ULONG TransferSize);


#define	ROM_INDEX	0xCB0		/* 32 bit Slot specific index address*/
#define	ROM_DATA	0xCB4		/* 32 bit Slot specific data address.*/

#define	SLOT_FLAGS	0xC84		/* 8 Bit Slot flags.		     */

	/* Bit definitions of SLOT_FLAGS */
#define	SLOT_ENABLE	0x01		/* Expansion board is enabled. r/w   */
#define	SLOT_IOCHECK	0x02		/* Serious error detected. ro	     */
#define	SLOT_IOCHKRST	0x04		/* Pulse for 500ns to reset. wo	     */
#define	SLOT_EISAROM	0x08		/* Rom firware is present.	     */

#define	PRODUCT_ID	0xC80		/* 32 bit EISA card id.		     */

#define MAX_EISA_IRQ	15		/* Max IRQ number		     */
#define	MAX_EISA_SLOT	15		/* Highest slot number.		     */
#define	MAX_EISA_BUS	0		/* Highest EISA bus number. 	     */

#define	EISA_IO_BUS_0	(512 * 1024)
/* define EISA_IO_BUS_1 (576 * 1024) if it exists. */

	/* 8259 offsets. */
#define	MASTER_MASK		0x21
#define SLAVE_MASK		0xA1
#define	MASTER_OCW		0x20
#define	SLAVE_OCW		0xA0
#define	READ_IRR		0x0A
#define	MASTER_EDGE		0x4D0
#define	SLAVE_EDGE		0x4D1

#endif
