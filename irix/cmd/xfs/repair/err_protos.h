#ident "$Revision: 1.3 $"

/* PRINTFLIKE1 */
void	do_abort(char *, ...);		/* abort, internal error */
/* PRINTFLIKE1 */
void	do_error(char *, ...);		/* abort, system error */
/* PRINTFLIKE1 */
void	do_warn(char *, ...);		/* issue warning */
/* PRINTFLIKE1 */
void	do_log(char *, ...);		/* issue log message */
