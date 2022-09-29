/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* Dont let the user break when you are doing something important */
/* Remembers if it got 'SIGINT' and executes it on allow_break */
/* A static buffer is used; don't call dont_break() twice in a row */

#include "mysys_priv.h"
#include "my_static.h"

	/* Set variable that we can't break */

void dont_break(void)
{
#if !defined(THREAD)
  my_dont_interrupt=1;
#endif
  return;
} /* dont_break */

void allow_break(void)
{
#if !defined(THREAD)
  {
    reg1 int index;

    my_dont_interrupt=0;
    if (_my_signals)
    {
      if (_my_signals > MAX_SIGNALS)
	_my_signals=MAX_SIGNALS;
      for (index=0 ; index < _my_signals ; index++)
      {
	if (_my_sig_remember[index].func)			/* Safequard */
	{
	  (*_my_sig_remember[index].func)(_my_sig_remember[index].number);
	  _my_sig_remember[index].func=0;
	}
      }
      _my_signals=0;
    }
  }
#endif
} /* dont_break */

	/* Set old status */

#if !defined(THREAD)
void my_remember_signal(int signal_number, sig_handler (*func) (int))
{
#ifndef __WIN32__
  reg1 int index;

  index=_my_signals++;			/* Nobody can break a ++ ? */
  if (index < MAX_SIGNALS)
  {
    _my_sig_remember[index].number=signal_number;
    _my_sig_remember[index].func=func;
  }
#endif /* __WIN32__ */
} /* my_remember_signal */
#endif /* THREAD */
