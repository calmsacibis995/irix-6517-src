struct sockd_entry {
        struct sockd_entry *next;
        void    (*func)(void *, void *);
        void    *arg1, *arg2;
};

extern int sockd_timeout(void (*)(), void *, long);
extern void tpisockd_init(void);
extern void sockd_init(void);

/* globals that sockd looks at to control its actions */
extern char pftimo_active;
extern char if_slowtimo_active;

/* functions sockd may use, if enabled */
extern void pffasttimo(void);
extern void pfslowtimo(void);
extern void if_slowtimo(void);
