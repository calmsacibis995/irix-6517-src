struct usio_vec {
    int (*read)(void *, char *, int);
    int (*write)(void *, char *, int);
    int (*get_status)(void *);
};

extern void *usio_ioc3_init(int);
