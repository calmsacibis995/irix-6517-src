#include <sys/errno.h>

extern int ckpt_enabled;

void ckpt_init(){ ckpt_enabled = 0;}
int ckpt_restartreturn(){ return ENOSYS; }
int ckpt_sys(){ return ENOSYS; }
int ckpt_prioctl() { return 0; }
void ckpt_lookup(){}
void ckpt_setfd(){}
void ckpt_vnode_free(){}
int ckpt_lookup_add() { return -1; }
void ckpt_lookup_free(){}
void pproc_ckpt_shmget(){}
int pproc_get_ckpt(){return -1;}
void pproc_ckpt_fork(){}
void pproc_ckpt_exit(){}
int ckpt_prioctl_attr(){ return (0); }
int ckpt_prioctl_thread() { return(0); }
