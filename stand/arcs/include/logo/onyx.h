#ident "$Revision: 1.2 $"

#if IP27
unsigned char clogo_data[4*1024];
int clogo_data_size = sizeof(clogo_data);

#else
unsigned char clogo_data[128*1024];
#endif

int clogo_w;
int clogo_h;

