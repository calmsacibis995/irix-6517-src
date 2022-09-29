

/* This is done this way for easy removability later on, if needed. */
#define NIC_ARG		, nic_t

void discover_hub_console(volatile __psunsigned_t, console_t *, int, int, int);
void discover_xbow_console(volatile __psunsigned_t, console_t *, 
			   console_t *, int, int);
void discover_bridge_console(volatile __psunsigned_t, console_t *, 
			     console_t *, int NIC_ARG, int);
void discover_baseio_console(volatile __psunsigned_t, int, console_t *, 
			     console_t *, int NIC_ARG);
int is_console_device(	volatile __psunsigned_t, 
			volatile __psunsigned_t NIC_ARG, int, int);
void get_ioc3_console(volatile __psunsigned_t, int, int , console_t *, 
		      console_t *, int , nic_t ) ;

void 	xbow_cleanup_link(__psunsigned_t, int) ;

#   define      LWU(x)          (*(volatile unsigned int   *) (x))
#   define      SW(x,v)         (LWU(x) = (unsigned int  ) (v))


