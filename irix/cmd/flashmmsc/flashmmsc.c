/*
 *
 * $Id: flashmmsc.c,v 1.9 1999/02/24 18:37:34 sasha Exp $
 *
 * flashmmsc
 *	- Flash firmware onto a Lego Multi-Module System Controller
 * 
 * This code has undergone a minor rewrite in order to support the "-a"
 * (automatic) upgrade option. When using the "-a" option, note that only
 * Origin 2000 (rack/multirack) systems support auto-flashing of MMSC 
 * firmware.
 */

#define kudzu

#ident  "$Revision: 1.9 $"
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/serialio.h>
#include <sys/capability.h>
#include <termios.h>
#include <ctype.h>
#include <stdlib.h>

/* Local MMSC IO routines, thanks Curt! */
#include "mmsc.h"
#include "double_time.h"
#include "error.h"
#include "oobmsg.h"

#define DTO        10        /* Timeout value for a normal data read */
#define MAX_MMSC   100       /* Maximum number of supported MMSC in system */
#define MMSC_VERSION_SIZE 25
#define OOB_BUFSZ 1024
#define MSC_FIRMWARE_CURRENT 3.1

/* MSC/MMSC information structure */
typedef struct {
  int    speed;                        /* Speed this unit supports */
  int    hwfc;                         /* Hardware flow control 1=on, 0=off*/
  int    rackNo;                       /* Rack Number */
  double revNo;                        /* Real revisions number */
  char   locn;                         /* U for upper, L for lower */
  char   version[MMSC_VERSION_SIZE];   /* Firmware version string */
} sc_info;

/* MSC daemon start/stop commands */
#define MSC_STOP_CMD   "/etc/init.d/sn0start stop"
#define MSC_START_CMD  "/etc/init.d/sn0start start"

/* Command issued to MMSC to get a prompt */
#define CNTRL_T      CTRL('t')

/* OOB stuff */
#define OBP_CHAR     '\xa0'
#define MMSC_COMMAND 61
#define STATUS_NONE  0


/* MMSC firmware version information constants */
#define TARGET_STRING "MMSC VERSION "    /* A string we expect to find in the
					   MMSC firmware image file right 
					   before the actual version string */
#define TARGET_LENGTH   13               /* Length of the above string 
					    including spaces, but not including
					    the terminating \0 */



/* XModem stuff */
#define PACKET_LEN	128		/* Normal packet length */
#define PACKET_LEN_1K	1024		/* XMODEM-1K packet length */

/* Xmodem CRC packet */
typedef struct xmodem_crc_packet {
	char	Header;		       /* Header byte */
	char	PacketNum;	       /* Packet number */
	char	PacketNumOC;	       /* One's complement of packet number */
	char	Packet[PACKET_LEN];    /* The data itself */
	char	CRCHi;		       /* High order byte of CRC */
	char	CRCLo;		       /* Low order byte of CRC */
} xmodem_crc_packet_t;

/* Xmodem 1K CRC packet */
typedef struct xmodem_1k_crc_packet {
	char	Header;			/* Header byte */
	char	PacketNum;		/* Packet number */
	char	PacketNumOC;		/* One's complement of packet number */
	char	Packet[PACKET_LEN_1K];	/* The data itself */
	char	CRCHi;			/* High order byte of CRC */
	char	CRCLo;			/* Low order byte of CRC */
} xmodem_1k_crc_packet_t;

#define SOH	'\001'		/* Start Of Header */
#define SOH_CRC	'C'		/* Start Of Header for XMODEM-CRC */
#define STX	'\002'		/* Start of Text (XMODEM-1K packet) */
#define EOT	'\004'		/* End Of Transmission */
#define EOT_STR "\004"
#define ACK	'\006'		/* Positive Acknowledgement */
#define NAK	'\025'		/* Negative Acknowledgement */
#define CAN	'\030'		/* Cancel */
#define CAN_STR	"\030"
#define PAD	'\032'		/* ^Z for padding */



/* Constants */
#define CANCEL_NUM_CANS		2	/* # of CAN's to send on cancel */

#define DOT_INTERVAL		8	/* Packets/dot in DIRECT mode */

#define MAX_EOTS		5	/* Max # of resends of EOT */
#define MAX_RETRIES		20	/* Max # of resends after NAK */

#define MODE_NONE		0	/* No mode specified */
#define MODE_AUTOMATIC		1	/* Automatic transfer */
#define MODE_MANUAL		2	/* Manual transfer */
#define MODE_DIRECT		3	/* Direct transfer to serial port */
#define MODE_PROBE              4       /* Just probe the serial links */
#define MODE_VERSION            5       /* Try to determine firmware's version */

#define RUPT_TIMEOUT		1	/* alarm expired */
#define RUPT_QUIT		2	/* Received quit signal */

#define TIMEOUT_ACK		10	/* Wait for ACK on packet */
#define TIMEOUT_DOWNLOAD_READY  500	/* Wait for receiver is ready byte */
#define TIMEOUT_EOT_ACK		15	/* Wait for ACK on final EOT */
#define TIMEOUT_SECOND_CAN	3	/* Wait for 2nd CAN of CAN-CAN */
#define LEN_64K                 64*1024 /* 64K */
#define FFSC_CMD_OP             0x3d    /* OOB data message opcode */

/*   CRC-16 constants.  From Usenet contribution by Mark G. Mendel,	*/
/*   Network Systems Corp.  (ihnp4!umn-cs!hyper!mark)			*/
#define	P	0x1021		/* the CRC polynomial. */
#define W	16		/* number of bits in CRC */
#define B	8		/* the number of bits per char */


/* Default values */
#define DFLT_IMAGEFILE	"/usr/cpu/firmware/mmscfw.bin"
#ifdef kudzu
#define DFLT_SERIALDEV  "/dev/ttyd1"
#else
#define DFLT_SERIALDEV  "/dev/ttyd2"
#endif
#define DFLT_CONSOLEDEV "/dev/ttyd1"
#define DFLT_MMSC_DEV   "/hw/machdep/mmsc_control"

/* Global variables : inherited from original code.*/
char	ErrMsg[160];
FILE	*LogFile = NULL;
int	RuptReason;
int	SerialFD;
int     SerialCommandFD;
char    *DevFile = DFLT_SERIALDEV;
struct termios OriginalTermIO;
extern unsigned short CRCTab[1<<B];	/* (defined below) */
char *FileName = DFLT_IMAGEFILE;
int  Verbose = 0;
char FileBuffer[PACKET_LEN_1K];

/* 
 * For auto mode, we use the following.
 */
sc_info* mmscInfo = NULL;
sc_info* mscInfo = NULL;
int mmCount = 0;          /* Number of MMSC found in system */
int mCount = 0;           /* Number of MSC found in system */


/* Function declarations */
void AbortTransfer(int, const char *);
void Alarm(int);
void Interrupt(int);
void Log(const char *, ...);
unsigned char OOBCheckSum(unsigned char *);
int  ReadWithTimeout(int, char *, int, int,int);
double extract_float_from_string(char* str);
void cleanup(void);
void add_sc_info(sc_info** info, char* buf, int* cntRead, int mscScan);
int set_io_speed(int mmscID,int desiredSpeed, int currentSpeed, 
		 struct termios* termIO,int serialFD);
void alloc_info_structs(void);
void auto_transfer(int SerialFD, int FileFD, int probing);
int get_firmware_version(char* fname);



/* int get_firmware_version(char* fname) 
 *
 * 
 * Effects:  Tries to find the versions string somewhere in the 
 *           the firmware image file specified by fname. If it fails
 *           to do so it will notify the user.  Returns 0 in case of
 *           success and -1 in case of failure.
 */


