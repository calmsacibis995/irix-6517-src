
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>

#define	DOPANIC(_s)	panic(_s)

void	part_init()			{return;}
int	part_id() 			{return(0);}
int	part_from_addr()		{return(0);}
int	part_from_region()		{return(0);}
int	nasid_to_partid()		{return(0);}

void	part_page_free() 		{DOPANIC("part_page_free");}
void	part_page_allocate_node()	{DOPANIC("part_page_allocate_node");}
void	part_page_allocate()		{DOPANIC("part_page_allocate");}
void	part_permit_page()		{DOPANIC("part_permit_page");}
void	part_get_nasid()		{DOPANIC("part_get_nasid");}
int	part_get_promid()		{return(-1);}
void	part_poison_page()		{DOPANIC("part_poison_page");}
void 	part_register_page()		{DOPANIC("part_register_page");}
void 	part_unregister_page()		{DOPANIC("part_unregister_page");}
void	part_interrupt_set()		{DOPANIC("part_interrupt_set");}
void	part_interrupt_id()		{DOPANIC("part_interrupt_id");}

void	part_start_thread()		{DOPANIC("part_start_thread");}
void	part_register_handlers()	{DOPANIC("part_register_handlers");}
void	part_deactivate()		{DOPANIC("part_deactivate");}
int	part_operation()		{return(ENOSYS);}
void 	part_export_page()		{DOPANIC("part_deactivate");}
void 	part_unexport_page() 		{DOPANIC("part_deactivate");}
void	part_set_var() 			{DOPANIC("part_deactivate");}
void	part_get_var() 			{DOPANIC("part_deactivate");}
void	part_heart_beater()		{return;}    
void	part_dump()			{return;}
    
