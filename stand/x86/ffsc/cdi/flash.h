/*
** =======================================================================
** DERIVED FROM...
**
**  Header Name     :   if00xsa.hpp
**
**  Class Leniage   :   RAMROMStoreage
**
**  Class Name      :   IntelFlashFileStorage
**
**  Description     :   A storage class that uses a Intel FlashFile
**                      Flash EPROM devices to store data.
**
**  Author          :   H. Michael Tavares
**
**  Creation Date   :   May 24, 1996
**
**  Compiler Env.   :   Borland C++
**
**                    (c) Copyright Lines Unlimited
**                        1996 All Rights Reserved
**
** BY JEFF BECKER, SGI
** =======================================================================
*/

/*************************************************
**
**  Object Headers
**
**************************************************/



#ifndef INTELFLASHFILESTORAGE
#define INTELFLASHFILESTORAGE

#define FLASH_ENABLE_DEFAULT    0x550;
#define FLASH_VPP_ON            0xff;
#define FLASH_VPP_OFF           0x00;

/*
**  was IntelFlashFileStorage Class
*/

struct flashStruct {
  int           flashVPP;       /* The flash power port */
  int           deviceOpen;     /* Flag to signal device is ready for action */
  int           numDevices;     /* Number of on-board flash devices */
  unsigned long position;       /* Current position in flash storage */
  unsigned      blockSize;      /* Size of blocks in bytes */
  int           deviceSocket;   /* The starting socket of the map */
  int           pageSelectPort; /* Page select I/O port */
  int           VGAOn;          /* VGA enable port */
  int           VGAOff;         /* VGA disable port */
  unsigned      windowSegment;  /* Absolute Ram/Rom window start segment */
  int           deviceSize;     /* Device size in K */
  int           *SGISocket;    /* Map Data Arrays */
  int           *SGIFlash;
};

#define BLOCK_SIZE_128      128
#define BLOCK_SIZE_512      512
#define BLOCK_SIZE_1K       1024
#define BLOCK_SIZE_8K       8  * (1024)
#define BLOCK_SIZE_32K      32 * (1024)

#define SIZE_128K   (128L * 1024L)

#define RAM_VGA_ENABLE    0x540
#define RAM_VGA_DISABLE   0x548
#define CMD_READ 	      0xff
#define CMD_WRITE	      0x40

#define PAGE_SELECT 0x760


#define READ_MODE       1
#define WRITE_MODE      2

#define LOOP_DOWN       10000
#define RETRY_LIMIT     100

#define NUMDEVICES      2
#define INTEL_BLOCKS    16
#define CDI_BLOCKS      32

#define CMD_READ        0xff
#define CMD_IDENT       0x90
#define CMD_READ_STATUS 0x70
#define CMD_CLR_STATUS  0x50
#define CMD_ERASE_SET   0x20
#define CMD_ERASE_CONF  0xd0
#define CMD_ERASE_SUSP  0xb0
#define CMD_ERASE_RESU  0xd0
#define CMD_WRITE       0x40
#define CMD_WRITE_ALT   0x10

#define WRITE_OK			0
#define CLEAR_STATUS_ERROR	 	-10
#define WRITE_STATUS_READY_ERROR	-11
#define WRITE_ERROR			-12
#define DATA_NOT_EQUAL_ERROR		-13

/*
**  Status constants
*/
#define STATUS_OK           			0
#define STATUS_INVALID_ACCESS_SPECIFIER    	-1 
#define STATUS_OPEN_ERROR  			-2
#define STATUS_FILE_NOT_OPEN			-3
#define STATUS_READ_ERROR			-4
#define STATUS_READ_ONLY			-5
#define STATUS_IDENTIFIER_ERROR			-6
#define STATUS_REGISTER_ERROR			-7
#define STATUS_ERASE_ERROR			-8
#define STATUS_OPEN_WINDOW_ERROR		-9

/* function prototypes */

void initFlash( struct flashStruct * );
int openFlash( struct flashStruct *, int, int );
int closeFlash(struct flashStruct * );
int readFlash( struct flashStruct *, char *, unsigned );
int writeFlash( struct flashStruct *, char *, unsigned );
#endif
