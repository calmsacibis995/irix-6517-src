/*
 * libsc_only.h - Standalone support library definitions and prototypes
 *
 * Thses routines are in libsc, but are global within libsc ONLY and NOT 
 * officially exported routines.
 *
 * $Revision: 1.3 $
 */
#ifndef _LIBSC_ONLY_H
#define _LIBSC_ONLY_H

#include <arcs/types.h>
#include <arcs/io.h>
#include <arcs/dirent.h>
#include <arcs/hinv.h>
#include <arcs/restart.h>
#include <stringlist.h>

/* cmd/menu_cmd.c */
extern int sa_get_response(char *);

/* gui/button.c */
extern void drawBitmap(unsigned short *, int, int);

/* lib/rb.c */
extern int checksum_rb(RestartBlock *);

/* lib/rbutil.c */
extern int isvalid_rb(void);

/******* FROM libsk *************/

/* cmd/passwd_cmd.c */
extern int validate_passwd(void);

/* ml/IP22.c & io/sgi_kbd.c */
extern void bell(void);

#endif
