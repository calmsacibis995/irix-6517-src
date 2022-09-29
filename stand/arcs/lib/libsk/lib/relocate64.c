#define _64BIT_OBJECTS 1
#define do_elf_reloc32(a, b, c, d, e, f) do_elf_reloc64(a, b, c, d, e, f)
#include "../../libsk/lib/relocate.c"
