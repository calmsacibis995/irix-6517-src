#include <assert.h>
#include <errno.h>
#include "fb.h"
#include <fcntl.h>
#include <libelf.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef xxx
#define DEBUG_A
#define DEBUG_B
#endif


#define SCNS 30
#define align(v,a,t) (t)(((long)v + (a-1))&~(a-1))


/* Consolidated elf data */
typedef struct{
  int         fd;		/* File descriptor */
  Elf*	      elf;		/* Elf descriptor */
  Elf32_Ehdr* ehdr;		/* Object file header */
  Elf_Scn*    scns[SCNS];	/* Section descriptors */
  Elf32_Shdr* shdrs[SCNS];	/* Section headers */
  Elf_Data*   data[SCNS];	/* Section data */
  Elf32_Addr  addr[SCNS];	/* Section (relocated) placement addresses */
  int         placedCnt;	/* Number of placed sections */
  int         placed[SCNS];	/* Indexes of placed sections */
  Elf32_Addr  ahl;		/* Last AHL from relocation (see relocate32) */
  int	      bssScnIdx;	/* Index of the bss section */
  int	      acommon;		/* Number of acommon references */
} ElfData_t;
 


int elfToFlash(FbState_t*, ElfData_t*);
int writeSections(FbState_t*,ElfData_t*);
int relocateSections(FbState_t*, ElfData_t*);
int placeSections(FbState_t*, ElfData_t*);
int relocate32(FbState_t*, ElfData_t*, Elf32_Rel*, Elf32_Shdr*);
Elf32_Addr value32(FbState_t*, ElfData_t*,int, int);

/* ELF ident string */
static unsigned char mipsELF32[EI_NIDENT] = {
  ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASS32, ELFDATA2MSB, EV_CURRENT };



/*************
 * loadElfFile
 */
int
loadElfFile(FbState_t* fbs){
  static int once;
  ElfData_t  ef;
  FbFile_t*  file = CURRENT_FILE(fbs);
  int        sts = 1;
  char*      ident;
  Elf_Scn*   scn=0;
  int        scnIdx;

  memset(&ef,0,sizeof(ElfData_t));

  /* One time check of the elf library */
  if (once==0){
    if (elf_version(EV_CURRENT)==NULL){
      printf(" ELF library out of rev\n");
      return 1;
    }
    once = 1;
  }

  /* Open elf file */
  if ((ef.fd = open(file->fileName,O_RDONLY))<0){
    printf(" file open error %s for file: %s\n",
	   strerror(errno),
	   file->fileName);
    return 1;
  }

  /* Initialize elf processing */
  if ((ef.elf = elf_begin(ef.fd,ELF_C_READ,NULL))==NULL){
    printf(" could not open file %s for elf processing\n",
	   file->fileName);
    goto exit;
  }

  
  /* Check ident data for input files.  We only handle ELF32 */
  if ((ident = elf_getident(ef.elf,NULL))==NULL){
    printf(" could not find ident data in file %s\n",
	   file->fileName);
    goto exit;
  }
  if (memcmp(mipsELF32,ident,EI_NIDENT)!=0){
    printf(" invalid elf ident data in file %s\n",
	    file->fileName);
    goto exit;
  }

  /* Get header */
  if ((ef.ehdr = elf32_getehdr(ef.elf))==NULL){
    printf(" could not get object file header for file %s\n",
	    file->fileName);
    goto exit;
  }

  /* Check that we can handle the sections */
  if (ef.ehdr->e_shnum > SCNS){
    printf(" too man section headers in file %s\n",
	   file->fileName);
    goto exit;
  }

  /*
   * Collect the section data.  In particular, collect scn, shdr and data
   * for each section.
   *
   * Note that elf_getscn skips the first section which is always null.
   */
  for (scnIdx=1,scn=NULL;scnIdx<ef.ehdr->e_shnum;scnIdx++){
    Elf32_Shdr* shdr;
    scn = ef.scns[scnIdx] = elf_nextscn(ef.elf,scn);
    shdr = ef.shdrs[scnIdx] = elf32_getshdr(scn);
    ef.data[scnIdx] = elf_getdata(scn,0);

    /* Display abbreviated section data */
    if (fbs->args->verbose){
      if (scnIdx==1)
	printf("\n"
	       "  #        Section    Addr     Size    Flags   Aln\n"
	       " =================================================\n");
      printf(" %2d %14s  %08x %08x %08x  %02x %c\n",
	     scnIdx,
	     elf_strptr(ef.elf,ef.ehdr->e_shstrndx, (size_t)shdr->sh_name),
	     shdr->sh_addr,
	     shdr->sh_size,
	     shdr->sh_flags,
	     shdr->sh_addralign,
	     (shdr->sh_flags & SHF_ALLOC) && (shdr->sh_type == SHT_PROGBITS)
	     ? '*' : ' ');
    }
  }

  if (fbs->args->verbose)
    printf("\n");

  /* Generate the flash image from the section data */
  sts = elfToFlash(fbs,&ef);

  /* Finished */
 exit:
  elf_end(ef.elf);
  close(ef.fd);
			 
  return sts;
}


