/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#include "mysys_priv.h"
#include "mysys_err.h"
#include <m_string.h>
#include <stdarg.h>
#include <m_ctype.h>

/* Define some external variables for error handling */

char	** NEAR errmsg[MAXMAPS]={0,0,0,0},
	   NEAR errbuff[NRERRBUFFS][ERRMSGSIZE];

/* Error message to user */
/*VARARGS2*/

int my_error(int nr,myf MyFlags, ...)
{
  va_list	ap;
  uint		olen, plen;
  reg1 char	*tpos, *endpos;
  char		* par;
  char		ebuff[ERRMSGSIZE+20];
  DBUG_ENTER("my_error");

  va_start(ap,MyFlags);
  DBUG_PRINT("my", ("nr: %d  MyFlags: %d  errno: %d", nr, MyFlags, errno));

  if (nr / ERRMOD == GLOB && errmsg[GLOB] == 0)
    init_glob_errs();

  olen=(uint) strlen(tpos=errmsg[nr / ERRMOD][nr % ERRMOD]);
  endpos=ebuff;

  while (*tpos)
  {
    if (tpos[0] != '%')
    {
      *endpos++= *tpos++;	/* Copy ordinary char */
      continue;
    }
    if (*++tpos == '%')		/* test if %% */
    {
      olen--;
    }
    else
    {
      /* Skipp if max size is used (to be compatible with printf) */
      while (isdigit(*tpos) || *tpos == '.' || *tpos == '-')
	tpos++;
      if (*tpos == 's')				/* String parameter */
      {
	par = va_arg(ap, char *);
	plen = (uint) strlen(par);
	if (olen + plen < ERRMSGSIZE+2)		/* Replace if possible */
	{
	  endpos=strmov(endpos,par);
	  tpos++;
	  olen+=plen-2;
	  continue;
	}
      }
      else if (*tpos == 'd' || *tpos == 'u')	/* Integer parameter */
      {
	register int iarg;
	iarg = va_arg(ap, int);
	plen= (uint) (int2str((long) iarg,endpos,*tpos == 'd'  ? -10 : 10)-
		      endpos);
	if (olen + plen < ERRMSGSIZE+2) /* Replace parameter if possible */
	{
	  endpos+=plen;
	  tpos++;
	  olen+=plen-2;
	  continue;
	}
      }
    }
    *endpos++='%';		/* % used as % or unknown code */
  }
  *endpos='\0';			/* End of errmessage */
  va_end(ap);
  DBUG_RETURN((*error_handler_hook)(nr, ebuff, MyFlags));
}

	/* Error as printf */

int my_printf_error (uint my_error, const my_string format, myf MyFlags, ...)
{
  va_list args;
  char ebuff[ERRMSGSIZE+20];

  va_start(args,MyFlags);
  (void) vsprintf (ebuff,format,args);
  va_end(args);
  return (*error_handler_hook)(my_error, ebuff, MyFlags);
}

	/* Give message using error_handler_hook */

int my_message(uint my_error, const char *str, register myf MyFlags)
{
  return (*error_handler_hook)(my_error, str, MyFlags);
}
