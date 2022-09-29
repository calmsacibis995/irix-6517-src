/*
 * Dummy Symbol Table, since VxWorks can't seem to live without one
 */

#include "vxWorks.h"
#include "symbol.h"

#include "a_out.h"

#undef READ
#undef WRITE

IMPORT disable_downloader ();

SYMBOL standTbl [1] =
    {
    {{NULL},"_disable_downloader", (char*) disable_downloader, 0, N_EXT | N_TEXT},
    };

ULONG standTblSize = 1;

