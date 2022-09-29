
/* private sync variable implementation */
typedef struct {
	int waiters;
	usema_t *wait;
} sv_t;
extern void sv_wait(sv_t *, ulock_t);
extern int sv_broadcast(sv_t *);
extern int sv_signal(sv_t *);
extern void sv_create(sv_t *);
extern void sv_destroy(sv_t *);
