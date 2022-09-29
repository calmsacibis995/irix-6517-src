#ident	"$Revision: 1.8 $"

typedef void (*adfnc_t)(void *obj, int bit, typnm_t next);

extern void	fa_agblock(void *obj, int bit, typnm_t next);
extern void	fa_agino(void *obj, int bit, typnm_t next);
extern void	fa_attrblock(void *obj, int bit, typnm_t next);
extern void	fa_cfileoffd(void *obj, int bit, typnm_t next);
extern void	fa_cfsblock(void *obj, int bit, typnm_t next);
extern void	fa_dfiloffd(void *obj, int bit, typnm_t next);
extern void	fa_dfsbno(void *obj, int bit, typnm_t next);
extern void	fa_dinode_union(void *obj, int bit, typnm_t next);
extern void	fa_dirblock(void *obj, int bit, typnm_t next);
extern void	fa_drfsbno(void *obj, int bit, typnm_t next);
extern void	fa_drtbno(void *obj, int bit, typnm_t next);
extern void	fa_ino(void *obj, int bit, typnm_t next);
#if VERS >= V_62
extern void	fa_cfileoffa(void *obj, int bit, typnm_t next);
extern void	fa_dfiloffa(void *obj, int bit, typnm_t next);
#endif
#if VERS >= V_654
extern void	fa_ino4(void *obj, int bit, typnm_t next);
extern void	fa_ino8(void *obj, int bit, typnm_t next);
#endif
