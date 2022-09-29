/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* Allow use of the -O variable= option to set long variables */

#include "mysys_priv.h"
#include <m_string.h>

	/* set all changeable variables */

void set_all_changeable_vars(CHANGEABLE_VAR *vars)
{
  for ( ; vars->name ; vars++)
    *vars->varptr= vars->def_value;
}


my_bool set_changeable_varval(const char* var, ulong val,
			      CHANGEABLE_VAR *vars)
{
  char buffer[256];
  sprintf( buffer, "%s=%lu", var, (unsigned long) val );
  return set_changeable_var( buffer, vars );
}


my_bool set_changeable_var(my_string str,CHANGEABLE_VAR *vars)
{
  char endchar;
  my_string end;
  DBUG_ENTER("set_changeable_var");
  DBUG_PRINT("enter",("%s",str));

  if (str)
  {
    if (!(end=strchr(str,'=')))
      fprintf(stderr,"Can't find '=' in expression '%s' to option -O\n",str);
    else
    {
      uint length=(uint) (end-str),found_count=0;
      CHANGEABLE_VAR *var,*found;
      const char *name;
      long num;

      for (var=vars,found=0 ; name=var->name ; var++)
      {
	if (!my_casecmp(name,str,length))
	{
	  found=var; found_count++;
	  if (!name[length])
	  {
	    found_count=1;
	    break;
	  }
	}
      }
      if (found_count == 0)
      {
	fprintf(stderr,"No variable match for: -O '%s'\n",str);
	DBUG_RETURN(1);
      }
      if (found_count > 1)
      {
	fprintf(stderr,"Variable prefix '%*s' is not unique\n",length,str);
	DBUG_RETURN(1);
      }

      num=(long) atol(end+1); endchar=strend(end+1)[-1];
      if (endchar == 'k' || endchar == 'K')
	num*=1024;
      else if (endchar == 'm' || endchar == 'M')
	num*=1024L*1024L;
      if (num < (long) found->min_value)
	num=(long) found->min_value;
      else if ((unsigned long) num >
	       (unsigned long) found->max_value)
	num=(long) found->max_value;
      *found->varptr=(long) ((ulong) (num-found->sub_size) /
			     (ulong) found->block_size);
      (*found->varptr)*= (ulong) found->block_size;
      DBUG_RETURN(0);
    }
  }
  DBUG_RETURN(1);
}
