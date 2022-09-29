/* Definitions ripped off from linker for relocatable elf in relocate.c */

#ifndef __LD_ELF_H__
#define __LD_ELF_H__

#include <elf.h>
#include <elfaccess.h>
#include <libelf.h>
#include "cmplrs/msym.h"

#ifdef _64BIT_OBJECTS

#define ELF_EHDR Elf64_Ehdr	    /* Elf header */
#define ELF_PHDR Elf64_Phdr	    /* Program header */
#define ELF_SHDR Elf64_Shdr	    /* Section header */
#define ELF_SYM  Elf64_Sym	    /* Symbol table entry */
#define ELF_REL  Elf64_Rel	    /* relocation records */
#define ELF_RELA Elf64_Rela	    /* relocation with addend */
#define ELF_REGINFO Elf64_RegInfo   /* register info */
#define ELF_DYN Elf64_Dyn           /* .dynamic section */
#define ELF_LIB Elf64_Lib           /* .liblist */
#define ELF_CONFLICT Elf64_Conflict /* .conflict list */
#define ELF_IFACE Elf_Interface_Descriptor /* .MIPS.interfaces */

#define ELF_ST_BIND ELF64_ST_BIND
#define ELF_ST_TYPE ELF64_ST_TYPE
#define ELF_ST_INFO ELF64_ST_INFO

#define ELF_MS_INFO ELF64_MS_INFO   /* set msym ms_info */

#undef ELF_AR_SYMTAB_NAME
#define ELF_AR_SYMTAB_NAME	"/SYM64/         "

#else

#define ELF_EHDR Elf32_Ehdr	    /* Elf header */
#define ELF_PHDR Elf32_Phdr	    /* Program header */
#define ELF_SHDR Elf32_Shdr	    /* Section header */
#define ELF_SYM  Elf32_Sym	    /* Symbol table entry */
#define ELF_REL  Elf32_Rel	    /* relocation records */
#define ELF_RELA Elf32_Rela	    /* relocation with addend */
#define ELF_REGINFO Elf32_RegInfo   /* register info */
#define ELF_DYN Elf32_Dyn           /* .dynamic section */
#define ELF_LIB Elf32_Lib   	    /* liblist */
#define ELF_CONFLICT Elf32_Conflict /* conflict list */
#define ELF_IFACE Elf_Interface_Descriptor /* .MIPS.interfaces */

#define ELF_ST_BIND ELF32_ST_BIND
#define ELF_ST_TYPE ELF32_ST_TYPE
#define ELF_ST_INFO ELF32_ST_INFO

#undef ELF_AR_SYMTAB_NAME
#define ELF_AR_SYMTAB_NAME	"/               "

#define ELF_MS_INFO ELF32_MS_INFO   /* set msym ms_info */

#endif /*_64BIT_OBJECTS */

#define ELF_AR_STRING_NAME	"//              "

#endif /* __LD_ELF_H__ */
