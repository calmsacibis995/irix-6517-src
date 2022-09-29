#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <y.tab.h>
#include <parse.h>

FILE *stdofp,*stdifp,*stderrfp;
struct action_string Saction;
parse_s_ptr parse_error;

main()
{
  stdofp      = stdout;
  stdifp      = stdin;
  stderrfp    = stderr;

  /* 
   * pointer to error structure.
   */
  parse_error = (parse_s_ptr)calloc(sizeof(parse_s),1);
  /* 
   * Clear fields in Saction.
   */
  memset(&Saction,0,sizeof(struct action_string));

  /* 
   * Loop forever.
   */
  yyparse();
}
