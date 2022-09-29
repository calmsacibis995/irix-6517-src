#ifndef __RLD_ELF_H__
#define __RLD_ELF_H__
/*
 * rld_defs.h
 *
 *      This file sets everything up for both 32 and 64 bit builds
 *      from the same source with very few #ifdefs in the source.
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

#ident "$Revision: 1.4 $"

#include <elf.h>
#undef ELF_MSYM
#include <libelf.h>
#include "rld_defs.h"

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
#define ELF_MSYM Elf32_Msym

#define ELF_ST_BIND ELF64_ST_BIND
#define ELF_ST_TYPE ELF64_ST_TYPE
#define ELF_ST_INFO ELF64_ST_INFO

#define ELF_MS_REL_INDEX 	ELF64_MS_REL_INDEX
#define ELF_MS_FLAGS 		ELF64_MS_FLAGS
#define ELF_MS_INFO		ELF64_MS_INFO

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
#define ELF_MSYM Elf32_Msym
#define ELFn_GOT Elf32_Got

#define ELF_ST_BIND ELF32_ST_BIND
#define ELF_ST_TYPE ELF32_ST_TYPE
#define ELF_ST_INFO ELF32_ST_INFO

#define ELF_MS_REL_INDEX 	ELF32_MS_REL_INDEX
#define ELF_MS_FLAGS 		ELF32_MS_FLAGS
#define ELF_MS_INFO		ELF32_MS_INFO

#undef ELF_AR_SYMTAB_NAME
#define ELF_AR_SYMTAB_NAME	"/               "

#endif /*_64BIT_OBJECTS */

#define ELF_AR_STRING_NAME	"//              "

#endif /* __RLD_ELF_H__ */
