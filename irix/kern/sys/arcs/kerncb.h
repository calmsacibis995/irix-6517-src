/* ARCS kernel -> prom callback interface (over SPB)
 */

#ifndef _ARCS_KERNCB_H
#define _ARCS_KERNCB_H

#ident "$Revision: 1.1 $"

#if _LANGUAGE_C
void arcs_eaddr(unsigned char *cp);
LONG arcs_nvram_tab(char *, int);
void arcs_printf(char *, ...);
CHAR *arcs_getenv(CHAR *);
#endif

#endif /* _ARCS_KERNKB_H */