int get_firmware_version(char *fname) {

  /* 2/16/99   Alexander (Sasha) Vladimirov */

  /* This works in a fairly straightforward way.  The version
   * string should be the string which immediately follows a 
   * whitespace after the TARGET_STRING sequence of characters 
   * is found.  So, first we must find the TARGET_STRING in the
   * file, and then whatever follows it must be the version 
   * string. 
   */


  int FileFD;
  char buffer[80];
  int found = 0;
  int num_read = 0;


  /* First, try to open the image file for reading. */

  if ((FileFD = open(fname, O_RDONLY)) < 0) {
    fprintf(stderr, "Unable to open image file \"%s\": %s\n",
	    fname, strerror(errno));
    return(-1);
  }

  /* Warn the user about how long this might take */
  
  printf("Searching for the version information.\n"
         "Please be patient, this might take a few minutes.\n\n");

  /* Now go through it, character by character, looking for the 
     first character of the target string */
  while ((read(FileFD, buffer, 1) > 0) && !found) {
    
    /* if we found the first character, let's see if we have the rest 
       of it */

    if (TARGET_STRING[0] == buffer[0]) {
      /* Try to read the rest of the characters */
      if ((num_read = read(FileFD, buffer+1, 79)) >= (TARGET_LENGTH-1)) {
	/* If we got them, compare them to our target string */
	if (strncmp(buffer, TARGET_STRING, TARGET_LENGTH) == 0) {
	  /* If we succedeed, time to print the actual version
	   * string. 
	   */

	  found = 1;
	  printf("The version of the MMSC firmware in the file %s is %s.\n", 
		 fname, buffer + TARGET_LENGTH);
	}

	else {
	  /* If this wasn't the string, we must back up to where we
	     were in the file before. */
	  if (lseek (FileFD, num_read * -1, SEEK_CUR) < 0) {
	    fprintf(stderr, "Error processing image file \"%s\": %s\n",
		    fname, strerror(errno));
	    close(FileFD);
	    return (-1);
	  }
	}
      }
    }
  }

  if (found) {
    close (FileFD);
    return (0);
  }

  /* If we didn't find the target string, it is probably an old 
     firmare release */

  printf("This image of the MMSC firmware does not contain"
	 " version information.\nIt is probably a verison"
	 " prior to 2.1.\n");
  close(FileFD);
  return (-1);
}






/*
 * This code will extract a real number of the form x.y from
 * a char* buffer and returns a floating point value.
 */
double extract_float_from_string(char* str)
{
  char* bufStart = 0x0;
  for(bufStart = str; *bufStart != NULL; bufStart++){
    if(isdigit(*bufStart)){
      *(bufStart + 3) = 0x0;
      return atof(bufStart);
    }
  }
  return 0.0;
}


/*
 * Routine called when we are done.
 */
void cleanup(void)
{}


/*
 * Parses a line of output from the command "r * b * ver" such
 * that we can determine where all the mmsc are in the system and
 * what version each is running.
 * When you call this routine, *sc_info should be allocated to 
 * size MAX_MMSC and the cntRead should be the current number of
 * detected MMSC in the system. If mscScan is non-zero, routine will
 * parse output from the command "r * b * msc ver" (for probing MSC's).
 */
void add_sc_info(sc_info** info, char* buf, int* cntRead, int mscScan)
{
  int i = 0,j = 0,k = 0,dc=0;
  char c = 0;
  char tmpbuf[4]; /* Won't work on system with more than 4 digits of rackno*/
  sc_info* inf = *info;
  char* ptr = buf;
  i = *cntRead;
  while(*ptr != '\0'){
    c = *ptr;
    k = 0;
    /* Read racknumber */
    if(c == 'R'){ 
      while(c != ':' && ptr !=NULL){
	c = *ptr++;
	/*
	 * The MSC has a racknumber, followed by a letter indicating
	 * whether it is upper or lower. We need both.
	 */
	if(mscScan){
	  if(isdigit(c))
	    tmpbuf[k++] = c ;
	  else if((c == 'U') || (c == 'L')){/* Read whether upper or lower. */
	    inf[i].locn = c;
	    c = *ptr++;
	  }
	} 
	/* The MMSC does not have a 'U' or 'L' prefix, so just
	 * get the number indicating the rack.
	 */
	else {
	  if(isdigit(c))
	    tmpbuf[k++] = c;
	}
      }
      inf[i].rackNo = atoi(tmpbuf);
    }
    /* Read version string: We extract a number of the form x.y 
     * where x is the major number and y the minor number.
     */
    if(c == ':'){
      j = 0;
      c = *ptr++;
      while((dc != 2)||((c !='\n') && (j < MMSC_VERSION_SIZE) && (c!= '\0'))){

	if(dc !=2)
	  inf[i].version[j++] = c;
	if(c == '.')
	  dc++;
	if(c == '\n') 
	  break;
	c = *ptr++;
      }
      inf[i].version[j] = 0;
    }
  }
  inf[i].revNo = extract_float_from_string(inf[i].version);
  *cntRead += 1;
}


/*
 * Sets the local MMSC to have a speed matching our tty's speed. 
 * This routine will set the IO speed of the local MMSC which we
 * are talking to.
 */
int set_io_speed(int mmscID,int desiredSpeed, int currentSpeed, 
		 struct termios* termIO,int serialFD)
{
  int r,status,len = 0;
  char bufIn[OOB_BUFSZ];
  char bufOut[OOB_BUFSZ];
  double expire = 0;

  /* Send command to MMSC to set its speed */
  mmsc_t* mc = mmsc_open_plus(DFLT_CONSOLEDEV,currentSpeed);  
  memset(bufIn,0x0,OOB_BUFSZ);
  memset(bufOut, 0x0,OOB_BUFSZ);
  sprintf(bufOut, "r %d com 4 speed %d hwflow N\0", mmscID, desiredSpeed);

  /* First send the command over to augment speed */
  if ((r = do_grab(mc)) < 0)
    return -1;  
  if ((r = mmsc_command(mc, FFSC_CMD_OP,
			(u_char*)bufOut,strlen(bufOut)+1)) < 0) {
    (void) do_release(mc);
    return -2;
  }
  if ((r = do_release(mc)) < 0)
    return -3;

  expire = double_time() + MMSC_TIMEOUT;
  if ((r = mmsc_response(mc, &status,(u_char*)bufIn,
			 OOB_BUFSZ, &len, expire)) < 0){
  }
  mmsc_close_plus(mc);
  free(mc);

  /* Get attributes for the serial device which we are attached to*/
  if (tcgetattr(serialFD, termIO) < 0) {
    printf("Unable to get termio info for serial line\n");
    perror("tcgetattr");
    exit(2);
  }    

  /* Finally, set our tty speeds accordingly*/
  cfsetispeed(termIO, desiredSpeed);
  cfsetospeed(termIO, desiredSpeed);
  if (tcsetattr(serialFD, TCSAFLUSH, termIO) < 0) {
    perror("tcsetattr");
    printf("Could not restore default termcap settings...\n");
    return -1;
  }

  /* 
   * Now, reopen the puppy and read all this junk so we don't
   * have the problem of xmodem transfers getting hosed.
   */
  mc = mmsc_open(DFLT_CONSOLEDEV,desiredSpeed,1);  
  /* ping MMSC to see if it's up ... */
  len = mmsc_transact(mc, OOB_NOP, 0, 0, 0, 0, 0);
  mmsc_close(mc);
  free(mc);
  printf("Speed changed on MMSC[%d] to %dbps,stat=0x%x -", 
	 mmscID, desiredSpeed, len);
  if(len == 0)
    printf("OK\n");
  else{
    printf("FAIL\n");
    exit(1);
  }

  return 0;
}

/*
 * Allocate MMSC/MSC information structures.
 */
void alloc_info_structs(void)
{
  mmCount = mCount = 0;
  /* Allocate information structs */
  mmscInfo = (sc_info*)malloc(sizeof(sc_info) * MAX_MMSC);
  if(!mmscInfo){
    printf("Could not allocate MMSC info structure.\n");
    exit(1);
  }

  mscInfo = (sc_info*)malloc(sizeof(sc_info) * MAX_MMSC);
  if(!mscInfo){
    printf("Could not allocate MSC info structure.\n");
    exit(1);
  }
  
}

