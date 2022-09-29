#include "common.h"
#include "cdrom.h"
#include "cye_mdebug.h"

#define min(a,b) ((a) < (b) ? (a) : (b))





int cdrom_blocksize = 512;



int
cd_get_blksize(void)
{
        return(cdrom_blocksize);
}



int
cd_set_blksize(FSBLOCK *fsb, int blocksize)
{

	/* if there's really a need to change */
	if(blocksize != cdrom_blocksize) {

		int blksize = blocksize;

		fsb->IO->FunctionCode = FC_IOCTL;
		fsb->IO->IoctlCmd = (IOCTL_CMD) (__psint_t)DIOCSETBLKSZ;
		fsb->IO->IoctlArg = (IOCTL_ARG) &blksize;
		if(fsb->DeviceStrategy(fsb->Device, fsb->IO)) {
			printf("cd_set_blksize() ERROR : DIOCSETBLKSZ\n");
			return(1);
		}

		cdrom_blocksize = blocksize;
	}
        return(0);
}



LONG
cd_voldesc(void)
{
	/* default value -- 16 * 2048 seems to be correct */
	return (LONG) 16 * CDROM_BLKSIZE;
}



/*
** cd_read()
**
** read count bytes from offset into buf -- buf doesn't have to be DMA
** alligned (since much of the iso.c code from mountcd is used, this
** was easier)
**
** we should also look into larger transfers in section II and maybe
** checking for allignment to avoid the dmabuf_malloc()/bcopy()
** step
*/

int
cd_read(FSBLOCK *fsb, LONG offset, void *buf, LONG count)
{
	char *cbuf;
	char *data = buf;
	IOBLOCK *iob = fsb->IO;
	char *original_address = fsb->IO->Address;
	LONG original_iob_offset = fsb->IO->Offset.lo;
	unsigned int slop, wegot, num_read, cdblksize=cd_get_blksize();

	fsb->IO->Offset.lo = 0;
#ifdef DEBUG
	printf("\ncd_read(%d,%d,%d) : blksz=%d offset=%d, count=%d buf=0x%x\n",
	iob->Controller, iob->Unit, iob->Partition, cdblksize, offset, count,
	buf);
#endif
	if(count <= 0) {
		fsb->IO->Offset.lo = original_iob_offset;
		return(iob->ErrorNumber = EIO);
	}

	if(buf == NULL) {
		printf("cd_read() ERROR : invalid (NULL) buffer\n");
		return(iob->ErrorNumber = EIO);
	}

	/*
	** I : read that portion of the beginning that is not aligned on a block
	**     boundary -- slop is how far into the block we need to start
	*/
	slop = offset % cdblksize;

	if (slop) {
		iob->StartBlock = offset / cdblksize;
		iob->Count = cdblksize;
		if(NULL == (cbuf = (char *)
		cye_dmabuf_malloc(BBTOB(BTOBB(iob->Count))))) {
			printf("cd_read() ERROR : dmabuf_malloc()\n");
			fsb->IO->Offset.lo = original_iob_offset;
			return(iob->ErrorNumber = ENOMEM);
		}
		iob->Address = cbuf;
#ifdef DEBUG
		printf("cd_read() I : at block %d for %d bytes into 0x%x\n",
		iob->StartBlock, iob->Count, iob->Address);
#endif
		wegot = DEVREAD(fsb);
		if(wegot != iob->Count) {
#ifdef DEBUG
			printf("cd_read() : I : DEVREAD() failed %d/%d\n",
			wegot, iob->Count);
#endif
			cye_dmabuf_free(cbuf);
			iob->Address = original_address;
			fsb->IO->Offset.lo = original_iob_offset;
			return(iob->ErrorNumber = EIO);
		}
		num_read = min(cdblksize-slop, count);
		bcopy(cbuf+slop, data, num_read);
		count -= num_read;
		data += num_read;
		offset += num_read;
		cye_dmabuf_free(cbuf);
	}

	/*
	** II : read the block aligned middle portion
	**
	**      since this is the only place where the count can be huge, we
	**      break it up into cdblksize DEVREAD() calls
	*/
	if (count / cdblksize) {
		LONG left;

		left = (count / cdblksize) * cdblksize;
		iob->Count =  cdblksize;

		while(left) {
			iob->StartBlock = offset / cdblksize;

			if(NULL == (cbuf = (char *)
			cye_dmabuf_malloc(BBTOB(BTOBB(iob->Count))))) {
				printf("cd_read() ERROR : dmabuf_malloc()\n");
				iob->Address = original_address;
				fsb->IO->Offset.lo = original_iob_offset;
				return(iob->ErrorNumber = ENOMEM);
			}
			iob->Address = cbuf;
#ifdef DEBUG
			printf("cd_read() II : at block %d for %d bytes into 0x%x\n",
			iob->StartBlock, iob->Count, iob->Address);
#endif
			wegot = DEVREAD(fsb);
			if(wegot != iob->Count) {
#ifdef DEBUG
				printf("cd_read() : II : DEVREAD() failed %d/%d\n",
				wegot, iob->Count);
#endif
				cye_dmabuf_free(cbuf);
				iob->Address = original_address;
				fsb->IO->Offset.lo = original_iob_offset;
				return(iob->ErrorNumber = EIO);
			}
			bcopy(cbuf, data, cdblksize);
			cye_dmabuf_free(cbuf);
			count -= cdblksize;
			data += cdblksize;
			offset += cdblksize;
			left -= cdblksize;
		}
	}

	/*
	** III : read that portion of an end that is not aligned on a block
	**       boundary
	*/
	if (count) {
		iob->StartBlock = offset / cdblksize;
		iob->Count = cdblksize;
		if(NULL == (cbuf = (char *)
		cye_dmabuf_malloc(BBTOB(BTOBB(iob->Count))))) {
			printf("cd_read() ERROR : dmabuf_malloc()\n");
			iob->Address = original_address;
			fsb->IO->Offset.lo = original_iob_offset;
			return(iob->ErrorNumber = ENOMEM);
		}
		iob->Address = cbuf;
#ifdef DEBUG
		printf("cd_read() III : at block %d for %d bytes into 0x%x\n",
		iob->StartBlock, iob->Count, iob->Address);
#endif
		wegot = DEVREAD(fsb);
		if(wegot != iob->Count) {
#ifdef DEBUG
			printf("cd_read() : III : DEVREAD() failed %d/%d\n",
			wegot, iob->Count);
#endif
			cye_dmabuf_free(cbuf);
			iob->Address = original_address;
			fsb->IO->Offset.lo = original_iob_offset;
			return(iob->ErrorNumber = EIO);
		}
		bcopy(cbuf, data, count);
		cye_dmabuf_free(cbuf);
	}

	iob->Address = original_address;
	fsb->IO->Offset.lo = original_iob_offset;
#ifdef DEBUG
	printf("cd_read() OKAY\n");
#endif
	return (0);
}
