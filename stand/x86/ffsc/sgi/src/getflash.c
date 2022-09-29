/* xmodrflash.c - receive a flash image over a serial line and flash the ffsc */

#include <vxworks.h>
#include <iolib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tasklib.h>

#include "ffsc.h"
#include "timeout.h"
#include "flash.h"
#include "xfer.h"

/* This #define causes us to flash blocks into RAM as soon as they */
/* are received. If not defined, everything is read into a large   */
/* buffer and flashed all at once after the transfer is complete.  */
#define FLASH_AS_YOU_GO
#define XFER_BUF_SIZE	(400*1024)


/*   CRC-16 constant array...						*/
/*   from Usenet contribution by Mark G. Mendel, Network Systems Corp.	*/
/*   (ihnp4!umn-cs!hyper!mark)						*/

/* CRCTab as calculated by initcrctab() */
unsigned short CRCTab[1<<B] = { 
    0x0000,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
    0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
    0x1231,  0x0210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
    0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
    0x2462,  0x3443,  0x0420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
    0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
    0x3653,  0x2672,  0x1611,  0x0630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
    0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
    0x48c4,  0x58e5,  0x6886,  0x78a7,  0x0840,  0x1861,  0x2802,  0x3823,
    0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
    0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,  0x0a50,  0x3a33,  0x2a12,
    0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
    0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,  0x0c60,  0x1c41,
    0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
    0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0x0e70,
    0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
    0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
    0x1080,  0x00a1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
    0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
    0x02b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
    0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
    0x34e2,  0x24c3,  0x14a0,  0x0481,  0x7466,  0x6447,  0x5424,  0x4405,
    0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
    0x26d3,  0x36f2,  0x0691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
    0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
    0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,  0x08e1,  0x3882,  0x28a3,
    0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
    0x4a75,  0x5a54,  0x6a37,  0x7a16,  0x0af1,  0x1ad0,  0x2ab3,  0x3a92,
    0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
    0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,  0x0cc1,
    0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
    0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,  0x0ed1,  0x1ef0
};


void diffbuf(unsigned char *MemBuf, long Size)
{
	unsigned char *FlashBuf;
	long Offset;
	struct flashStruct ffscFlash;

	FlashBuf = malloc(32*1024);
	if (FlashBuf == NULL) {
		ffscMsg("malloc2 failed");
		return;
	}

	initFlash(&ffscFlash);
	if (openFlash(&ffscFlash, READ_MODE, -1) != 0) {
		ffscMsg("openFlash failed");
		free(FlashBuf);
		return;
	}

	for (Offset = 0;  Offset < Size; Offset += 32*1024) {
		int CmpSize;

		if (readFlash(&ffscFlash, FlashBuf, 32*1024) != 0) {
			ffscMsg("readFlash failed");
			free(FlashBuf);
			closeFlash(&ffscFlash);
			return;
		}

		if ((Size - Offset) < 32*1024) {
			/*CmpSize = (Size - Offset);*/
			/* Don't bother with last block, it is incomplete */
			/* if it was done by the serial downloader	  */
			break;
		}
		else {
			CmpSize = 32*1024;
		}
		if (memcmp(FlashBuf, MemBuf + Offset, CmpSize) != 0) {
			ffscMsg("Buffers do not compare at offset %ld!!",
				Offset);
			taskSuspend(0);
		}
		else {
			ffscMsg("Buffers DO compare!!");
		}
	}

	closeFlash(&ffscFlash);

	free(FlashBuf);
	return;
}


void writebuf(unsigned char *MemBuf, long Size)
{
	long Offset;
	struct flashStruct ffscFlash;

	initFlash(&ffscFlash);
	if (openFlash(&ffscFlash, WRITE_MODE, -1) != 0) {
		ffscMsg("openFlash failed");
		return;
	}

	for (Offset = 0;  Offset < Size; Offset += 32*1024) {
		int WriteSize;

		if ((Size - Offset) < 32*1024) {
			WriteSize = (Size - Offset);
		}
		else {
			WriteSize = 32*1024;
		}

		if (writeFlash(&ffscFlash, MemBuf + Offset, WriteSize) != 0) {
			ffscMsg("writeFlash failed");
			closeFlash(&ffscFlash);
			return;
		}
		else {
			ffscMsg("Wrote %d bytes at offset %ld",
				WriteSize, Offset);
		}
	}

	closeFlash(&ffscFlash);
	return;
}
	

