#ifndef _DHCPTAB_H
#define _DHCPTAB_H

extern void read_dtab(void);
extern int lookup_dtab(u_char len, u_char type, u_char *chaddr);

#define DROP_REQ	0x0001
#define LOG_REQ		0x0002
#define EXEC_REQ	0x0004

struct dhcpmacs {
    int line;
    u_char hlen;
    u_char htype;
    u_char filler[2];
    u_char haddr[16];
    char  *regex_mac;		/* if non zero then match against regex */
    int flags;
  /* security issues
    int background;
    char exec_cmd[256];
  */
};

#endif /* _DHCPTAB_H */