/************
 * elfToFlash	Move an elf image to a FLASH segment
 */
int
elfToFlash(FbState_t* fbs, ElfData_t* ef){
  int       i;
  FbFile_t* file = CURRENT_FILE(fbs);
  long*     dest;
  int       sts;
  enum {relocate,writeout} pass;

  /* Place the sections in memory or FLASH */
  if (sts = placeSections(fbs,ef))
    return sts;

  /* Relocate the sections */
  if (sts = relocateSections(fbs,ef))
    return sts;


  /*
   * We treat symbols referencing the special ACOMMON section just as if
   * they are referencing the .bss section.  It's not clear how permanent
   * this assumption can be so for now we warn against this.
   */
  if (ef->acommon){
    printf(" warning,"
	   " segment contains symbol(s) allocated in .comm sections.\n");
    ef->acommon = 0;
  }

  /* Write relocated sections to flash image */
  if (sts = writeSections(fbs,ef))
    return sts;

  return 0;
}


/*************
 * relocateElf	Relocate an elf section
 *------------
 * This routine controls the relocation of all the sections.  Note that
 * the order of the relocation sections is not defined by ELF so we must
 * scan the entire section list.
 *
 * Note that the destination addresses of the sections was previously
 * computed and is found in ef->addrs[].
 */
int
relocateSections(FbState_t* fbs, ElfData_t* ef){
  FbFile_t*   file = CURRENT_FILE(fbs);
  int         sts;
  int         scnIdx;

  /* Check that we still have relocation information. */
  if (ef->ehdr->e_type != ET_REL){
    printf(" file %s is not a relocatable file\n", file->fileName);
    return 1;
  }    

  /* Scan for relocation sections */
  for (scnIdx=1;scnIdx<ef->ehdr->e_shnum;scnIdx++){
    Elf32_Shdr* shdr = ef->shdrs[scnIdx];
    Elf32_Rel*  rel  = (Elf32_Rel*)ef->data[scnIdx]->d_buf;
    int         entries;

    /* Filter out the non-relocation sections */
    if (shdr->sh_type!=SHT_REL)
      continue;

    /* Process each relocation entry */
    for (entries=0;
	 entries <shdr->sh_size/shdr->sh_entsize;
	 rel++,entries++)
      if (sts = relocate32(fbs, ef, rel, shdr))
	return sts;
  }
  return 0;
}


/*********
 * value32	Return symbol value
 */
Elf32_Addr
value32(FbState_t* fbs, ElfData_t* ef, int stidx, int symidx){
  Elf32_Sym* sym = ((Elf32_Sym*)ef->data[stidx]->d_buf)+symidx;
  Elf32_Addr addr = 0;
  int        shndx = sym->st_shndx;
  int        stype = ELF32_ST_TYPE(sym->st_info);
  int  	     sbind = ELF32_ST_BIND(sym->st_info);

  assert(shndx>=0);		/* Must not be a special index */
  
  /* Pick up the section base */
  if (sym->st_shndx==SHN_MIPS_ACOMMON){
    /*
     * Some assembly code puts objects in common.  That means we
     * have some symbols referencing the special section SHN_MIPS_ACOMMON.
     *
     * We assume that .comm symbols have already been collected and merged
     * and their symbol values are offsets into the .bss section.
     */
    addr = ef->addr[ef->bssScnIdx];
    ef->acommon++;
  }
  else
    if (((short)sym->st_shndx)>0)
      addr = ef->addr[sym->st_shndx];

  /* For function and object references, add the symbol value */
  if (stype==STT_FUNC || stype==STT_OBJECT)
    addr += sym->st_value;
  
#ifdef DEBUG_A
  printf(" value: %08x [%-20s: %08x]\n",
	 addr,
	 elf_strptr(ef->elf,ef->shdrs[stidx]->sh_link,(size_t)sym->st_name),
	 sym->st_value);
#endif

  return addr;
}