/*
 * Loops through all the MMSC and flashes them one by one.
 * @@ TODO: make use HWFC, implement network worm code in MMSC firmware.
 */ 
void auto_transfer(int SerialFD, int FileFD, int probing)
{
  int  CRC;
  int  Attempts,Value;
  int  PacketNum;
  int  i,t,m,localId,len;
  int Result,ms,xs=0;
  char savedChar;
  char RcvBuffer[1];
  char oobRcvBuf[OOB_BUFSZ];
  char oobSendBuf[OOB_BUFSZ];
  char* tokenPtr = 0x0;
  struct termios TermIO;
  xmodem_1k_crc_packet_t Packet;
  mmsc_t* mc;

  /* For now, enable debug output */
  /*   debug_enable("/tmp/flashmmsc.log");*/
  printf("WARNING: If you have MMSC firmware < 1.1, the -a option will not work.\n");
  /* Kill the MMSC daemon to acquire the MMSC console/control device */
  printf("Killing MMSC daemon .... \n");
  if(system(MSC_STOP_CMD)!=0){
    printf("Could not stop MSC daemon.\n");
    exit(1);
  }

  /* Init mem */
  alloc_info_structs();

  if(probing){
    printf("Probing system MMSC configuration ....\n");
  }
  /* See if it's up ... */
  printf("Pinging MMSC...");

  /* Open MMSC device */
  mc = mmsc_open(DFLT_CONSOLEDEV, 9600,0);  
  /* ping MMSC to see if it's up ... */
  len = mmsc_transact(mc, OOB_NOP, 0, 0, 0, 0, 0);
  mmsc_close(mc);

  printf(" stat=0x%x -", len);
  if(len == 0)
    printf("OK\n");
  else{
    printf("FAIL\n");
    printf("There do not appear to be any MMSC in this system.\n");
    printf("This may be because you are running MMSC firmware revision < 1.1.\n");
    printf("Otherwise, reboot your sytem and try again.\n");
    printf("In worse case, you may have to flash manually.\n");
    exit(1);
  }
  printf("Analyzing hardware configuration...\n");

  /* Find out Local Rack ID (MMSC thereof) */
  mc = mmsc_open(DFLT_CONSOLEDEV, 9600,0);  
  memset(oobRcvBuf,0x0,OOB_BUFSZ);
  memset(oobSendBuf, 0x0,OOB_BUFSZ);
  strcpy(oobSendBuf, "r . rackid\0");
  mmsc_transact(mc,FFSC_CMD_OP, 
		(u_char*)oobSendBuf,strlen(oobSendBuf)+1,
		(u_char*)oobRcvBuf,OOB_BUFSZ,&len);

  memset(oobSendBuf, 0x0,OOB_BUFSZ);
  sprintf(oobSendBuf, "%c", oobRcvBuf[1]);
  localId = atoi(oobSendBuf);
  mmsc_close(mc);

  printf("Local MMSC is rack %d, connected via [%s] fd: 0x%x on host/IO6.\n",
	 localId,DevFile, SerialFD);
  fflush(stdout);

  /* Find all MMSC versions */
  /* Send message packet over SerialFD to probe for all MMSC's */
  mc = mmsc_open(DFLT_CONSOLEDEV, 9600,0);  
  memset(oobSendBuf,0x0,OOB_BUFSZ);
  memset(oobRcvBuf,0x0,OOB_BUFSZ);
  strcpy(oobSendBuf, "r * b * ver\0");
  mmsc_transact(mc,FFSC_CMD_OP, 
		(u_char*)oobSendBuf,strlen(oobSendBuf)+1,
		(u_char*)oobRcvBuf,OOB_BUFSZ,&len);
  mmsc_close(mc);

  /* Parse output, and add it to our array of sc_info... */
  for(i = 0,tokenPtr = oobRcvBuf; oobRcvBuf[i] !=0;i++){
    /* Commas delimit multiline output. Ready buffer for parsing */
    if(oobRcvBuf[i] == ','){
      oobRcvBuf[i] = '\n';
      savedChar = oobRcvBuf[i+1]; /*Swap char with null byte */
      oobRcvBuf[i+1] = 0;
      add_sc_info(&mmscInfo,tokenPtr, &mmCount,1);
      oobRcvBuf[i+1] = savedChar; /* Restore buffer ... */
      tokenPtr = &oobRcvBuf[i+1];
      oobRcvBuf[i] = ',';
    }
  }

  /* terminal token */
  oobRcvBuf[i] = '\n';
  oobRcvBuf[i+1] = 0;
  add_sc_info(&mmscInfo,tokenPtr, &mmCount,1); 

  if(mmCount == 0){
    printf("flashmmsc: FATAL, could not find any MMSC!\n");
    exit(1);
  } 
  else
    printf("* %d MMSC on this system.\n",mmCount);

  for(i = 0; i < mmCount; i++){
    printf("o MMSC located in rack %d (Firmware r%.2f)\n",
	   mmscInfo[i].rackNo,
	   mmscInfo[i].revNo);
  }

  fflush(stdout);
  fflush(stdout);

  /* Now probe for MSC's */
  mc = mmsc_open(DFLT_CONSOLEDEV, 9600,0);  
  memset(oobSendBuf,0x0,OOB_BUFSZ);
  memset(oobRcvBuf,0x0,OOB_BUFSZ);
  strcpy(oobSendBuf, "r * b * msc ver\0");
  mmsc_transact(mc,FFSC_CMD_OP, 
		(u_char*)oobSendBuf,strlen(oobSendBuf)+1,
		(u_char*)oobRcvBuf,OOB_BUFSZ,&len);

  mmsc_close(mc);

  /* Parse output, and add it to our array of sc_info (MSC's) ... */
  for(i = 0,tokenPtr = oobRcvBuf; oobRcvBuf[i] !=0;i++){
    if(oobRcvBuf[i] == ','){
      oobRcvBuf[i] = '\n';
      savedChar = oobRcvBuf[i+1]; /*Swap char with null byte */
      oobRcvBuf[i+1] = 0;
      add_sc_info(&mscInfo,tokenPtr, &mCount,1);
      oobRcvBuf[i+1] = savedChar; /* Restore buffer ... */
      tokenPtr = &oobRcvBuf[i+1];
      oobRcvBuf[i] = ','; 
    }
  }

  /* terminal token */
  oobRcvBuf[i] = '\n';
  oobRcvBuf[i+1] = 0;
  add_sc_info(&mscInfo,tokenPtr, &mCount,1); 

  if(mCount == 0){
    printf("flashmmsc: FATAL, could not find any MSC!\n");
    exit(1);
  } 
  else
    printf("* %d MSC found on this system.\n",mCount);

  /* Report what we found */
  for(i = 0; i < mCount; i++){
    printf("o MSC located in %s rack %d (Firmware r%.2f) (%s)\n",
	   (mscInfo[i].locn == 'U') ? "UPPER" : "LOWER",
	   mscInfo[i].rackNo,
	   mscInfo[i].revNo,
	   (mscInfo[i].revNo <= 0.0) ? "Offline" : "Online");
  }


  /* 
   * Determine the maximum speed that this firmware can be flashed at.
   * Setup the hw flow control if necessary.
   * @@: TODO, add code to set flow control.
   */
  for(m = 1; m <= mmCount; m++){
    if(mmscInfo[m-1].revNo >= 1.1){
      /* Only supports 115,200 bps max xfer */
      mmscInfo[m-1].speed = 115200;
      printf("* MMSC[%d] (version %.2f) supports 115K FLASH upload.\n",
	     m,mmscInfo[m-1].revNo);
    } 
    else if(mmscInfo[m-1].revNo < 1.1){
      /* Well, it only supports 38,400 bps max xfer, but that doesn't work
       * and we need to do it at 9600 :-(
       */
      /*       mmscInfo[m-1].speed = 38400; */
      mmscInfo[m-1].speed = 9600;
      printf("* MMSC[%d] (version %.2f) only supports 57.6K FLASH upload.\n",
	     m,mmscInfo[m-1].revNo);
    } 
    else {
      mmscInfo[m-1].speed = 9600;
      printf("* MMSC[%d] (version %.2f) only supports 9600bps FLASH upload.\n"
	     ,m,mmscInfo[m-1].revNo);
    }
  }
  fflush(stdout);

  /* get terminal characteristics */
  if (tcgetattr(SerialFD, &TermIO) < 0) {
    printf("Unable to get termio info for serial line\n");
    perror("tcgetattr");
    exit(2);
  }    

  /* Save old termcap ... */
  OriginalTermIO = TermIO;

  /* Now, since we have collected all of this perfect data about MMSC's
   * we will go into our upload loop, sending the new image to each one
   * of them (in lockstep).
   *
   */
  if(!probing){

    /* Now, we have perfect information about the system configuration
     * and the serial link is about to be maxxed out. So, loop through all the
     * MMSC's and flash each one. But first, we must set the speed on the 
     * local MMSC to the fastest speed it supports, if it's greater than
     * than the normal speed of 9600.
     */

    if (mmscInfo[localId-1].speed > 9600) {
      set_io_speed(localId, mmscInfo[localId-1].speed,9600,&TermIO,SerialFD);

    }

    for(m = 1; m <= mmCount; m++){  
    
      /* Note: array indices 0-based, but rack numbers are 1-based */
      if(mmscInfo[m-1].revNo <= 0.0){
	printf("MMSC %d does not have a version I understand (%.2f)\n",
	       m,mmscInfo[m-1].revNo);
	printf("Skipping MMSC[%d]...\n", m);
	continue;
      } 
      else
	printf("MMSC[%d] (Firmware r%.2f) (starting upgrade).\n",m,
	       mmscInfo[m-1].revNo);
      
      /* Open the image file we are going to FLASH this pup with ... */
      FileFD = open(FileName, O_RDONLY);
      if (FileFD < 0) {
	fprintf(stderr, "Unable to open image file \"%s\": %s\n",
		FileName, strerror(errno));
	exit(2);
      }


     /* 
      * Now, tell the MMSC to initiate a transfer.
      */
      memset(oobRcvBuf,0x0,OOB_BUFSZ);
      memset(oobSendBuf, 0x0,OOB_BUFSZ);
      sprintf(oobSendBuf, "r %d flash",m);

      mc = mmsc_open(DFLT_CONSOLEDEV, 9600,1);        
      mmsc_transact(mc,FFSC_CMD_OP, 
		    (u_char*)oobSendBuf,strlen(oobSendBuf)+1,
		    (u_char*)oobRcvBuf,OOB_BUFSZ,&len);

      /* 
       * VERY IMPORTANT: close the MMSC device so that the console
       *                 device does not escape characters. If this
       *                 does not happen, your xmodem upload WILL FAIL!!
       */
      mmsc_close(mc);
      free(mc);


      /* XMODEM: Upload time. Now that we have closed the MMSC device,
       *         we can open the file descriptor for the console device
       *         and put it in raw mode, sending the data ala XModem 1K
       *
       * Get terminal characteristics again, (they have changed) 
       */
      if (tcgetattr(SerialFD, &TermIO) < 0) {
	printf("Unable to get termio info for serial line\n");
	printf("You will have to reflash MMSC %d manually.\n",m);
	perror("tcgetattr");
	exit(2);
      }    
      
	/* Now, setup the new settings for raw-mode to upload the image */
      TermIO.c_iflag &= ~(BRKINT|ISTRIP|ICRNL|INLCR|IGNCR|IXON|IXOFF);
      TermIO.c_oflag &= ~(OPOST|TABDLY);
      TermIO.c_lflag &= ~(ISIG|ICANON|ECHO);
      TermIO.c_cc[VMIN]  = 1;
      TermIO.c_cc[VTIME] = 1;
      
      /* Finally, put the terminal in raw mode */  
      if (tcsetattr(SerialFD, TCSAFLUSH, &TermIO) < 0) {
	perror("Unable to set termio info for serial line.\n");
	printf("You will need to flash MMSC %d manually!", m);
	exit(2);
      }
      
      /*
       * VERY IMPORTANT:
       * It takes 95 seconds (about) for the FLASH to be cleared out
       * on the remote end. This is why we have a large value for 
       * TIMEOUT_DOWNLOAD_RETRY. If the TIMEOUT_DOWNLOAD retry is less
       * than about 4 minutes, we often will run into failures
       *
       * NOTE:
       * From this point on, we cannot send anything to the console 
       * other than XModem upload data. To do so would destroy the 
       * transmission of valid XModem data and fill the console with
       * garbage. Weeeeh! What fun!!! 
       */
      do {
	Result =ReadWithTimeout(SerialFD,RcvBuffer, 
				1,TIMEOUT_DOWNLOAD_READY,0);
	if (Result < 0) {
	  AbortTransfer(-1, ErrMsg);
	  /*NOTREACHED*/
	}
	if (Result == 0) {
	  if (RuptReason == RUPT_TIMEOUT) {
	    AbortTransfer(SerialFD, 
			  "Timed out waiting for transfer to begin");
	    /*NOTREACHED*/
	  }
	  else {
	    AbortTransfer(-1, "Transfer cancelled");
	  }
	}
      } while (*RcvBuffer != SOH_CRC);  
      
      /* Start pumping data across */
      PacketNum = 1;
      Result = read(FileFD, FileBuffer, PACKET_LEN_1K);
      while (Result > 0) {
	
	/* Build the front of the packet */
	Packet.Header      = STX;
	Packet.PacketNum   = PacketNum & 0xFF;
	Packet.PacketNumOC = ~Packet.PacketNum;
	
      /* Pad the file data if we don't have a complete packet */
	if (Result < PACKET_LEN_1K) 
	  for (i = Result;  i < PACKET_LEN_1K; ++i) 
	    FileBuffer[i] = PAD;
	
	/* 
	 * Copy over the bytes in the next packet, calculating     
	 * the CRC as we do so.					
	 */
	CRC = 0;
	for (i = 0;  i < PACKET_LEN_1K;  ++i) {
	  int index;
	  Packet.Packet[i] = FileBuffer[i];
	  index = (CRC >> (W-B)) ^ FileBuffer[i];
	  CRC = ((CRC << B) ^ CRCTab[index]) & 0xffff;
	}
	/* Fill in the CRC */
	Packet.CRCHi = (CRC >> 8) & 0xff;
	Packet.CRCLo = CRC & 0xFF;
	/* Attempt to transfer the packet to the other side */
	Attempts = 0;
	do {
	  /* Write the buffer to stdout */
	  Result = write(SerialFD, &Packet, sizeof(Packet));
	  if (Result < 0) {
	    sprintf(ErrMsg, "Write to remote side failed: %s",
		    strerror(errno));
	    AbortTransfer(-1, ErrMsg);
	  }
	  if (Result != sizeof(Packet)) {
	    sprintf(ErrMsg, "Incomplete write (%d bytes)", Result);
	    AbortTransfer(SerialFD, ErrMsg);
	  }
	
#ifdef FLUSH_INPUT
	  /* Flush any input */
	  Value = FREAD;
	  if (ioctl(SerialFD, TIOCFLUSH, &Value) < 0) {
	    sprintf(ErrMsg, "TIOCFLUSH failed; %s",strerror(errno));
	    AbortTransfer(SerialFD, ErrMsg);
	  }
#endif
	  /* Say what we have done so far if desired */	  
	  if (Verbose) {
	    Log("Wrote packet %d  CRC 0x%02x%02x",Packet.PacketNum,
		Packet.CRCHi, Packet.CRCLo);
	    for (i=0; i< 8; i++)
	      Log("Byte %d: %d\n", i, *(((char *) (&Packet)) + i));

	  }

	  /* Increment the number of attempts */
	  ++Attempts;

	  /* Wait for an acknowledgement */
	  *RcvBuffer = '\0';
	  Result = ReadWithTimeout(SerialFD,RcvBuffer,1,TIMEOUT_ACK,0);
	  if (Result < 0) {
	    /* ReadWithTimeout filled in ErrMsg */
	    AbortTransfer(SerialFD, ErrMsg);
	  }
	  else if (Result == 0) {
	    /* Interrupt occurred */
	    if (RuptReason == RUPT_TIMEOUT) {
	      Log("Timeout on ACK, attempt %d", Attempts);
	      continue;
	    }
	    else if (RuptReason == RUPT_QUIT) {
	      AbortTransfer(SerialFD, "Transfer cancelled");
	      /*NOTREACHED*/
	    }
	    else {
	      AbortTransfer(SerialFD, "Unexpected interrupt");
	      /*NOTREACHED*/
	    }
	  }

	  /* Process the response character */
	  switch (*RcvBuffer) {	  
	  case CAN:
	    if (ReadWithTimeout(SerialFD,RcvBuffer,1,
				TIMEOUT_SECOND_CAN,0)==1 && *RcvBuffer==CAN){
	      AbortTransfer(-1, "Transfer cancelled by receiver");
	      /*NOTREACHED*/
	    }
	    else {
	      Log("Discarding single CAN");
	    }
	    break;
	  
	  case NAK:
	    Log("NAK on attempt %d of packet %d",
		Attempts, Packet.PacketNum);
	    break;
		  
	  case ACK:
	    if (Verbose) {
	      Log("ACK on attempt %d of packet %d", 
		  Attempts, Packet.PacketNum);
	    }
	    break;
	    
	  default:
	    Log("Ignoring non-ACK '%c' (%d)", *RcvBuffer, *RcvBuffer);
	    break;
	  }
	  
	} while (*RcvBuffer != ACK  &&  Attempts < MAX_RETRIES);
	
	/* Croak if we exceeded the max # of retries */
	if (Attempts >= MAX_RETRIES) {
	  AbortTransfer(SerialFD, "Too many errors");
	  /*NOTREACHED*/
	}
      
	/* Read the next batch of data from the file */
	++PacketNum;
	Result = read(FileFD, FileBuffer, PACKET_LEN_1K);
      }
    

      /* We have apparently transferred the entire file. Now send
       * an EOT character and wait for one last ACK back from the MMSC.
       */
      if (Verbose) {
	Log("Sent entire file, negotiating EOT");
      }

      for (i = 0;  i < MAX_EOTS;  ++i) {
	if (write(SerialFD, EOT_STR, 1) != 1) {
	  sprintf(ErrMsg, "Unable to send final EOT: %s",	
		  strerror(errno));
	  AbortTransfer(SerialFD, ErrMsg);
	  /*NOTREACHED*/
	}
      
	Result = ReadWithTimeout(SerialFD, RcvBuffer, 1, TIMEOUT_EOT_ACK,0);
	if (Result < 0) {
	  /* ReadWithTimeout filled in ErrMsg for us */
	  AbortTransfer(SerialFD, ErrMsg);
	  /*NOTREACHED*/
	}
	if (Result == 0) {
	  if (RuptReason == RUPT_TIMEOUT) {
	    Log("Timed out waiting for EOT-ACK, attempt %d",i);
	  }
	  else {
	    Log("Interrupted waiting for EOT-ACK");
	  }
	}
	else if (*RcvBuffer != ACK) {
	  Log("Received non-ACK on EOT: '%c' (%d)",
	      *RcvBuffer, *RcvBuffer);
	}
	else {
	  /* We got our ACK */
	  break;
	}
      }
      
      /* Make sure we didn't run out of attempts */
      if (Attempts >= MAX_EOTS) {
	Log("Never received ACK for final EOT");
      }
      /* Done sending file, close it. */
      close(FileFD); 

      if(m == mmCount){
	/* FIRST: Tell the local MMSC to set its speed back to 9600 */
	memset(oobRcvBuf,0x0,OOB_BUFSZ);
	memset(oobSendBuf, 0x0,OOB_BUFSZ);
	sprintf(oobSendBuf, "r %d com 4 speed 9600 hwflow N",localId);
	mc = mmsc_open(DFLT_CONSOLEDEV, 9600,1);        
	mmsc_transact(mc,FFSC_CMD_OP, 
		    (u_char*)oobSendBuf,strlen(oobSendBuf)+1,
		      (u_char*)oobRcvBuf,OOB_BUFSZ,&len);
	mmsc_close(mc);
	free(mc);

	/* Then, restore the terminal settings on this device */
	if (tcsetattr(SerialFD, TCSAFLUSH, &OriginalTermIO) < 0) {
	  perror("tcsetattr");
	  printf("Could not restore default termcap settings...\n");
	  exit(-1);
	}
      }

      /* Finished MMSC numbered "m", say that now that we have cons back. */
      printf("New image [%s] transferred to MMSC[%d]:",FileName,m);
      printf("rebooting ...:");

      /* 
       * Reboot MMSC
       */
      memset(oobRcvBuf,0x0,OOB_BUFSZ);
      memset(oobSendBuf, 0x0,OOB_BUFSZ);
      sprintf(oobSendBuf, "r %d reset_mmsc\0",m);
      mc = mmsc_open(DFLT_CONSOLEDEV, 9600,1);        
      mmsc_transact(mc,FFSC_CMD_OP, 
		    (u_char*)oobSendBuf,strlen(oobSendBuf)+1,
		    (u_char*)oobRcvBuf,OOB_BUFSZ,&len);
      /* Now, wait about 30 seconds */
      sleep(40);

      /* Ping it to be sure it's back up */
      len = mmsc_transact(mc, OOB_NOP, 0, 0, 0, 0, 0);
      mmsc_close(mc);      
      free(mc); /* VERY IMPORTANT */
      if(len == 0)
	printf("OK\n");
      else{
	printf("BAD!\n Reflash MMSC[%d] manually using -m option...\n",m);
	exit(-1);
      }

#if 0
      /*
       * You just flashed the localId, since it is now running a more 
       * recent version, we should reverify the versions and recompute
       * the speeds so the rest of the upgrade will go faster. 
       */
      if((mmCount > 1) && (m == localId)){
	mmCount = 0;
	free(mmscInfo);
	  /* Reallocate MMSC information struct */
	mmscInfo = (sc_info*)malloc(sizeof(sc_info) * MAX_MMSC);
	if(!mmscInfo){
	  printf("Could not allocate MMSC info structure, reflash.\n");
	  exit(1);
	}
	printf("Recomputing MMSC versions to accelerate upgrade ...\n");
	/* Find all MMSC versions */
	/* Send message packet over SerialFD to probe for all MMSC's */
	mc = mmsc_open(DFLT_CONSOLEDEV, 9600,0);  
	memset(oobSendBuf,0x0,OOB_BUFSZ);
	memset(oobRcvBuf,0x0,OOB_BUFSZ);
	strcpy(oobSendBuf, "r * b * ver\0");
	mmsc_transact(mc,FFSC_CMD_OP, 
		      (u_char*)oobSendBuf,strlen(oobSendBuf)+1,
		      (u_char*)oobRcvBuf,OOB_BUFSZ,&len);
	mmsc_close(mc);
	free(mc);
	/* Parse output, and add it to our array of sc_info... */
	for(i = 0,tokenPtr = oobRcvBuf; oobRcvBuf[i] !=0;i++){
	  /* Commas delimit multiline output. Ready buffer for parsing */
	  if(oobRcvBuf[i] == ','){
	    oobRcvBuf[i] = '\n';
	    savedChar = oobRcvBuf[i+1]; /*Swap char with null byte */
	    oobRcvBuf[i+1] = 0;
	    add_sc_info(&mmscInfo,tokenPtr, &mmCount,1);
	    oobRcvBuf[i+1] = savedChar; /* Restore buffer ... */
	    tokenPtr = &oobRcvBuf[i+1];
	    oobRcvBuf[i] = ',';
	  }
	}

	/* terminal token */
	oobRcvBuf[i] = '\n';
	oobRcvBuf[i+1] = 0;
	add_sc_info(&mmscInfo,tokenPtr, &mmCount,1); 
	
	if(mmCount == 0){
	  printf("flashmmsc: FATAL, could not find any MMSC!\n");
	  exit(1);
	} 
	else
	  printf("* %d MMSC on this system.\n",mmCount);

	for(i = 0; i < mmCount; i++){
	  printf("o MMSC located in rack %d (Firmware r%.2f)\n",
		 mmscInfo[i].rackNo,
		 mmscInfo[i].revNo);
	}
	fflush(stdout);

	for(i = 1; i <= mmCount; i++){
	  if(mmscInfo[i-1].revNo >= 1.1){
	    /* Only supports 115,200 bps max xfer */
	    mmscInfo[i-1].speed = 115200;
	    printf("MMSC[%d] (version %.2f) supports 115K FLASH upload.\n",
		   i,mmscInfo[i-1].revNo);
	  } 
	  else if(mmscInfo[i-1].revNo < 1.1){
	    /* Only supports 57,600 bps max xfer */
	    mmscInfo[i-1].speed = 57600;
	    printf("MMSC[%d] (version %.2f) supports 57.6K FLASH upload.\n",
		   i,mmscInfo[i-1].revNo);
	  } 
	  else {
	    mmscInfo[i-1].speed = 9600;
	    printf("MMSC[%d] (version %.2f) supports 9600bps FLASH upload.\n",
		   i,mmscInfo[i-1].revNo);
	  }
	}
	fflush(stdout);
      } /* End speed recomputation */
#endif
      /* Do the next one! */
    }
    printf("Your system firmware has been upgraded on rack(s)[");
    for(m = 1; m <= mmCount; m++){
      if(m != mmCount)
	printf("%d,",m);
      else
	printf("%d",m);
    }
    printf("]\n");
    printf("Now, I will fix your flow control as necessary...\n");
    /* Now, if you have MSC's which are running Firmware <= 3.1, you need
     * to set com{2,3} HWFLOW N. The new version of MSC firmware will not
     * have HWFLOW set to yes by default.
     */
    mc = mmsc_open(DFLT_CONSOLEDEV, 9600,1);  
    
    for(i = 0; i < mCount; i++){
      if(mscInfo[i].revNo > 0.0 && mscInfo[i].revNo <= MSC_FIRMWARE_CURRENT){
	printf("* HWFLOW OFF: MSC located in %s rack %d (Firmware r%.2f) (%s):",
	       (mscInfo[i].locn == 'U') ? "UPPER" : "LOWER",
	       mscInfo[i].rackNo,
	       mscInfo[i].revNo,
	       (mscInfo[i].revNo <= 0.0) ? "Offline" : "Online");
	
	/* Upper MSC is connected to MMSC via com2; lower = com3  */
	if(mscInfo[i].locn == 'U')
	  t = 2;
	else 
	  t = 3; 
	
	/* Set com 2/3 HWFLOW off for this rack */
      memset(oobRcvBuf,0x0,OOB_BUFSZ);
      memset(oobSendBuf, 0x0,OOB_BUFSZ);
      sprintf(oobSendBuf, "rack %d com %d hwflow n\0", 
	      mscInfo[i].rackNo,t);
      
      /*Tell MMSC to set HWFLOW off */
      len = mmsc_transact(mc, FFSC_CMD_OP,
			  (u_char*)oobSendBuf,strlen(oobSendBuf)+1,
			  (u_char*)oobRcvBuf,OOB_BUFSZ,&len);
      printf("%s\n",oobRcvBuf);
      }
    }
    mmsc_close(mc);
    free(mc);

    printf("Disco...\n");
    printf("Your MMSC firmware on all racks has now been upgraded.\n");
  } /* Not probing */
  /* Close tty */
  close(SerialFD);
}


