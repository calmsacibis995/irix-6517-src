/*
 * elf.c
 *
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libelf.h>
#include "convert.h"

#define ERROR_N_EXIT(x) {fprintf(stderr, "Error: %s.\n", x); return(-1);}
#define NUM_SECTION_TYPES (sizeof(section_types) / sizeof(char *))
#define PADDR_MASK      0x00ffffff

#define MAXLOAD	4
static ulong section_offset[MAXLOAD+1];
static ulong section_paddr[MAXLOAD+1];
static ulong section_size[MAXLOAD+1];
static int nload=0;
static int cs;
FILE *file;

int
ElfInitialize(int fd)
{
	static unsigned char MipsElf32Ident[EI_NIDENT] = {
		0x7f, 'E', 'L', 'F',
		ELFCLASS32, ELFDATA2MSB, EV_CURRENT,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static unsigned char MipsElf64Ident[EI_NIDENT] = {
		0x7f, 'E', 'L', 'F',
		ELFCLASS64, ELFDATA2MSB, EV_CURRENT,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	long long bssaddr = 0;
	char *elf_ident;
	int Elf64;
	Elf *elf;
	int i;

	if (elf_version(EV_CURRENT) == NULL)
		ERROR_N_EXIT("ELF library out of date")

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
		ERROR_N_EXIT("Could not open ELF file")

	if ((elf_ident = elf_getident(elf, NULL)) == NULL)
		ERROR_N_EXIT("Could not find ELF identification")

	Elf64 = 0;
	if (memcmp(MipsElf32Ident, elf_ident, EI_NIDENT) != 0) {
		if (memcmp(MipsElf64Ident, elf_ident, EI_NIDENT) == 0)
			Elf64 = 1;
		else
			ERROR_N_EXIT("Not a valid MIPS ELF file")
	}

	/* initialize segment */
	cs = 0;
	
	/* find ELF program header */
	if (Elf64) {
		Elf64_Ehdr *ehdr;
		Elf64_Phdr *phdr;
		Elf64_Shdr *shdr;
		Elf_Scn *scn;
		char *name;

		if ((ehdr = elf64_getehdr(elf)) == NULL)
			ERROR_N_EXIT("Can't get ELF program header")
		if ((phdr = elf64_getphdr(elf)) == NULL)
			ERROR_N_EXIT("Can't get ELF program header")

		start_address = ehdr->e_entry;

		/*  Find ".bss" section address by looking at section headers
		 * to work around cmplr bugs where p_filesz != 0 for bss.
		 */
		for (i=0,scn=0; i < ehdr->e_shnum; i++) {
			scn = elf_nextscn(elf,scn);
			if (!scn) break;

			shdr = elf64_getshdr(scn);
			name = elf_strptr(elf, ehdr->e_shstrndx,
					  (size_t)shdr->sh_name);
			if (strstr(name,"bss")) {
				bssaddr = shdr->sh_addr;
				break;
			}
		}

		/* Find all text/data sections.
		 */
		for (i=0; i < ehdr->e_phnum; i++,phdr++) {
			if (phdr->p_type != PT_LOAD)		/* junk */
				continue;
			if (phdr->p_filesz == 0)		/* bss */
				continue;
			if (bssaddr == phdr->p_vaddr) {
#ifdef VERBOSE	/* been this way for 3 years, it's not changing (jeffs) */
				fprintf(stderr,"Warning: found bss segment at 0x%llx with phdr->p_filesz != 0.  Skipped.\n", bssaddr);
#endif
				continue;
			}

			if (nload >= MAXLOAD)
				ERROR_N_EXIT("too many sections (>4).")

			/* First loadable block should be text,
			 * then data.
			 */
			if (phdr->p_flags & PF_X)
				load_address = phdr->p_vaddr;
			section_size[nload] = phdr->p_filesz;
			section_offset[nload] = phdr->p_offset;
			section_paddr[nload] = phdr->p_paddr;
			nload++;
		}
		if ((file=fdopen(fd,"r")) == NULL)
			ERROR_N_EXIT("Could not open file as a stream");
		if (fseek(file,section_offset[0],SEEK_SET) != 0)
			ERROR_N_EXIT("Could not seek");
	}
	else {
		Elf32_Ehdr *ehdr;
		Elf32_Phdr *phdr;

		if ((ehdr = elf32_getehdr(elf)) == NULL)
			ERROR_N_EXIT("Can't get ELF program header")
		if ((phdr = elf32_getphdr(elf)) == NULL)
			ERROR_N_EXIT("Can't get ELF program header")

		start_address = ehdr->e_entry;

		/* Find all text/data sections.
		 */
		for (i=0; i < ehdr->e_phnum; i++,phdr++) {
			if (phdr->p_type != PT_LOAD)		/* junk */
				continue;
			if (phdr->p_filesz == 0)		/* bss */
				continue;

			if (nload >= MAXLOAD)
				ERROR_N_EXIT("too many sections (>4).")

			/* First loadable block should be text,
			 * then data.
			 */
			if (phdr->p_flags & PF_X)
				load_address = phdr->p_vaddr;
			section_size[nload] = phdr->p_filesz;
			section_offset[nload] = phdr->p_offset;
			section_paddr[nload] = phdr->p_paddr;
			nload++;
		}
		if ((file=fdopen(fd,"r")) == NULL)
			ERROR_N_EXIT("Could not open file as a stream");
		if (fseek(file,section_offset[0],SEEK_SET) != 0)
			ERROR_N_EXIT("Could not seek");
	}

	/*  Pad out sizes that end at section gaps.  The elf files seem
	 * to be aligned so we can pad the length and go ahead and convert
	 * the pad in the executable.
	 */
        for (i=0; i < nload; i++) {
                if (((section_paddr[i] & PADDR_MASK)+section_size[i]) < (section_paddr[i+1] & PADDR_MASK)) {
                        section_size[i] += (section_paddr[i+1] & PADDR_MASK) -
                                ((section_paddr[i] & PADDR_MASK) + section_size[i]);
                }
        }

	elf_end(elf);

	return(0);
}

int
ElfRead(char *buffer, int length, int fd)
{
	int l;

	/* Check for end of segment, and bump to next segment if needed.
	 */
	if (section_size[cs] <= 0) {
		cs++;
		if (cs >= nload)		/* EOF */
			return(0);
		if (fseek(file,section_offset[cs],SEEK_SET) != 0)
			ERROR_N_EXIT("Could not seek");
	}

	/* Make sure there is enough data left in the section
	 */
	if (length > section_size[cs]) {
		length = section_size[cs];
	}
	section_size[cs] -= length;

	/* Read the data
	 */
	if (length > 0)
		if ((l=fread(buffer,length,1,file)) != 1)
			ERROR_N_EXIT("Can't read segment");

	return(length);
}

int
ElfClose(int fd)
{
	return OK;
}
