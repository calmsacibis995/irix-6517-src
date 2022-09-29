#include <stdio.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>


#define	MAX_CELLS	16
#define	CELL_NONE	-1

#define	CE_PANIC	1
#define	CE_NOTE		2
#define	CE_CONT		3

#define	TRUE	1
#define	FALSE	0

#define	CELL_STATUS_FILE	"cell_status_file"
#define	HB_INTERVAL		1	/* 1 sec interval */
#define	DEAD_TIME		20	/*  dead poll time */

#define	KM_SLEEP	0
#define	KM_NOSLEEP	1

/*
 * Dummy definitions.
 */
#define	KT_PS		1
#define	KT_PRMPT	2
typedef void (st_func_t)(void *);
extern cell_t local_cell;

#define	cellid() local_cell

#define	delay(x) sleep(x)

extern void 	cmn_err(int, char *, ...);
extern void     hb_get_connectivity_set(cell_set_t *, cell_set_t *);
extern void     hb_get_send_set(cell_set_t *);
extern void     hb_get_recv_set(cell_set_t *);
extern void	*kmem_zalloc(int, int);
extern void	kmem_free(void *, int);
extern void 	spinlock_init(lock_t *, char *);
extern int	sthread_create(char *, caddr_t, uint_t, uint_t, uint_t, uint_t,
                          st_func_t, void *, void *, void *, void *);
extern	int	lbolt;