int main(int argc, char **argv)
{
  int  ArgError = 0;
  int  Attempts;
  int  CRC;
  int  FileFD;
  int  i;
  int  Mode = MODE_PROBE;
  int  PacketNum;
  int  RackID = -1;
  int  Result;
  int  Value;
  char RcvBuffer[1];
  char Version[80];
  struct termios TermIO;
  xmodem_1k_crc_packet_t Packet;
  int done = 0;
  char Command[80];
  char Buffer[80];
  cap_t		ocap;
  cap_value_t		cap_device_mgt = CAP_DEVICE_MGT;
  

  /* Parse command line arguments */
	for (i = 1;  i < argc;  ++i) {
		if (strcmp(argv[i], "-a") == 0) {
			Mode = MODE_AUTOMATIC;
		}
		else if (strcmp(argv[i], "-d") == 0) {
			Mode = MODE_DIRECT;
		}
		else if (strcmp(argv[i], "-p") == 0) {
			Mode = MODE_PROBE;
		}
		else if (strcmp(argv[i], "-f") == 0) {
			++i;
			if (i == argc) {
				fprintf(stderr,
					"%s: Must specify filename with -f\n",
					argv[0]);
				ArgError = 1;
			}
			else {
				FileName = argv[i];
			}
		}
		else if (strcmp(argv[i], "-l") == 0) {
			++i;
			if (i == argc) {
				fprintf(stderr,
					"%s: Must specify filename with -l\n",
					argv[0]);
				ArgError = 1;
			}
			else {
				DevFile = argv[i];
			}
		}
		else if (strcmp(argv[i], "-L") == 0) {
			++i;
			if (i == argc) {
				fprintf(stderr,
					"%s: Must specify filename with -L\n",
					argv[0]);
				ArgError = 1;
			}
			else {
				LogFile = fopen(argv[i], "w");
				if (LogFile == NULL) {
					fprintf(stderr,
						"%s: Unable to open log file "
						    "\"%s\"\n",
						argv[0], argv[i]);
					ArgError = 1;
				}
				else {
					setbuf(LogFile, NULL);
				}
			}
		}
		else if (strcmp(argv[i], "-m") == 0) {
			Mode = MODE_MANUAL;
		}
		else if (strcmp(argv[i], "-r") == 0) {
			++i;
			if (i == argc) {
				fprintf(stderr,
					"%s: Must specify rack ID with -r\n",
					argv[0]);
				ArgError = 1;
			}
			else {
				RackID = atoi(argv[i]);
			}
		}
		else if (strcmp(argv[i], "-v") == 0) {
			Verbose = 1;
		}
		else if (strcmp(argv[i], "-V") == 0) {
		  Mode = MODE_VERSION;
		}
		else {
			fprintf(stderr,
				"%s: Don't recognize option \"%s\"\n",
				argv[0], argv[i]);
			ArgError = 1;
		}
	}

	/* Make sure we got all the arguments we need */
	if (FileName == NULL) {
		fprintf(stderr,
			"%s: Must specify the image to be flashed\n",
			argv[0]);
		ArgError = 1;
	}


	/* If the user wants to know the version of the firmware, do just
	 * that and leave.
	 */

	if (Mode == MODE_VERSION) {
	  if (get_firmware_version(FileName)) {
	    exit (1);
	  }  
	  exit (0); 
	}

	  
	if (Mode == MODE_NONE) { 
		fprintf(stderr,
			"%s: Must specify a mode option (e.g. -a, -m)\n",
			argv[0]);
		ArgError = 1;
	}

	/* If we got an argument error, bail out */
	if (ArgError) {
		fprintf(stderr,
			"\nUsage: %s {-a|-m|-p} [-f <filename>] [-v] \n"
			"         %s -d -l <serial> [-f <filename>] \n"
			"         %s -V [-f <filename>] \n"

			"    -a indicates automatic download initiation\n"
			"    -d indicates direct download to serial port\n"
			"    -l specifies a serial device name\n"
			"    -m indicates manual download initiation\n"
			"    -f specifies the file containing the image to "
			            "be flashed\n"
			"    -p indicates probe-only mode\n"
			"    -v turns on verbose output\n"
			"    -V reports the firmware version number\n",

			argv[0], argv[0], argv[0]);
		exit(1);
	}

	/* Show what options we got if desired */
	if (Verbose) {
		printf("Mode: %d   File: %s  Serial: %s\n",
		       Mode, FileName, DevFile);
	}


	/* Open the image file */
	if((Mode != MODE_AUTOMATIC) && (Mode != MODE_PROBE)){
	  
	  FileFD = open(FileName, O_RDONLY);
	  if (FileFD < 0) {
	    fprintf(stderr, "Unable to open image file \"%s\": %s\n",
			FileName, strerror(errno));
	    exit(2);
	  }
	}

	/* Open the serial file if necessary */
	if ((Mode == MODE_DIRECT)){
	  if (DevFile == NULL) {
	    fprintf(stderr,
		    "Must specify -l option with -d\n");
	    exit(1);
	  }
		
	  SerialFD = open(DevFile, O_RDWR);
	  if (SerialFD < 0) {
	    fprintf(stderr,
		    "Unable to open serial device %s: %s\n",
		    DevFile, strerror(errno));
	    exit(1);
	  }
	  printf("SerialFD = 0x%x\n", SerialFD);
	}
	else {
		SerialFD = 0;
	}

	/* Arrange to trap SIGINT, etc. */
	if (signal(SIGINT, Interrupt) == SIG_ERR  ||
	    signal(SIGHUP, Interrupt) == SIG_ERR)
	{
		fprintf(stderr,
			"Unable to set SIGINT/SIGHUP handler: %s\n",
			strerror(errno));
		exit(2);
	}

	/* If we are in "manual" mode, tell the operator they need to	*/
	/* start things up on the MMSC side.				*/
	if (Mode == MODE_MANUAL) {
	  printf("\nReady to transfer new image to full-feature system\n"
		 "controller. To begin the transfer, type your MMSC\n"
		 "escape character (normally CTRL-T) followed by the\n"
		 "command:\n"
		 "\n"
		 "    rack <rackid> flash\n"
		 "\n"
		 "where <rackid> is the identifier for the system\n"
		 "controller you wish to upgrade.\n");
	}

	/* 
	 * Auto mode: flash all MMSC attached to private network.
	 */
	if (Mode == MODE_AUTOMATIC) {
	  auto_transfer(SerialFD, FileFD,0);
	  exit(0);
	}

	/* 
	 * Report all MMSC on system 
	 */
	if(Mode == MODE_PROBE){
	  auto_transfer(SerialFD, FileFD,1);
	  exit(0);                    /* +---- probe flag */
	}


	/* 
	 * Normal terminal I/O doesn't work beyond this point, except	
	 * in DIRECT mode. If we are in AUTOMATIC mode, tell the MMSC 
	 * to initiate a transfer.
	 */

	/* Put the terminal in raw mode */
	if (tcgetattr(SerialFD, &TermIO) < 0) {
	  printf("Unable to get termio info for serial line\n");
	  perror("tcgetattr");
	  exit(2);
        }

	OriginalTermIO = TermIO;

        TermIO.c_iflag &= ~(BRKINT|ISTRIP|ICRNL|INLCR|IGNCR|IXON|IXOFF);
        TermIO.c_oflag &= ~(OPOST|TABDLY);
        TermIO.c_lflag &= ~(ISIG|ICANON|ECHO);
        TermIO.c_cc[VMIN]  = 1;
        TermIO.c_cc[VTIME] = 1;

	if (Mode == MODE_DIRECT) {
		TermIO.c_cflag &= ~(CSIZE | PARENB);
		TermIO.c_cflag |= CS8;
		cfsetospeed(&TermIO, B19200);
	}

	if (tcsetattr(SerialFD, TCSAFLUSH, &TermIO) < 0) {
                perror("Unable to set termio info for serial line");
                exit(2);
        }

	/* If we are in DIRECT mode, send over a "U" to indicate we want */
	/* to upload an image to the MMSC.				 */
	if (Mode == MODE_DIRECT) {
		write(SerialFD, "u", 1);
	}

	/* Wait for the other side to say it is ready to begin */
	if (Mode == MODE_DIRECT) {
		printf("Waiting for MMSC to initiate transfer...\n");
	}
	do {
		Result = ReadWithTimeout(SerialFD,
					 RcvBuffer,
					 1,
					 TIMEOUT_DOWNLOAD_READY,0);
		if (Result < 0) {
			AbortTransfer(-1, ErrMsg);
			/*NOTREACHED*/
		}
		if (Result == 0) {
			if (RuptReason == RUPT_TIMEOUT) {
				AbortTransfer(SerialFD,
					      "Timed out waiting for transfer "
					          "to begin");
				/*NOTREACHED*/
			}
			else {
				AbortTransfer(-1, "Transfer cancelled");
			}
		}
	} while (*RcvBuffer != SOH_CRC);

	/* Say we are about to begin if desired */
	if (Mode == MODE_DIRECT) {
		printf("MMSC is ready, now downloading image\n");
	}

	/* Start pumping data across */
	PacketNum = 1;
	Result = read(FileFD, FileBuffer, PACKET_LEN_1K);
	while (Result > 0) {

		/* Build the front of the packet */
		Packet.Header	   = STX;
		Packet.PacketNum   = PacketNum & 0xFF;
		Packet.PacketNumOC = ~Packet.PacketNum;

		/* Pad the file data if we don't have a	complete packet */
		if (Result < PACKET_LEN_1K) {
			for (i = Result;  i < PACKET_LEN_1K; ++i) {
				FileBuffer[i] = PAD;
			}
		}

		/* Copy over the bytes in the next packet, calculating	*/
		/* the CRC as we do so.					*/
		CRC = 0;
		for (i = 0;  i < PACKET_LEN_1K;  ++i) {
			int index;

			Packet.Packet[i] = FileBuffer[i];

			index = (CRC >> (W-B)) ^ FileBuffer[i];
			CRC = ((CRC << B) ^ CRCTab[index]) & 0xffff;
		}

		/* Fill in the CRC */
		Packet.CRCHi = (CRC >> 8) & 0xff;
		Packet.CRCLo = CRC & 0xFF;

		/* Attempt to transfer the packet to the other side */
		Attempts = 0;
		do {

			/* Write the buffer to stdout */
			Result = write(SerialFD, &Packet, sizeof(Packet));
			if (Result < 0) {
				sprintf(ErrMsg,
					"Write to remote side failed: %s",
					strerror(errno));
				AbortTransfer(-1, ErrMsg);
			}
			if (Result != sizeof(Packet)) {
				sprintf(ErrMsg,
					"Incomplete write (%d bytes)",
					Result);
				AbortTransfer(SerialFD, ErrMsg);
			}

#ifdef FLUSH_INPUT
			/* Flush any input */
			Value = FREAD;
			if (ioctl(SerialFD, TIOCFLUSH, &Value) < 0) {
				sprintf(ErrMsg,
					"TIOCFLUSH failed; %s",
					strerror(errno));
				AbortTransfer(SerialFD, ErrMsg);
			}
#endif

			/* Say what we have done so far if desired */
			if (Verbose) {
				Log("Wrote packet %d  CRC 0x%02x%02x",
				    Packet.PacketNum,
				    Packet.CRCHi, Packet.CRCLo);
			}
			if (Mode == MODE_DIRECT  &&
			    PacketNum % DOT_INTERVAL == 0)
			{
				printf(".");
				fflush(stdout);
			}

			/* Increment the number of attempts */
			++Attempts;

			/* Wait for an acknowledgement */
			*RcvBuffer = '\0';
			Result = ReadWithTimeout(SerialFD,
						 RcvBuffer,
						 1,
						 TIMEOUT_ACK,0);
			if (Result < 0) {
				/* ReadWithTimeout filled in ErrMsg */
				AbortTransfer(SerialFD, ErrMsg);
			}
			else if (Result == 0) {
				/* Interrupt occurred */
				if (RuptReason == RUPT_TIMEOUT) {
					Log("Timeout on ACK, attempt %d",
					    Attempts);
					continue;
				}
				else if (RuptReason == RUPT_QUIT) {
					AbortTransfer(SerialFD,
						      "Transfer cancelled");
					/*NOTREACHED*/
				}
				else {
					AbortTransfer(SerialFD,
						      "Unexpected interrupt");
					/*NOTREACHED*/
				}
			}

			/* Process the response character */
			switch (*RcvBuffer) {

			    case CAN:
				if (ReadWithTimeout(SerialFD,
						    RcvBuffer,
						    1,
						    TIMEOUT_SECOND_CAN,0) == 1 &&
				    *RcvBuffer == CAN)
				{
					AbortTransfer(-1,
						      "Transfer cancelled by "
						          "receiver");
					/*NOTREACHED*/
				}
				else {
					Log("Discarding single CAN");
				}
				break;


			    case NAK:
				Log("NAK on attempt %d of packet %d",
				    Attempts, Packet.PacketNum);
				break;


			    case ACK:
				if (Verbose) {
					Log("ACK on attempt %d of packet %d",
					    Attempts, Packet.PacketNum);
				}
				break;


			    default:
				Log("Ignoring non-ACK '%c' (%d)",
				    *RcvBuffer, *RcvBuffer);
				break;
			}

		} while (*RcvBuffer != ACK  &&  Attempts < MAX_RETRIES);

		/* Croak if we exceeded the max # of retries */
		if (Attempts >= MAX_RETRIES) {
			AbortTransfer(SerialFD, "Too many errors");
			/*NOTREACHED*/
		}

		/* Read the next batch of data from the file */
		++PacketNum;
		Result = read(FileFD, FileBuffer, PACKET_LEN_1K);
	}

	/* Time for another progress report */
	if (Mode == MODE_DIRECT) {
		printf("\nDownload complete, disconnecting from MMSC...\n");
	}

	/* We have apparently transferred the entire file. Now send */
	/* an EOT character and wait for one last ACK.		    */
	if (Verbose) {
		Log("Sent entire file, negotiating EOT");
	}
	for (i = 0;  i < MAX_EOTS;  ++i) {
		if (write(SerialFD, EOT_STR, 1) != 1) {
			sprintf(ErrMsg, 
				"Unable to send final EOT: %s",
				strerror(errno));
			AbortTransfer(SerialFD, ErrMsg);
			/*NOTREACHED*/
		}

		Result = ReadWithTimeout(SerialFD,
					 RcvBuffer,
					 1,
					 TIMEOUT_EOT_ACK,0);
		if (Result < 0) {
			/* ReadWithTimeout filled in ErrMsg for us */
			AbortTransfer(SerialFD, ErrMsg);
			/*NOTREACHED*/
		}
		if (Result == 0) {
			if (RuptReason == RUPT_TIMEOUT) {
				Log("Timed out waiting for EOT-ACK, "
				        "attempt %d",
				    i);
			}
			else {
				Log("Interrupted waiting for EOT-ACK");
			}
		}
		else if (*RcvBuffer != ACK) {
			Log("Received non-ACK on EOT: '%c' (%d)",
			    *RcvBuffer, *RcvBuffer);
		}
		else {
			/* We got our ACK */
			break;
		}
	}

	/* Make sure we didn't run out of attempts */
	if (Attempts >= MAX_EOTS) {
		Log("Never received ACK for final EOT");
	}

	/* Tidy up */
	close(FileFD);
	if (Verbose) {
		Log("File transfer complete");
	}

	/* Wait a moment to get back our terminal, then say goodbye */
	if (Mode != MODE_DIRECT) {
		sleep(1);

		if (tcsetattr(SerialFD, TCSAFLUSH, &OriginalTermIO) < 0) {
			perror("Unable to restore termio info for serial "
			           "line");
		}
	}
	else {
		close(SerialFD);
	}

	printf("\n\nNew image transferred to MMSC\n");

	exit(0);
}


