/*
 * FLASH PROM utility
 *
 * This program takes a elf file that contains program image begining with
 * a FLASH segment header, calculates the appropriate checksums, updates the
 * header and appends a checksum.  It then generates an intel hex-32 version
 * of the file with the calculated checksums.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <flash.h>

#include <libelf.h>
#include <sym.h>

#include <unistd.h>

#define PHDRS 4
#define SCNS 30
#define PSCNS 4

typedef struct FData_{
  int         fd;		/* File descriptor */
  char*       file;		/* Name of file */
  Elf*        elf;		/* Elf descriptor */
  Elf32_Ehdr* ehdr;		/* Object file header */
  Elf32_Phdr* phdr;		/* Program file header */
  Elf_Scn*    scns[SCNS];	/* Section descriptors */
  Elf32_Shdr* shdrs[SCNS];	/* Section header */
  Elf_Data*   data[SCNS];
  int         pshdrs[PSCNS];	/* Index of PROGBIT && ALLOC sections */
  int         pscns;		/* Number of PROGBIT && ALLOC sections */
  Elf32_Addr  imageBase;	/* Load address of image */
  long*	      image;		/* Pointer to program image */
  size_t      imageLength;	/* Image length */
} FData;

void usage(void);
int elfInit(FData*);
int intelHex(FData*);
int buildImage(FData *ef);

int verbose = 0;

/******
 * main		Main entry for futil
 *-----
 *
 * Usage: futil [-v] <infile>
 */
int
main(int argc, char** argv){
  FData ifile;
  int   sts;
  int   i;

  memset(&ifile,0, sizeof(ifile));

  /* Examine the arguments */
  for (i=1; i<argc; i++)
    if (argv[i][0]=='-'){
      if (strcmp(argv[i],"-v")==0)
	verbose = 1;
      else
	usage();
    }
    else
      if (ifile.file==0)
	ifile.file = argv[i];

  /* Open the file and set it up for ELF processing */
  if ((ifile.fd = open(ifile.file,O_RDONLY))<0){
    fprintf(stderr," futil: Error opening %s for input: %s\n",
	    ifile.file,strerror(errno));
    exit(1);
  }
  if (elfInit(&ifile))
    goto error;

  /* Build the in memory image */
  if (sts = buildImage(&ifile))
    goto error;

  /* Generate the hex file */
  sts = intelHex(&ifile);

 error:
  close(ifile.fd);

  return sts;
}


/*******
 * usage
 */
void
usage(){
  fprintf(stderr," Usage: futil [-v] file\n");
  exit(1);
}


/*********
 * elfInit
 */
int
elfInit(FData* ef){

  /* ELF ident string */
  static unsigned char mipsELF32[EI_NIDENT] = {
    ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS32, ELFDATA2MSB, EV_CURRENT };
    
  static   version=0;
  char*    ident;
  int      i;
  Elf_Scn* scn;
  int      sts = 0;
  

  /* Check the elf library version (once) */
  if (version++ == 0){
    if (elf_version(EV_CURRENT)==NULL){
      fprintf(stderr," futil: ELF library out of rev\n");
      return 1;
    }
  }

  /* Elf_begin initializes ELF processing */
  if ((ef->elf = elf_begin(ef->fd,ELF_C_RDWR,NULL))==NULL){
    fprintf(stderr,
	    " futil: Could not open file %s for elf processing\n",
	    ef->file);
    return 1;
  }

  /* Check ident data for input files.  We only handle ELF32 */
  if ((ident = elf_getident(ef->elf,NULL))==NULL){
    fprintf(stderr,
	    " futil: Could not find ident data in file %s\n",
	    ef->file);
    return 1;
  }
  if (memcmp(mipsELF32,ident,EI_NIDENT)!=0){
    fprintf(stderr,
	    " futil: Invalid elf ident data in file %s\n",
	    ef->file);
    return 1;
  }
  
  /* Get headers */
  if ((ef->ehdr = elf32_getehdr(ef->elf))==NULL){
    fprintf(stderr,
	    " futil: Could not get object file header for file %s\n",
	    ef->file);
    return 1;
  }
  if (ef->ehdr->e_phnum != 3){
    fprintf(stderr," futil: Incorrect number of program headers for file %s\n"
	   "       futil expects 3 headers (reginfo, text, data).\n",
	   ef->file);
    return 1;
  }
  if ((ef->phdr = elf32_getphdr(ef->elf))==NULL){
    fprintf(stderr,
	    " futil: Could not get program file header for file %s\n",
	    ef->file);
    return 1;
  }
  if (ef->ehdr->e_shnum > SCNS){
    fprintf(stderr,
	    " futil: Too many section headers in file %s\n",
	    ef->file);
    return 1;
  }    
  
  if (verbose)
    fprintf(stderr,"\n"
	    "          Section     Addr       Size       Flags\n"
	    "    ===============================================\n");
  /* Get section data */
  for (i=0,scn=NULL;i<ef->ehdr->e_shnum;i++){
    Elf32_Shdr* shdr;
    scn = ef->scns[i]= elf_nextscn(ef->elf,scn);
    if (!scn)
      break;
    shdr = ef->shdrs[i] = elf32_getshdr(scn);
    
    if (verbose)
      fprintf(stderr," %16s  0x%08x 0x%08x 0x%08x %c\n",
	      elf_strptr(ef->elf,ef->ehdr->e_shstrndx, (size_t)shdr->sh_name),
	      shdr->sh_addr,
	      shdr->sh_size,
	      shdr->sh_flags,
	      (shdr->sh_flags & SHF_ALLOC) && (shdr->sh_type == SHT_PROGBITS)
	      ? '*' : ' ');	
    
    
    /* Record the sections SHT_PROGBITS, SHF_ALLOC sections */
    if ((shdr->sh_flags & SHF_ALLOC) && (shdr->sh_type == SHT_PROGBITS))
      if (ef->pscns<PSCNS)
	ef->pshdrs[ef->pscns++] = i;
      else{
	fprintf(stderr,
		" futil: Too many program sections in input file %s,\n"
		"       Only .text, .data, and .rodata allowed\n",
		ef->file);
	return 1;
      }
  }
  
  return sts;
}


