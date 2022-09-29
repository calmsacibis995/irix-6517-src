#include <assert.h>
#include "fb.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

/* Structure and macros used to manage generation of Intel hex records */
typedef struct{
  int           dirty;		/* Record structure has been written */
  unsigned char _data[32];	/* Record buffer */
  FILE*         file;		/* Output file */
  unsigned long lastELA;	/* Last ELA record address written */
} IntelHexRec_t;

#define bcnt  _data[0]
#define rtype _data[3]

#define emitData(r,c) r->_data[4+r->bcnt++] = c
#define emitAddr(r,a) r->_data[1] = ((a)>>8) & 0xff; r->_data[2] = (a) & 0xff
#define emitType(r,t) r->_data[3] = t


/*************
 * flushRecord	Finish and flush the record
 */
void
flushRecord(IntelHexRec_t* ihr){
  int         i;
  signed char xsum = 0;
  char*       hex="0123456789ABCDEF";
  char        buf[60];

  if (!ihr->dirty)
    return;
  
  for (i=0;i<ihr->bcnt+4;i++){
    signed char d = ihr->_data[i];

    xsum     += d;
    buf[i*2]  = hex[d>>4 & 0xf];
    buf[i*2+1]= hex[d & 0xf]; 
  }
  xsum = -xsum;
  buf[(ihr->bcnt+4)*2]   = hex[xsum>>4 & 0xf];
  buf[(ihr->bcnt+4)*2+1] = hex[xsum    & 0xf];
  buf[(ihr->bcnt+4)*2+2] = '\0';
  fprintf(ihr->file,":%s\n",buf);

  ihr->dirty = 0;
  ihr->bcnt  = 0;
}


/*********
 * emitELA
 */
void
emitELA(IntelHexRec_t* ihr,unsigned long addr){
  assert(ihr->bcnt==0);
  ihr->dirty = 1;
  emitAddr(ihr,0);
  emitType(ihr,4);
  emitData(ihr, (addr>>24) & 0xff);
  emitData(ihr, (addr>>16) & 0xff);
  flushRecord(ihr);
}


/********
 * format	Format data record
 */
void
format(IntelHexRec_t* ihr, unsigned long addr, long word){
  if (ihr->bcnt==0){
    emitAddr(ihr,addr&0xffff);
    emitType(ihr,0);
  }
  assert(ihr->rtype == 0);
  ihr->dirty = 1;

  emitData(ihr,(word>>24)&0xff);
  emitData(ihr,(word>>16)&0xff);
  emitData(ihr,(word>> 8)&0xff);
  emitData(ihr,(word    )&0xff);

  if (ihr->bcnt==16)
    flushRecord(ihr);
}


/*********
 * emitEOF
 */
void
emitEOF(IntelHexRec_t* ihr){
  assert(ihr->bcnt==0);
  ihr->dirty = 1;
  emitAddr(ihr,0);
  emitType(ihr,1);
  flushRecord(ihr);
}

 
/**************
 * writeHexFile		Write out an image of the FLASH PROM as hex records
 */
void
writeHexFile(FbState_t* fbs){
  IntelHexRec_t ihr;
  int           len = fbs->end - fbs->image;
  int           addr;

  memset(&ihr,0,sizeof(ihr));

  /* Open output file */
  if ((ihr.file = fopen(fbs->args->hexFileName,"wb") ) == 0){
    printf(" unable to open image file %s \n", fbs->args->imageFileName);
    return;
  }

  
  /*
   * Process every byte in the image
   * (Note: this means we are writing out some bytes that are empty due to
   *  alignment.  It doesn't seem worth detecting and working around that.)
   *
   * Note: fbs->args->flashBase is the base address of the flash used for
   *       generating HEX records.  HEX records may be read into a device
   *       programmer in which case the addresses are relative to the 
   *       flash's internal addresses (i.e. 0,1,2,3,...), or records
   *       may be read into the platform itself in which case the addresses
   *       are platform relative, typically bfc00000, bfc00001,...
   */
  for (addr = 0; addr<len;addr+=4){
    unsigned long eaddr = fbs->args->flashBase + addr;
    if ((eaddr & 0xffff0000) != (ihr.lastELA & 0xffff0000)){
      flushRecord(&ihr);
      emitELA(&ihr,eaddr);
      ihr.lastELA = eaddr;
    }
    format(&ihr,eaddr,*(long*)(fbs->image+addr));
  }
  flushRecord(&ihr);
  emitEOF(&ihr);
  fclose(ihr.file);
}





