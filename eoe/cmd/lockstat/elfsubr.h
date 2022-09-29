/*
 * "elfsubr.h"
 *
 */

#include <sys/types.h>
#include <libelf.h>

#define ELF_NONE	0
#define ELF_UNKNOWN	1
#define ELF_MIPS32	2
#define ELF_MIPS64	3

/* sets value in third field to the value associated with symbol name
   in second field.  Returns 0 if symbol is found */
extern int	sym_val64(Elf *, char *, __uint64_t *);
extern int	sym_val32(Elf *, char *, __uint32_t *);

/* returns the file type of an elf file according to above #defines */
extern int	elf_type(Elf *);

/* returns the section in the elf file with the name in the second field */
extern Elf_Scn	*find_section(Elf *, char *);

