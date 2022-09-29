#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/include/RCS/error.h,v 1.1 1999/05/25 19:19:20 tjm Exp $"

/** icrash specific error codes
 **
 **/

/* Error codes primarily for use with trace functions
 */
#define E_NO_RA                 1000  /* No return address                 */
#define E_NO_TRACE              1001  /* No trace record                   */
#define E_NO_EFRAME             1002  /* No exception frame                */
#define E_NO_KTHREAD            1003  /* No kthread pointer in trace_rec   */
#define E_NO_PDA                1004  /* No pdap in trace_rec              */
#define E_NO_LINKS              1005  /* No links to follow in struct      */
#define E_STACK_NOT_FOUND       1006  /* Stack not in trace_rec            */

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