#define DATA(s) ((char*)ef->data[shdr->sh_info]->d_buf)
#define M26     0x3FFFFFF
#define M16     0xFFFF

/************
 * relocate32	Relocate an entry
 *-----------
 * As we don't currently support gprel addressing, and as we only process
 * images that have already been linked, we only have deal with a few
 * of the relocation types.
 */
int
relocate32(FbState_t* fbs,
	   ElfData_t* ef,
	   Elf32_Rel* rel,
	   Elf32_Shdr* shdr){
  int            rsym  = ELF32_R_SYM(rel->r_info);
  int            rtype = ELF32_R_TYPE(rel->r_info);
  unsigned long* loc   = (unsigned long*)(DATA(shdr)+ rel->r_offset);
  unsigned long* loc2;
  Elf32_Rel*     rel2;
  Elf32_Addr     AHL,S,P,A;

#ifdef DEBUG_A
  printf(" off:   %08x, rsym: %3d, rtype: %2d\n",
	 rel->r_offset, rsym, rtype);
#endif

  S = value32(fbs,ef,shdr->sh_link,rsym);

  switch(rtype){

  case R_MIPS_32:
    /* Simple 32 bit relocation */
    *loc += S;
    break;

  case R_MIPS_26:
    /* Relocate a jump address */
    A = *loc & M26;
    P = ef->addr[shdr->sh_info];
    *loc = (*loc & ~M26) | (((((A<<2)|(P & 0xf0000000))+S)>>2) & M26);
    break;

  case R_MIPS_HI16:
    /*
     * The HI16 and LO16 relocations are messy.
     * See the MIPS ABI supplement for description of this relocation.
     */
    rel2 = rel+1;
    assert(ELF32_R_TYPE(rel2->r_info)==R_MIPS_LO16);
    loc2 = (unsigned long*)(DATA(shdr)+rel2->r_offset);
    AHL =  (*loc<<16) ;
    /* partially compute ahl and save it */
    ef->ahl = AHL;
    /* Complete ahl for the HI16*/
    AHL =  AHL + (short)(*loc2 & M16);

    *loc = (*loc & ~M16) | ((AHL+S) - (short)(AHL+S))>>16;
    break;
  case R_MIPS_LO16:
    /* use the saved partial AHL to get the
	AHL for the LO16
    */
    AHL = ef->ahl;
    AHL += (short)((*loc) & M16);
    *loc = (*loc & ~M16) | (AHL+S & M16);
    break;

  default:
    printf(" unsupported relocation type [%d] while processing file %s\n",
	   rtype,CURRENT_FILE(fbs)->fileName);
    return 1;
  }

#ifdef DEBUG_B
  printf(" pc:    %08x, value: %08x\n\n",
	 ef->addr[shdr->sh_info]+rel->r_offset,
	 *loc);
#endif
  return 0;
}


/**************
 * placeSection	Assign section addresses.
 */
