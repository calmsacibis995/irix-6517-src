#ident "$Header: "

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include <stdlib.h>
#include <string.h>
#include <filehdr.h>
#include <errno.h>
#include <assert.h>
#include <klib/klib.h>

/* elf_open()
 */
Elf *
elf_open(char *fname, struct filehdr *hdr, Elf32_Ehdr *ehdr)
{
	int i, fd;
	Elf *elfp;

	/* make elf happy
	 */
	if (elf_version(EV_CURRENT) == EV_NONE) {
		return((Elf *)NULL);
	}

	/* open it up and see what type of file we have
	 */
	if ((fd = open(fname, O_RDONLY)) < 0) {
		return((Elf *)NULL);
	}

	if (read(fd, hdr, sizeof(*hdr)) != sizeof(*hdr)) {
		(void)close(fd);
		return((Elf *)NULL);
	}

	/* open it up for elf access
	 */
	if ((elfp = elf_begin(fd, ELF_C_READ, (Elf *)NULL)) == NULL) {
		(void)close(fd);
		return((Elf *)NULL);
	}

	if (lseek(fd, 0, SEEK_SET) < 0) {
		(void)close(fd);
		return((Elf *)NULL);
	}

	if (read(fd, ehdr, sizeof(*ehdr)) != sizeof(*ehdr)) {
		(void)close(fd);
		return((Elf *)NULL);
	}

	if (IS_ELF(*ehdr)) {
		/* If we have a 32-bit binary, but still -o32, return failure!
		 */
		if ((ehdr->e_ident[EI_CLASS] == ELFCLASS32) &&
			(!(ehdr->e_flags & EF_MIPS_ABI2))) {
				(void)close(fd);
				return((Elf *)NULL);
		}
	}
	else {
		(void)close(fd);
		return((Elf *)NULL);
	}
	return(elfp);
}
