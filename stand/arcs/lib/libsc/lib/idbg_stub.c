#ident	"lib/libsc/lib/idbg_stub.c:  $Revision: 1.15 $"

/*
 * This file is used for non-debug standalone programs.  If the
 * debugger is being used, then the real idbg.c should be included
 * directly on the link line.
 */

void
idbg_init()
{
}

int symmon;