#define SCN(i)  (ef->scns[ef->pshdrs[i]])
#define SHDR(i) (ef->shdrs[ef->pshdrs[i]])

/************
 * buildImage	Build image in memory
 */
int
buildImage(FData *ef){
  int            i;
  int            sts = 0;
  FlashSegment*  fseg;
  long*	         lp;
  long	         xsum;

  /* Verify the program sections are in ascending address order */
  for (i=0; i<ef->pscns-1;i++)
    if (SHDR(i)->sh_addr > SHDR(i+1)->sh_addr){
      fprintf(stderr,
	      " futil: Sections not in order in input file %s,\n"
	      "       Sections must be in order of"
	      " ascending memory addresses\n");
      return 1;
    }

  /*
   * Set image base address.
   * Compute image length.
   * Round up to nearest 8 bytes.
   * Allocate an image buffer and init to 0;
   */
  ef->imageBase = SHDR(0)->sh_addr;
  ef->imageLength = 
    SHDR(ef->pscns-1)->sh_size
      + SHDR(ef->pscns-1)->sh_addr - SHDR(0)->sh_addr 
	+ 8;			/* Account for checksum we are adding */
  ef->imageLength = (ef->imageLength + 7) & ~7;

  ef->image = (long*)malloc(ef->imageLength);
  if (ef->image==0){
    fprintf(stderr,
	    " futil: Unable to allocate buffer of size %d for image %s\n",
	    ef->imageLength,
	    ef->file);
    return 1;
  }
  memset(ef->image,0,ef->imageLength);


  /* Fill the image */

  for (i=0;i<ef->pscns-1;i++){
    Elf_Data* data = elf_getdata(SCN(i),0);
    memcpy((char*)ef->image+(SHDR(i)->sh_addr - ef->imageBase),
	   data->d_buf,
	   data->d_size);
  }
  
  /* Map the beginning of the image to a FLASH segment header */
  fseg = (FlashSegment*)ef->image;
  fseg->segLen = ef->imageLength;


  /* Compute and set the header checksum */
  assert(ef->imageLength > hdrSize(fseg));
  for (lp = (long*)fseg, xsum = 0; lp!= chksum(fseg); lp++)
    xsum += *lp;
  *lp = - xsum;

  /* Compute the checksum for the entire flash segment */
  for (lp = (long*)fseg, xsum = 0; lp!= segChksum(fseg); lp++)
    xsum += *lp;
  *lp = - xsum;

  /* Done with elf */
  elf_end(ef->elf);
  return 0;
}


/* Intel hex record assembly */
typedef struct _ihr{
  int           dirty;
  unsigned char _data[32];
} IntelHexRec;
#define bcnt  _data[0]
#define rtype _data[3]

#define emitData(r,c) r->_data[4+r->bcnt++] = c
#define emitAddr(r,a) r->_data[1] = ((a)>>8) & 0xff; r->_data[2] = (a) & 0xff
#define emitType(r,t) r->_data[3] = t

/*************
 * flushRecord	Finish and flush the record
 */
void
flushRecord(IntelHexRec* ihr){
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
  printf(":%s\n",buf);

  ihr->dirty = 0;
  ihr->bcnt  = 0;
}

/*********
 * emitELA
 */
void
emitELA(IntelHexRec* ihr, Elf32_Addr addr){
  assert(ihr->bcnt==0);
  ihr->dirty = 1;
  emitAddr(ihr,0);
  emitType(ihr,4);
  emitData(ihr, (addr>>24) & 0xff);
  emitData(ihr, (addr>>16) & 0xff);
  flushRecord(ihr);
}

/*********
 * emitEOF
 */
void
emitEOF(IntelHexRec* ihr){
  assert(ihr->bcnt==0);
  ihr->dirty = 1;
  emitAddr(ihr,0);
  emitType(ihr,1);
  flushRecord(ihr);
}

/********
 * format	Format data record
 */
void
format(IntelHexRec* ihr, Elf32_Addr addr, long word){
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


/**********
 * intelHex	Generate an intel hex file of the image
 */
int
intelHex(FData* ef){
  Elf32_Addr  lastELA = 0;
  Elf32_Addr  addr;
  IntelHexRec ihr;

  memset(&ihr,0,sizeof(ihr));
  
  /* Output every word in the image */
  for (addr = 0; addr<ef->imageLength;addr+=4){
    Elf32_Addr eaddr = ef->imageBase + addr;
    
    /* Check for 64K segment overflow */
    if ((eaddr & 0xffff0000) != (lastELA & 0xffff0000)){
      flushRecord(&ihr);
      emitELA(&ihr,eaddr);
      lastELA = eaddr;
    }
    format(&ihr,eaddr,ef->image[addr/4]);
  }
  flushRecord(&ihr);
  emitEOF(&ihr);
  return 0;
}
