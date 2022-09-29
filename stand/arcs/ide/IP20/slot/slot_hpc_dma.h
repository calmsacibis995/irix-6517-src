#define	DATASIZE 64 

#define MS1			1000		/* 1 ms */
#define	SCSI_DELAY		(1 * 1000)	/* 1 sec */


/*
	for slot0 HPC 0x1f9nxxxx where n=1xxx
	for slot1 HPC 0x1f9nxxxx where n=0xxx
*/

#define SCSI_BC_OFFSET 		0x88	/* SCSI byte count register */
#define SCSI_CBP_OFFSET 	0x8c	/* SCSI current buffer pointer */
#define SCSI_NBDP_OFFSET	0x90	/* SCSI next buffer pointer */
#define SCSI_CTRL_OFFSET	0x94	/* SCSI control register */
#define SCSI_PNTR_OFFSET	0x98	/* SCSI control register */

#define SCSI_CTRL_RESET	0x01		/* reset WD33C93 chip */
#define SCSI_CTRL_FLUSH	0x02		/* flush SCSI buffers */
#define SCSI_CTRL_DMA_DIR	0x10		/* transfer in */
#define SCSI_CTRL_STRTDMA 0x80		/* start SCSI dma transfer */


#define SCSI_COUNT_SHIFT  16 /* used in setting count in PNTR */

/* memory descriptor structure 
 */


struct md {
    unsigned int bc;
    unsigned int  cbp;
    unsigned int nbdp;
};
