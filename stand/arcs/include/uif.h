#ifndef __UIF_H__
#define __UIF_H__

#ident	"include/uif.h:  $Revision: 1.6 $"

/*
 * IDE universal status codes.
 */

#ifdef LANGUAGE_C

#include "ide_msg.h"
#include "parser.h"

typedef	int	bool_t;

enum bitsense { BIT_TRUE, BIT_INVERT };
enum runflag { RUN_UNTIL_DONE, RUN_UNTIL_ERROR };
enum arcsboolean { FALSE, TRUE };
extern __psint_t *Reportlevel;

/* 
 * global IDE uif functions
 */
void	memerr(char *, __psunsigned_t, __psunsigned_t , int);
void	msg_printf(int msglevel, char *fmt, ...);
void	okydoky(void);
void	sum_error(char *test_name);
void	busy(int);
void	buffon(void);
void	buffoff(void);

#endif /* LANGUAGE_C */
#endif
