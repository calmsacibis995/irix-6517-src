#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libmdebug/RCS/md_type.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <filehdr.h>
#include <fcntl.h>
#include <stdio.h>
#include <sym.h>
#include <symconst.h>
#include <syms.h>
#include <cmplrs/stsupport.h>
#include <elf_abi.h>
#include <elf_mips.h>
#include <libelf.h>
#include <sex.h>
#include <ldfcn.h>
#include <errno.h>

#include "icrash.h"
#include "eval.h"
#include "libmdebug.h"

extern int errno;
extern int debug;

/*
 * Name: md_dotype()
 * Func: Print out what a structure looks like in the MDEBUG data.
 */
void
md_dotype(command_t cmd)
{
	int i;
	md_type_t *ptr;

	for (i = 0; i < cmd.nargs; i++) {
		ptr = md_lkup_type(cmd.args[i], stStruct);
		if (!ptr) {
			ptr = md_lkup_type(cmd.args[i], stEnum);
		}
		if (!ptr) {
			ptr = md_lkup_type(cmd.args[i], stTypedef);
		}
		if (!ptr) {
			continue;
		}
		md_print_type(0, ptr, 0, cmd.flags & C_WHATIS, cmd.ofp);
	}
}