int getflash(struct flashStruct *ffscFlash, int fd, int FeedbackFD) {

  int CRC, i, nread, ntmouts, still_to_read;
  int curPktNum = 0;
  int datalen;
  int nerrors = 0;
  int packetlen;
  int ReturnValue = -1;
  xmodem_1k_crc_packet_t packet;
  unsigned char hdrchar, CRCHi, CRCLo;
  unsigned char *pkt_bufptr;

#ifndef FLASH_AS_YOU_GO

  unsigned char *BigBuf;
  long Offset;

  Offset = 0L;
  BigBuf = malloc(XFER_BUF_SIZE);
  if (BigBuf == NULL) {
	  ffscMsg("malloc failed");
	  return -1;
  }
#endif

  ioctl(fd, FIORFLUSH, 0);

  ffscMsg("\r\nTransferring firmware image");
  if (FeedbackFD >= 0) {
    fdprintf(FeedbackFD, "\r\nTransferring firmware image\r\n");
  }

  ntmouts = 0;
  do {
    if (ntmouts == ffscTune[TUNE_XFER_MAX_TMOUT]) {
      ffscMsg("Way too many timeouts!");
      goto Done;
    }    
    hdrchar = SOH_CRC;
    if ((write(fd, &hdrchar, 1)) != 1) {
      ffscMsg("Error writing to file descriptor");
      goto Done;
    }
    nread = timeoutRead(fd, &packet, 
			sizeof(xmodem_1k_crc_packet_t), 10000000);
    if (nread == -1) {
      if (errno == EINTR)
	ntmouts++;
      else {
	ffscMsg("Got error in timeoutRead");
	goto Done;
      }
    }
    else if (nread == 0) {
      ffscMsg("EOF reading XMODEM packet");
      goto Done;
    }
  } while (nread == -1);

  while (1) {                        /* xmodem receive loop */

    if (ffscDebug.Flags & FDBF_XMODEM) {
      ffscMsg("Got %d from timeoutRead", nread);
    }

    if (packet.Header == STX) {
      datalen = PACKET_LEN_1K;
      packetlen = sizeof(xmodem_1k_crc_packet_t);
    }
    else {
      datalen = PACKET_LEN;
      packetlen = sizeof(xmodem_crc_packet_t);
    }

    if (nread != packetlen) {
      switch (packet.Header) {
      case EOT:
	hdrchar = ACK;
	if ((write(fd, &hdrchar, 1)) != 1){
	  ffscMsg("Error writing to file descriptor");
	  goto Done;
	}
	ffscMsg("\r\nImage download complete");

#ifndef FLASH_AS_YOU_GO
	ffscMsg("Writing image to flash storage");
	writebuf(BigBuf, Offset);
	diffbuf(BigBuf, Offset);
#endif

	if (FeedbackFD >= 0) {
	  write(FeedbackFD, "\r\n\r\n", 4);
	}

	ReturnValue = 0;                  /* SUCCESS !!! */
	goto Done;
	/*NOTREACHED*/

      case CAN:
	ffscMsg("Got CAN - aborting");
	goto Done;
	/*NOTREACHED*/

      default:                  /* timeout in middle of packet */
	ntmouts = 0;
	still_to_read = packetlen - nread;
	if (ffscDebug.Flags & FDBF_XMODEM) {
	  ffscMsg("In default case, ntmouts is %d, still_to_read is %d",
		  ntmouts,still_to_read);
	}
	pkt_bufptr = ((unsigned char *) (&packet)) + nread;
	do {
	  if (ntmouts == ffscTune[TUNE_XFER_MAX_TMOUT]) {
	    ffscMsg("Timeout reading partial packet");
	    goto Done;
	  }
	  nread = timeoutRead(fd, pkt_bufptr, still_to_read, 1000000);
	  if (ffscDebug.Flags & FDBF_XMODEM) {
	    ffscMsg("Got %d from partial packet timeoutRead", nread);
	  }
	  if (nread == 0) {
	    ffscMsg("EOF from partial packet timeoutRead");
	    goto Done;
	  }
	  else if (nread < 0) {
	    if (errno == EINTR) {
	      ntmouts++;
	    }
	    else {
	      ffscMsg("Got error in timeoutRead");
	      goto Done;
	    }
	  }
	  else {
	    still_to_read -= nread;
	    pkt_bufptr += nread;
	  }
	} while (still_to_read > 0);
	break;
      }
    }
                                              /* process packet */
/*    for (i=0; i< 8; i++)
      ffscMsg("Byte %d: %d", i, *(((char *) (&packet)) + i)); */
    if (packet.Header != SOH  &&  packet.Header != STX) {
      ffscMsg("Bad packet header %d", packet.Header);
      nerrors++;
      goto Nack_Pkt;
    }
    if (packet.PacketNum + packet.PacketNumOC != 0xff) {
      ffscMsg("Bad packet number checksum: %x/%x",
	      packet.PacketNum, packet.PacketNumOC);
      nerrors++;
      goto Nack_Pkt;
    }
    if (packet.PacketNum != ((curPktNum + 1) & 0xff)) {
      ffscMsg("Bad packet number");
      nerrors++;
      goto Nack_Pkt;
    }
    CRC = 0;                                /* compute/check CRC */
    for (i = 0;  i < datalen;  ++i) {
      int index;
      
      index = (CRC >> (W-B)) ^ packet.Packet[i];
      CRC = ((CRC << B) ^ CRCTab[index]) & 0xffff;
    }
    
    /* Fill in the CRC */
    CRCHi = (CRC >> 8) & 0xff;
    CRCLo = CRC & 0xFF;
    if (packet.Header == STX) {
	    if ((CRCHi != packet.CRCHi) || (CRCLo != packet.CRCLo)) {
		    ffscMsg("Bad packet CRC: is %x/%x  should be %x/%x",
			    packet.CRCHi, packet.CRCLo, CRCHi, CRCLo);
		    nerrors++;
		    goto Nack_Pkt;
	    }
    }
    else {
	    xmodem_crc_packet_t *Short = (xmodem_crc_packet_t *) &packet;

	    if ((CRCHi != Short->CRCHi) || (CRCLo != Short->CRCLo)) {
		    ffscMsg("Bad packet CRC: is %x/%x  should be %x/%x",
			    Short->CRCHi, Short->CRCLo, CRCHi, CRCLo);
		    nerrors++;
		    goto Nack_Pkt;
	    }
    }
	    
    curPktNum = packet.PacketNum;
    hdrchar = ACK;
    if ((write(fd, &hdrchar, 1)) != 1) {                  /* ACK the packet */
      ffscMsg("Error writing to file descriptor");
      goto Done;
    }
    

#ifdef FLASH_AS_YOU_GO
    writeFlash(ffscFlash, packet.Packet, datalen);
#else
    bcopy(packet.Packet, BigBuf+Offset, datalen);
    Offset += datalen;
#endif

    if (FeedbackFD >= 0) {
      write(FeedbackFD, ".", 1);
    }

    /* write(2, packet.Packet, PACKET_LEN);*/
    nread = timeoutRead(fd, &packet, sizeof(xmodem_1k_crc_packet_t), 10000000);
    if (nread == -1) {
      if (errno == EINTR)
	ntmouts++;
      else {
	ffscMsg("Got error in timeoutRead");
	goto Done;
      }
    }
    continue;
    
  Nack_Pkt:
    if (nerrors == ffscTune[TUNE_XFER_MAX_ERROR]) {
      ffscMsg("Too many errors!");
      goto Done;
    }
    else {
      hdrchar = NAK;
      if ((write(fd, &hdrchar, 1)) != 1) {                /* NAK the packet */
	ffscMsg("Error writing to file descriptor");
	goto Done;
      }
      nread = timeoutRead(fd, &packet, sizeof(xmodem_1k_crc_packet_t),
			  10000000);
      if (nread == -1) {
        if (errno == EINTR)
	  ntmouts++;
	else {
	  ffscMsg("Got error in timeoutRead");
	  goto Done;
	}
      }
    }
  }

Done:

	ffscMsg("Number of Flash Packet Errors: %d", nerrors);
	ffscMsg("Number of Flash Packet Timeouts: %d", ntmouts);
  /* Come here and clean up before exiting */
#ifndef FLASH_AS_YOU_GO
  free(BigBuf);
#endif
  return ReturnValue;
}
