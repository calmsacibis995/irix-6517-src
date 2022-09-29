/*
 * elfmap.c
 *
 * 	This routine maps an ELF file into memory, ready
 *      for execution.
 * XXX this is really just used to load rld itself...
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.7 $"

#include "synonyms.h"
#include <sgidefs.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <elf.h>
#include <sys/elf.h>
#include "rld_elf.h"
#include "rld_defs.h"
#include "elfmap.h"

#define error(string)
#undef PERROR
#define PERROR(string)

#ifdef DEBUG
#define DPRINTF(x) if (elfmap_debug) printf(x)
static int elfmap_debug = 1;
#else
#define DPRINTF(x)
#endif

#define HBUFSIZE 512

/* PROTOTYPES ************************************************/
static ELF_EHDR *getEhdr(int, char *);
static int validEhdr(ELF_EHDR *);
static ELF_PHDR *getPhdr(ELF_EHDR *, char *);
	  
/***************************************************************
*
*  _elfmap
*  elfmap
*
*  This function takes a pathname (path) and a bit string
*  (opts) as input, verifies that the path name points to
*  an ELF executable or dynamic object, and maps that
*  object into memory.
*
*  Output: This function returns the base address of the file
*  as mapped.  It returns _EM_ERROR otherwise.
*
****************************************************************/

/* ARGSUSED */
ELF_ADDR
__elfmap(char *path, int infd, int opts) {

  int segnum;
  ELF_EHDR *pEhdr;
  ELF_PHDR *pPhdr;
  ELF_PHDR *pPload;
  void *baseaddr;
  char buffer[HBUFSIZE];

  if (!infd && !path) {
    error("No file specified.");
    return((ELF_ADDR)_EM_ERROR );
  }

  if (!infd) {
    /* Open file */
    DPRINTF(("elfmap: opening file %s, opt = 0x%x\n", path, opts));
    if ((infd = open(path, O_RDONLY)) == -1) {
      PERROR("elfmap");
      return((ELF_ADDR)_EM_ERROR);
    }
  } 

  /* Read and verify Elf Header */
  DPRINTF(("elfmap: reading ELF header.\n"));
  pEhdr = getEhdr(infd, buffer);
  if (pEhdr == NULL) {
    close(infd);
    return((ELF_ADDR)_EM_ERROR);
  }

  /* Read program header */
  DPRINTF(("elfmap: reading program header\n"));
  pPhdr = getPhdr(pEhdr, buffer);
  if (pPhdr == NULL) {
    error("Couldn't read Program header.");
    close(infd);
    return((ELF_ADDR)_EM_ERROR );
  }


  segnum = pEhdr->e_phnum;
  pPload = pPhdr;
  while ( (segnum>0) && (pPload->p_type != PT_LOAD)) {
    pPload++;
    segnum--;
  }

  if (segnum == 0) {
    error("elfmap: no PT_LOAD segment found.");
    close(infd);
    return((ELF_ADDR)_EM_ERROR);
  }
      
  baseaddr = (void *) syssgi(SGI_ELFMAP, infd, pPload, segnum);
  if ((OFFSET)baseaddr < 0) {
    error("elfmap: couldn't map file.");
    close(infd);
    return((ELF_ADDR)_EM_ERROR);
  } 
  /* success! */

  close(infd);
  return((ELF_ADDR) baseaddr);
}

/*******************************************************************
 *
 *  getEhdr
 *
 *  This function reads in the ELF header for an ELF file pointed
 *  to by infd.  It will allocate space and verify the size of
 *  the header as well.  All other validation is done by 
 *  validEhdr().  The progress state is EH_SUCCESS if we are
 *  completely successful. Error returns have e->state set to
 *  other values.  This is so we can still return the address
 *  of pEhdr so that it can be freed in one place.
 *
 ******************************************************************/

static ELF_EHDR
*getEhdr(int fd, char *buf) 
{
  ELF_EHDR *pEhdr;

  /* Try to read elfheader and program header in one fell swoop */
  if (read(fd, buf, HBUFSIZE) != HBUFSIZE) {
      error("elfmap: Unexpected EOF");
      return NULL;
  }

  pEhdr = (ELF_EHDR *) buf;

  /* Check size? */
  if (HBUFSIZE < pEhdr->e_ehsize) {
    DPRINTF(("***Size mismatch - !!\n"));
    error("elfmap: Too large an elf header!");
    return NULL;
  }

  /* Validate header */
  if (!validEhdr(pEhdr))
    return NULL;

  return(pEhdr);
}



/**************************************************************
 *
 *  validEhdr
 *
 *  This function checks an ELF header verify that this
 *  is an appropriate file for dynamic loading.  It
 *  returns TRUE if the file is valid, FALSE otherwise.
 *
 **************************************************************/

static int
validEhdr(ELF_EHDR *pEhdr) 
{
  unsigned char *ident;

  /* Check ELF magic */
  ident = pEhdr->e_ident;
  if ( (ident[EI_MAG0] != 0x7f) || (ident[EI_MAG1] != 'E') 
      || (ident[EI_MAG2] != 'L') || (ident[EI_MAG3] != 'F') ) {
    error("elfmap: not an ELF file.");
    return(FALSE);
  }

  /* Check Endian */
  /* According to the Mips ABI supplement p. 4-1, this
   * is the correct value.  I wonder how they handle
   * little endian.???
   */
  if (ident[EI_DATA] != ELFDATA2MSB) {
    error("elfmap: object is opposite Endian from me.");
    return(FALSE);
  }

  /* Check file class */
  if (ident[EI_CLASS] != ELFCLASS) {
    error("elfmap: object has invalid class,");
    return(FALSE);
  }
  /* Check machine type */
  if ((pEhdr->e_machine != EM_NONE) && (pEhdr->e_machine != EM_MIPS)) {
    error("elfmap: object is not for this machine.");
    return(FALSE);
  }

  /* Check ofile type */
  if ((pEhdr->e_type != ET_EXEC) && (pEhdr->e_type != ET_DYN)) {
    error("elfmap: object is not an executable or dynamic object.");
    return(FALSE);
  }

  return(TRUE);
}


/*********************************************************************
 *
 *  getPhdr
 *
 *  This function is passed a file descriptor (infd) and an
 *  ELF header.  It allocates space for and reads a program header.
 *  and returns a pointer to it.  It returns NULL if there is
 *  a problem. 
 *
 ********************************************************************/

static ELF_PHDR 
*getPhdr(ELF_EHDR *pE, char *buf)
{
  ELF_PHDR *pPhdr;
  unsigned size;

  /* allocate space for headers */
  size = pE->e_phentsize * pE->e_phnum;
  if (HBUFSIZE >= pE->e_phoff + size) {
      /* We picked it up in hbuffer, find it now */
      pPhdr = (ELF_PHDR *)((ELF_ADDR)buf + pE->e_phoff);
  } else {
      error("elfmap: program header > HBUFSIZE.");
      return(NULL);
  }

  return (pPhdr);
}