int
placeSections(FbState_t* fbs, ElfData_t* ef){
  FbFile_t*      file  = CURRENT_FILE(fbs);
  int            inRam = file->segmentType & FS_RAM;
  int            scnIdx;
  enum {ro,rw,bss} pass;
  int            warning=0;

  /*
   * Scan the section list 3 times.  The first time, process the
   * read only sections, the second time the read/write sections, and
   * the third time the bss sections.  This simple approach insures
   * that the text and read only data sections preceed the data sections
   * and the bss sections are last.  Since ELF doesn't guarantee the
   * order of the sections in the file, this step is necessary to insure
   * the ordering of the sections in the image.
   */
  for (pass=ro; pass<=bss; pass++)
    for (scnIdx=1;scnIdx<ef->ehdr->e_shnum;scnIdx++){

      unsigned long* start;
      Elf32_Shdr*    shdr = ef->shdrs[scnIdx];
      int            size = align(shdr->sh_size,4,long);
      char*          sectionName = elf_strptr(ef->elf,ef->ehdr->e_shstrndx,
					      (size_t)shdr->sh_name);

      /* Filter out all non-text, non-data and non-bss sections first. */
      if ((shdr->sh_flags & SHF_ALLOC==0) ||
	  !(shdr->sh_type == SHT_PROGBITS || shdr->sh_type == SHT_NOBITS))
	continue;

      /* Filter out the sections based on the pass */
      switch(pass){
      case ro:
	if (shdr->sh_flags & SHF_WRITE)
	  continue;
	break;
      case rw:
	if ((shdr->sh_flags & SHF_WRITE)==0 || shdr->sh_type==SHT_NOBITS) 
	  continue;

	/* One time warning about writable sections in flash */
	if (!inRam){
	  warning++;
	  printf("-WARNING: flash segment"
		 " \"%s\" contains a writable data section\n",
		 file->segmentName);
	}
	break;
      case bss:
	if (shdr->sh_type != SHT_NOBITS)
	  continue;
	break;
      }

      /*
       * We deal with two cases:
       *
       *   RAM resident code -- All addr are based on file->ramDesc.endOffset.
       *
       *   FLASH resident code -- Allocated sections (text, data) are based on
       *		the file->flashDesc but the bss (SHT_NOBITS) is based
       *                on file->ramDesc.endOffset.
       */
      if (inRam || shdr->sh_type==SHT_NOBITS)
	start = &file->ramDesc.endOffset;
      else
	start = &file->flashDesc.endOffset;
      
      
      /*
       * Update the descriptor to reflect section alignment.
       * Record the address.
       * Update the descriptor to reflect the sction size.
       */
      *start = align(*start,shdr->sh_addralign,unsigned long);
      ef->addr[scnIdx] = (Elf32_Addr)*start;
      *start += size;

      /* For the BSS section, remember it's index */
      if ((shdr->sh_type == SHT_NOBITS) &&
	  (shdr->sh_flags & SHF_MIPS_GPREL)==0)
	  ef->bssScnIdx = scnIdx;


      /* Report placement of section */
      if (fbs->args->verbose)
	printf(" placeSection: %-12s\t%08x:%08x [%05x]\n",
	       sectionName,
	       ef->addr[scnIdx],
	       ef->addr[scnIdx]+size,
	       size);

      /* If the user didn't specify a RAM address, we catch it here */
      if (ef->addr[scnIdx]==0){
	printf(" invalid destination address [0]"
	       " for section %s of segment %s\n",
	       sectionName,
	       file->segmentName);
	return 1;
      }
      
      /* Record the order of the section placement */
      ef->placed[ef->placedCnt++] = scnIdx;
  }
  return 0;
}


/**************
 * writeSections	Write sections to flash image
 *-------------
 *
 * There are two cases:
 *
 * 1) The segment is to be executed directly from flash.  The sections
 *    should be at image offset = ef->addr[sectionIndex] - promVaddr.
 *
 * 2) The segment is to be copied to RAM first.  In this case a FlashBlock
 *    (flash.h) header generated for each section and placed immediately ahead
 *    of the section data in the flash.  At runtime the section data is 
 *    copied to the location specified in the FlashBlock.
 *
 * Note that when (if) we add compression support, that case will be more or
 * less like the FS_RAM case but we will compress the section data first.
 */
int
writeSections(FbState_t* fbs,ElfData_t* ef){
  FbFile_t*     file = CURRENT_FILE(fbs);
  FlashBlock    fblock;
  int           placedIdx;
  unsigned long exaddr;

  /*
   * We only look at the placed sections and even then, we ignore SHT_NOBIT
   * sections as they are not placed in the FLASH.
   *
   * Note that the first section contains the header and has space reserved
   * for the block addresses (not used, however, on sections executed from
   * flash).  So space is set up for the block headers only in the second
   * and subsequent sections

   * XXX why not just concatenate everything and update the lenght appropriately?
   * I.E., why have any structure to the segments at all?
   * Maybe next change ...
   */
  for (placedIdx=0;placedIdx<ef->placedCnt;placedIdx++){
    int         scnIdx = ef->placed[placedIdx];
    Elf_Data*   data = ef->data[scnIdx];
    Elf32_Shdr* shdr = ef->shdrs[scnIdx];
    int         size = 0;

    if (shdr->sh_type == SHT_NOBITS)
      continue;


    if (file->segmentType & FS_RAM){
      /*
       * Segment is copied to RAM for execution.
       * Construct a flash block and copy it to the image first.
       *
       * NOTE: If/when we add compression, it takes place here.
       */

      /* construct the block header ...
      */
      fblock.addr = ef->addr[scnIdx];
      fblock.length = shdr->sh_size;
      size = sizeof(FlashBlock);
      if(!placedIdx) {
        /* Copy the section first  - segment and block headers preallocated 
	*/
        memcpy(fbs->image + file->imageDesc.endOffset,data->d_buf,shdr->sh_size);
        memcpy(fbs->image + file->imageDesc.endOffset + sizeof(FlashSegment), 
	  &fblock, size);
      } else {
        memcpy(fbs->image + file->imageDesc.endOffset, &fblock, size);
	file->imageDesc.endOffset += sizeof(FlashBlock);
        memcpy(fbs->image + file->imageDesc.endOffset ,data->d_buf,shdr->sh_size);
      }

      /*
       * If this is the text section, record its starting address as the
       * execution address.
       */
      if (shdr->sh_flags & SHF_EXECINSTR)
	exaddr = fblock.addr;
    } else {
      /*
       * Executing from FLASH directly.
       * Construct a flash destination address that is properly aligned
       *   for the section.
       */
      memcpy(fbs->image + file->imageDesc.endOffset,data->d_buf,shdr->sh_size);
      file->imageDesc.endOffset = align(file->imageDesc.endOffset,
					shdr->sh_addralign,unsigned long);
    }

    if (fbs->args->verbose)
      printf(" writeSection: %-12s\t%08x:",
	     elf_strptr(ef->elf,ef->ehdr->e_shstrndx,
			(size_t)shdr->sh_name),
	     file->imageDesc.endOffset);
    
    file->imageDesc.endOffset += shdr->sh_size;
    size += shdr->sh_size;
    if (fbs->args->verbose)
      printf("%08x [%05x]\n",file->imageDesc.endOffset,size);
  }

  /*
   * For FS_RAM files, we must write out a final fblock of zero length and
   * an address equal to the execution address of the segment.
   */
  if (file->segmentType & FS_RAM){
    fblock.addr = exaddr;
    fblock.length = 0;
    memcpy(fbs->image+file->imageDesc.endOffset,&fblock,sizeof(FlashBlock));
    file->imageDesc.endOffset += sizeof(FlashBlock);
  }
  return 0;
}


