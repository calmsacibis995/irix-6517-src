/* Procedures (functions with changes output of select) */
#ifdef __GNUC__
#pragma implementation				// Implement procedure class
#endif

#include "mysql_priv.h"
#include "procedure.h"

/*****************************************************************************
** Setup handling of procedure
** Return 0 if everything is ok
*****************************************************************************/

Procedure *
setup_procedure(THD *thd,ORDER *param,select_result *result,int *error)
{
  DBUG_ENTER("setup_procedure");
  *error=0;
  if (!param)
    DBUG_RETURN(0);
  my_printf_error(ER_UNKNOWN_PROCEDURE,ER(ER_UNKNOWN_PROCEDURE),MYF(0),
		  (*param->item)->name);
  *error=1;
  DBUG_RETURN(0);
}
