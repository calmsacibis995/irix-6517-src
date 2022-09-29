#pragma once
typedef struct _haddr {
	void *addr;
	int   len;
	int   type;
} myHaddr_t ;

extern void sub_gethostbyname(const char *);
extern void sub_gethostent();
extern void sub_gethostbyaddr(myHaddr_t * );

