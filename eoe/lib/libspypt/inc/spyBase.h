#ifndef _SPYBASE_H
#define _SPYBASE_H

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/types.h>


/* Basic spying definitions
 *
 * Spy is the service-library and a user is the client-app.
 */

/* Basic object: process
 *
 * A process is registered with spy by the client as a shared data
 * structure containing process information, callbacks for spy to
 * use and private handles for the client and spy to use.
 *
 * A client registers a text address and arg as callback.
 * This permits the client to register different callbacks
 * for each process it instantiates.
 */
typedef int (*scbFunc_t)(void *,...);
#define SPC(f, p, a1, a2, a3, a4) \
	((p)->sp_vec->f((p)->sp_vec->f ## _arg, a1, a2, a3, a4))

#define SPC_ENTRY(f)	scbFunc_t f; void* f ## _arg;
typedef struct spyCallBack {
	SPC_ENTRY(spc_symbol_addr)	/* (void*,char*,off64_t*) */
	SPC_ENTRY(spc_memory_read)	/* (void*,int,off64_t,char*,size_t) */
	SPC_ENTRY(spc_register_read)	/* (void*,int,void*) */
} spyCallBack_t;


/* Process description shared between spy and client.
 */
typedef enum { SP_O32 = 0, SP_N32, SP_N64 } spABI_t;

#define SP_LIVE(p)	((p)->sp_procfd != -1)

typedef struct spyProc {
	spABI_t		sp_abi;
	int		sp_procfd;	/* /proc or -1 if not live */

	spyCallBack_t*	sp_vec;

	void*		sp_client;	/* private client data for proc */
	void*		sp_spy;		/* private spy data for proc */

} spyProc_t;


#ifdef __cplusplus
}
#endif

#endif /* _SPYBASE_H */
