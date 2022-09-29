/* xdr_ptrace.h - xdr header for remote ptrace structures */

/*
modification history
--------------------
01d,10dec92,maf  added standard guard against multiple inclusion.
01c,29aug92,maf  changed name of ptrace_return member errno to errno_val,
		   since errno is now a macro in VxWorks 5.1.
01b,25may91,maf  now uses counted bytes struct to transfer registers;
		   removed references to old xdr_regs functions.
		 removed includes of "xdr_regs.h" and "reg.h".
01a,05jun90,llk  extracted from xdr_ptrace.h.
*/

#ifndef __INCxdrptraceh
#define __INCxdrptraceh	1

/*
 *  Counted byte structure used by READ/WRITE TEXT/DATA
 *  and GET/SET REGS/FPREGS
 */
struct c_bytes {
	u_int	len;
	caddr_t	bytes;
};
typedef struct c_bytes C_bytes;

/*
 * enum for discriminated union ptrace_info
 */
enum ptype {
	NOINFO = 0,		/* no additional infomation */
	DATA = 1		/* c_bytes */
};
typedef enum ptype ptype;

/*
 * discrimnated union for passing additional data to be 
 * written to the debugged process.
 */
struct ptrace_info {
	ptype	ttype;
	caddr_t	more_data;	
};
typedef struct ptrace_info Ptrace_info;

/*
 * structure passed to server on all remote ptrace calls
 */
struct rptrace {
	int 	pid;
	int 	data;
	int 	addr;	/* FIX! this really should be caddr_t or something */
	Ptrace_info	info;
};
typedef struct rptrace Rptrace;

/*
 * structure returned by server on all remote ptrace calls
 */
struct ptrace_return {
	int status;
	int errno_val;
	Ptrace_info	info;
};
typedef struct ptrace_return Ptrace_return;

bool_t xdr_c_bytes();
bool_t xdr_ptrace_info();
bool_t xdr_rptrace();
bool_t xdr_ptrace_return();

#endif /* __INCxdrptraceh */
