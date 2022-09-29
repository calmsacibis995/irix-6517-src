#ifndef __EISA_DMA_H__
#define __EISA_DMA_H__

#ident $Revision: 1.3 $

#define	NOT_SHARED	0
#define	SHARED		1

#define	TIMING_ISA	0
#define	TIMING_TYPE_A	1
#define	TIMING_TYPE_B	2
#define	TIMING_TYPE_C	3

#define	SIZE_BIT_8	0
#define	SIZE_BIT_16	1
#define SIZE_BIT_32	2

#define	MODE_DEMAND	0
#define	MODE_SINGLE	1
#define	MODE_BLOCK	2
#define	MODE_CASCADE	3

typedef enum {
	Read, 
	Write,
	Verify
} TRANSFER_TYPE;

typedef struct dma_transfer_t {
	ULONG		TransferMode;	/* demand, single, block, cascade. */
	ULONG		ChannelNumber;  /* 0 - 7			   */
	BOOLEAN		Shared;		/* 0 = Not shared.		   */
	TRANSFER_TYPE	TransferType;	/* Read, Write or Verify.	   */
	ULONG		Size;		/* Number of bytes to transfer.	   */
	PVOID		Buffer;		/* Address of buffer.		   */
} DMA_TRANSFER, *PDMA_TRANSFER;


typedef	struct dma_status_t {
	BOOLEAN	CompleteTransfer;
	PVOID	LastByteTransferred;
} DMA_STATUS, *PDMA_STATUS;

#endif
