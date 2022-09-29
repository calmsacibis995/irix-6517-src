/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* Function to set a varible when we got a alarm */
/* Used by my_lock samt functions in m_alarm.h */


#include "mysys_priv.h"
#include "my_alarm.h"

#ifdef HAVE_ALARM

	/* ARGSUSED */
sig_handler my_set_alarm_variable(int signo)
{
  my_have_got_alarm=1;			/* Tell program that time expired */
  return;
}

#endif /* HAVE_ALARM */
