/*
 * Written by Mani Varadarajan.
 *
 * Dumps the contents of the loadable sections on an ELF binary
 * to stdout.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <libelf.h>
#include <elf.h>
#include <stdlib.h>
#include <assert.h>

/*
 * Sorts the given section header array of size nelem
 * in order of loading address.
 */
static void
bubblesort (Elf32_Shdr **shdrArray, int nelem)
{
   int i, j;
   Elf32_Shdr *temp;
   
   for (i = 0; i < nelem; i++) {
      for (j = i; j < nelem; j++) {
	 if (shdrArray[j]->sh_addr < shdrArray[i]->sh_addr) {
	    temp = shdrArray[j];
	    shdrArray[j] = shdrArray[i];
	    shdrArray[i] = temp;
	 }
      }
   }
}


static void
usage (char *name)
{
   fprintf (stderr, "Usage: %s file\n", name);
   exit (0);
}


int 
main (int argc, char *argv[])
{
   int objfd, bytecount, i = 0;
   Elf *elf;
   Elf32_Ehdr *ehdr;
   Elf_Scn *scn;
   Elf32_Shdr *next_shdr, *shdr, *loadableScnHdrs[20];
   unsigned long baseAddr = 0xffffffff;
   unsigned int maxSegSize = 0;
   char *buf;

   if (argc != 2)
      usage (argv[0]);

   if ((objfd = open (argv[1], O_RDONLY)) < 0) {
      fprintf (stderr, "Cannot open object file %s\n", argv[0]);
      return 1;
   }

   if (elf_version (EV_CURRENT) == EV_NONE) {
      fprintf (stderr, "ELF library out of date. Exiting.\n");
      return 1;
   }

   if ((elf = elf_begin (objfd, ELF_C_READ, (Elf *) 0)) == 0) {
      fprintf (stderr, "Error: Elf_begin failed\n");
      return 1;
   }

   bzero(&loadableScnHdrs[0], sizeof(Elf32_Shdr *) * 20);

   scn = 0;
   while (scn = elf_nextscn (elf, scn)) {
      if ((shdr = elf32_getshdr (scn)) == 0) {
	 fprintf (stderr, "Error: can't read section header\n");
	 return 1;
      }

      /*
       * I am assuming that sh_type == SHT_PROGBITS is the first
       * way of distinguishing one segment from another. The next
       * is an address check. This is based on the ELF section
       * headers rather than the program header, because the program 
       * header appears to wrong for some binary builds, and hence is
       * unreliable.
       */
      if ((shdr->sh_type != SHT_PROGBITS && shdr->sh_type != SHT_MIPS_OPTIONS && shdr->sh_type != SHT_MIPS_REGINFO)  ||  shdr->sh_addr == 0)
	 continue;

      if (shdr->sh_addr < baseAddr)
	 baseAddr = shdr->sh_addr;

      if (shdr->sh_size > maxSegSize)
	 maxSegSize = shdr->sh_size;

      loadableScnHdrs[i++] = shdr;
   }

   loadableScnHdrs[i] = 0;

   bubblesort(&loadableScnHdrs[0], i);

   buf = (char *) malloc (maxSegSize);
   bytecount = 0xa0;	/* XXX need to get this from the elf header info XXX */

   for (i = 0; loadableScnHdrs[i] != 0; i++) {
      int err;

      shdr = loadableScnHdrs[i];
      next_shdr = loadableScnHdrs[i+1];
      if (next_shdr != 0)
	bytecount = next_shdr->sh_addr - shdr->sh_addr;
      else
	bytecount = shdr->sh_size;
      if (lseek (objfd, shdr->sh_offset, SEEK_SET) < 0) {
	 perror ("lseek");
	 return 1;
      }

      if (read (objfd, (void *) buf, bytecount) < 0) {
	 perror ("Error reading binary file");
	 return 1;
      }

      if (write (1, buf, bytecount) < 0) {
	 perror ("Error writing to output");
	 return 1;
      }
   }

   elf_end (elf);
}
