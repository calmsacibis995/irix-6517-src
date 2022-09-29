/*
 * Flash build -- A tool to construct flash ROM images
 *
 *
 * This program reads data and elf files that contain the flash components and
 * constructs an in-memory image of the flash.  It then writes out the image
 * as either a binary image or as a hex file.
 *
 */

#include <assert.h>
#include "fb.h"
#include <stdio.h>
#include <stdlib.h>

extern char* optarg;
extern int   optind;

void usage(void);
int getArgs(int,char**);
int setIfNotSetUlong(unsigned long*, char*);
int setIfNotSetChar(char**,char*);

/* Command arguments */
static FbArgs_t args;



/******
 * main
 */
int
main(int argc, char** argv){

  /* Process command arguments */
  if (getArgs(argc,argv))
    return 1;

  /* Fill in default values */
  if (args.flashSize==0)
    args.flashSize = DEFAULT_FLASH_SIZE;
  if (args.flashVaddr==0)
    args.flashVaddr = DEFAULT_FLASH_VADDR;
  if (args.flashPageSize==0)
    args.flashPageSize = DEFAULT_FLASH_PAGE_SIZE;

  return buildFlash(&args);
}


/*********
 * getArgs	Process argument list
 *-----
 *
 *  fb [<flashArgs>] [<outputArgs>] <fileArgs>...
 *
 */
int
getArgs(int argc, char** argv){
  int c;
  int sts=0;
  int fileIndex = 0;

  char* options = "uDb:s:v:p:i:h:e:N:V:l:dx:r:cP";

  /* Process the argument list */
  while(sts==0 && optind<argc){
    /* Process options */
    while (sts==0 && ( c = getopt(argc, argv, options))!= EOF)
      switch(c){

	/*
	 * General options
	 */
      case 'u':
	/* Usage */
	usage();
	return 1;
      case 'D':
	args.verbose = 1;
	break;
	
	/*
	 * FLASH/FLASH options
	 */
      case 's':
	/* Flash size */
	sts = setIfNotSetUlong(&args.flashSize,"flash size [-s]");
	break;
      case 'v':
	/* Flash Vaddr */
	sts = setIfNotSetUlong(&args.flashVaddr,"flash virtual address [-v]");
	break;
      case 'p':
	/* Flash flash page size */
	sts = setIfNotSetUlong(&args.flashPageSize,"flash page size [-p]");
	break;
      case 'b':
	/* Flash base address (set writeHex.c) */
	sts = setIfNotSetUlong(&args.flashBase,"flash base address [-b]");
	break;

	/*
	 * Output file options
	 */
      case 'i':
	/* Image output file name */
	sts = setIfNotSetChar(&args.imageFileName,"image file name [-i]");
	break;
      case 'h':
	/* Hex output file name */
	sts = setIfNotSetChar(&args.hexFileName,"hex file name [-h]");
	break;
      case 'e':
	/* Elf output file name */
	sts = setIfNotSetChar(&args.elfFileName,"elf file name [-e]");
	break;

	/*
	 * Segment options
	 */
      case 'N':
	/* Segment Name */
	sts = setIfNotSetChar(&args.files[fileIndex].segmentName,
			      "segment name [-N]");
	break;
      case 'V':
	/* Segment Version */
	sts = setIfNotSetChar(&args.files[fileIndex].segmentVersion,
			      "segment version [-V]");
	break;
      case 'd':
	/* Data segment */
	break;
      case 'x':
	/* Executable segment */
	args.files[fileIndex].segmentType |= FS_CODE;
	sts = setIfNotSetUlong(&args.files[fileIndex].ramAddress,
			       "segment BSS address");
	break;
      case 'r':
	/* Copy to RAM for execution */
	args.files[fileIndex].segmentType |= FS_RAM|FS_CODE;
	sts = setIfNotSetUlong(&args.files[fileIndex].ramAddress,
			       "segment TEXT address");
	break;
      case 'c':
	/* Compress file */
	args.files[fileIndex].segmentType |= FS_COMPRESS;
	break;
      case 'P':
	/* PIC executable */
	args.files[fileIndex].segmentType |= FS_PIC;
	break;
      case 'l':
	/* Pad length */
	sts = setIfNotSetUlong(&args.files[fileIndex].segmentPadToLength,
			       "segment pad to length [-l]");
	break;
	
      case '?':
	/* error */
	printf("fb -u for help\n");
	return 1;
      }
    

    /* Process file name */
    if (optind<argc){
      FbFile_t* f = &args.files[fileIndex];
      f->fileName = argv[optind];
      if (f->segmentName==0){
	printf(" segment name for file %s required\n", f->fileName);
	return 1;
      }
      if (f->segmentVersion==0){
	printf(" version string for file %s required\n", f->fileName);
	return 1;
      }
      optind++;
      if (++fileIndex >= FILE_COUNT){
	printf(" too many files\n");
	return 1;
      }
      args.fileCount++;
    }
  }

  /* Verify that some files specified */
  if (args.fileCount==0){
    printf(" no input files specified\n");
    return 1;
  }
  return sts;
}


/******************
 * setIfNotSetUlong
 */
int
setIfNotSetUlong(unsigned long* ul, char* text){
  if (*ul){
    printf(" %s already set, cannot set more than once\n");
    return 1;
  }
  *ul = strtoul(optarg,0,0);
  return 0;
}


/*****************
 * setIfNotSetChar
 */
int
setIfNotSetChar(char** s, char* text){
  if (*s){
    printf(" %s already set, cannot set more than once\n",text);
    return 1;
  }
  *s = optarg;
  return 0;
}


/*******
 * usage
 */
void
usage(void){
  printf(" fb [<flashArgs>] [<outputArgs>] <segArgs>...\n\n"
	 " <flashArgs> ::=\t-s <flashSize>\t\t-- Size of flash in bytes\n"
	 " \t\t\t-v <flashVaddr>\t\t-- Virtual address of flash\n"
         " \t\t\t-b <promBase>\t\t-- Prom base address\n"
	 " \t\t\t-p <flashPageSize>\t-- Page size of flash prom\n"
	 " \t\t\t-u\t\t\t-- fb usage\n\n"
	 " <outputArgs> ::=\t-i <imageFileName>\t-- Image file name\n"
	 " \t\t\t-h <hexFileName>\t-- Hex file name\n"
	 " \t\t\t-e <elfFileName>\t-- Elf file name\n\n\n"
	 " <segArgs> ::= <segHdrArgs> [<segType>] fileName\n\n"
	 " <segHdrArgs> ::=\t-N <segmentName>\t-- Name of flash segment\n"
	 " \t\t\t-V <segmentVersion>\t-- Version of flash segment\n\n"
	 " <segType> ::=\t\t-d\t\t\t-- Data segment (default)\n"
	 " \t\t\t-x <BSS address>\t-- FLASH executable segment\n"
	 " \t\t\t-r <TEXT address>\t-- RAM executable segment\n"
	 " \t\t\t-l <PAD length>\t\t-- Length to pad segment to\n"
	 " \t\t\t-c\t\t\t-- Compressed segment\n"
	 " \t\t\t-P\t\t\t-- PIC executable segment\n"
	 );
	 
}