/*
 * AbortTransfer
 *	Come here if aborting the transfer for any reason. The transfer
 *	is formally cancelled (if possible) by sending several CANCEL
 *	characters to the receiver, then the specified error message is
 *	printed on stderr.
 */
void
AbortTransfer(int FD, const char *ErrMsg)
{
	int i;

	if (FD >= 0) {
		for (i = 0;  i < CANCEL_NUM_CANS;  ++i) {
			write(FD, CAN_STR, 1);
		}
	}

        if (tcsetattr(SerialFD, TCSAFLUSH, &OriginalTermIO) < 0) {
                perror("Unable to restore termio info for serial line");
        }

	if (ErrMsg != NULL) {
		Log(ErrMsg);
		fprintf(stderr, "\n%s\n", ErrMsg);
	}

	exit(3);
}


/*
 * Alarm
 *	Simple signal handler for SIGALRM. All it does at this stage
 *	is return, but at least it will have kicked us out of read/write.
 */
void
Alarm(int SigNum)
{
	RuptReason = RUPT_TIMEOUT;

	return;
}


/*
 * Interrupt
 *	Simple signal handler for SIGINT/SIGHUP. All it does at this stage
 *	is make a note of what happened.
 */
void
Interrupt(int SigNum)
{
	RuptReason = RUPT_QUIT;

	return;
}


