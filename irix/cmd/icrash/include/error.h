#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/include/RCS/error.h,v 1.16 1999/05/25 19:21:38 tjm Exp $"

/** icrash specific error codes
 **
 **/
#define E_NO_LINKS              1001  /* No links to follow in struct      */

/* Errors codes primarily for use with eval (print) functions
 */
#define E_OPEN_PAREN            1100
#define E_CLOSE_PAREN           1101
#define E_BAD_MEMBER            1102
#define E_BAD_OPERATOR          1103
#define E_BAD_TYPE              1104
#define E_NOTYPE                1105
#define E_BAD_POINTER           1106
#define E_BAD_VARIABLE          1107  /* Bad kernel variable */
#define E_BAD_INDEX             1108
#define E_BAD_STRING            1109
#define E_END_EXPECTED          1110
#define E_BAD_EVAR              1111  /* Bad eval variable */
#define E_SYNTAX_ERROR          1199

void print_error();
