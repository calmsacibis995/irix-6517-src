#include <ulocks.h>

extern void sema_init(int);
extern usema_t* sema_create(void);
extern void sema_cleanup();
extern void sema_p(usema_t* sema);
extern void sema_v(usema_t* sema);