/*
 * Log
 *	Write a message to the log file, if present
 */
void
Log(const char *Format, ...)
{
	va_list Args;

	if (LogFile == NULL) {
		return;
	}

	va_start(Args, Format);
	vfprintf(LogFile, Format, Args);
	fprintf(LogFile, "\n");
	va_end(Args);
}


/*
 * OOBCheckSum
 *	Calculate the checksum of the specified OOB message
 */
unsigned char
OOBCheckSum(unsigned char *Msg)
{
	unsigned char Sum;
	int DataLen;
	int i;

	DataLen = Msg[2] * 256 + Msg[3];
	Sum = 0;
	for (i = 1;  i < DataLen + 4;  ++i) {
		Sum += Msg[i];
	}

	return Sum;
}

/*
 * ReadLine: reads a line of text from the tty which is 
 * terminated with a newline. We store the newline in the buffer
 * and terminate it with a null character.
 */
int ReadLine(int fd, register char* ptr, int maxlen)
{
  char	c;
  int	n, rc;
  for (n = 1; n < maxlen; n++){
    if ( (rc = read(fd, &c, 1)) == 1) {
      *ptr++ = c;
      if (c == '\n') /* store newline in buffer */
	break;
    } 
    else if (rc == 0) {
      if (n == 1)
	return(0);	/* EOF, no data read */
      else
	break;		/* EOF, some data was read */
    } 
    else
      return(-1);	/* error */
  }
  *ptr = 0;

  return(n);
}



