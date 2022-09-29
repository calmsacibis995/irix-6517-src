#include "synonyms.h"

#ident "$Revision: 1.1 $"

/*
 * Global "cipso" variable, boolean, should RPC server set its process 
 * label to the label of incoming request (be "label agile") or not?
 * Appropriate privilege is required when this is set.
 */
int __svc_label_agile _INITBSS;		/* not agile by default */
