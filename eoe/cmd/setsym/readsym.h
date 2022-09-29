/*
 * "readsym.h"
 *
 */

#define DBSTBL64 0
#define DBSTBL32 1

extern void	init_read_symtab(int, Elf *, int, int, void *, char *, int);
extern int	dwarf_read_symtab(int Verbose);
extern int      elf_read_symtab(void);