/*
 * ReadWithTimeout
 *	Like a normal read(2), except that it times out after the
 *	specified number of seconds. In this case, 0 is returned.
 *      If nlt is nonzero, we call ReadLine to fill the buffer.
 */
int
ReadWithTimeout(int FD, char *Buffer, int Len, int TimeoutSecs, int nlt)
{
	int ReadError;
	int Result;
	void (*OldHandler)(int);

	/* Arrange for a SIGALRM in due time */
	OldHandler = signal(SIGALRM, Alarm);
	if (OldHandler == SIG_ERR) {
	  sprintf(ErrMsg, "Unable to set SIGALRM handler: %s\n",
			strerror(errno));
		return -1;
	}

	alarm(TimeoutSecs);
	if(nlt){
	  Result = ReadLine(FD, Buffer,Len);
	  ReadError = errno;
	  
	} 
	else{
	  /* Read away */
	  Result = read(FD, Buffer, Len);
	  ReadError = errno;
	}
	/* Restore the old signal handler and reset the alarm */
	alarm(0);
	if (signal(SIGALRM, OldHandler) == SIG_ERR) {
		sprintf(ErrMsg,
			"Unable to restore original SIGALRM handler: %s",
			strerror(errno));
	}

	/* If the read failed due to interruption, we just timed out */
	if (Result < 0  &&  ReadError == EINTR) {
		Result = 0;
	}

	/* Save info about other failures */
	else if (Result < 0) {
		sprintf(ErrMsg, "Read failed: %s", strerror(ReadError));
	}
	return Result;
}


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