/**************
 * writeElfFile		Write image out as a primitive elf file
 */
void
writeElfFile(FbState_t* fbs){
  Elf32_Ehdr ehdr;
  Elf32_Phdr phdr;
  int        fd = open(fbs->args->elfFileName,O_WRONLY|O_CREAT,
		       S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  int        len = fbs->end - fbs->image;


  if (fd<0){
    printf(" unable to open elf file %s \n", fbs->args->elfFileName);
    return;
  }

  /*
   * We generate a very primitive ELF file consisting only of the
   * elf header, the program header and the image.  No sections or
   * other data.  This is adequate as input to the current simulator
   * and to the prom programmer.
   */

  memset(&ehdr,0,sizeof ehdr);
  ehdr.e_ident[EI_MAG0] = ELFMAG0;
  ehdr.e_ident[EI_MAG1] = ELFMAG1;
  ehdr.e_ident[EI_MAG2] = ELFMAG2;
  ehdr.e_ident[EI_MAG3] = ELFMAG3;
  ehdr.e_ident[EI_CLASS]= ELFCLASS32;
  ehdr.e_ident[EI_DATA] = ELFDATA2MSB;
  ehdr.e_ident[EI_VERSION]=EV_CURRENT;
  ehdr.e_type           = ET_EXEC;
  ehdr.e_machine        = EM_MIPS;
  ehdr.e_version        = EV_CURRENT;
  ehdr.e_entry          = fbs->args->flashVaddr;
  ehdr.e_phoff          = sizeof ehdr;
  ehdr.e_shoff	        = 0;
  ehdr.e_flags          = 0;
  ehdr.e_ehsize         = sizeof ehdr;
  ehdr.e_phentsize      = sizeof phdr;
  ehdr.e_phnum          = 1;
  ehdr.e_shentsize      = 0;
  ehdr.e_shnum          = 0;
  ehdr.e_shstrndx       = 0;

  memset(&phdr,0,sizeof phdr);
  phdr.p_type           = PT_LOAD;
  phdr.p_offset         = sizeof ehdr + sizeof phdr;
  phdr.p_vaddr          = ehdr.e_entry;
  phdr.p_paddr          = ehdr.e_entry;
  phdr.p_filesz         = len;
  phdr.p_memsz          = phdr.p_filesz;
  phdr.p_flags          = PF_X|PF_W|PF_R;
  phdr.p_align          = 4;

  if ((write(fd,&ehdr,sizeof ehdr)!= sizeof ehdr) ||
      (write(fd,&phdr,sizeof phdr)!= sizeof phdr) ||
      (write(fd,fbs->image,len) != len))
    printf(" write error while writing file %s, %s\n",
	   fbs->args->elfFileName,
	   strerror(errno));
  close(fd);

}
