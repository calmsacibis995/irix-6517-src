#pragma once

#include <netdb.h>

extern void sub_getservent();
extern void sub_getservbyname(struct servent *serv_struct);
extern void sub_getservbyport(struct servent *serv_struct);
