/***********************************************************************\
 *	File:		router.c					*
 *									*
 *	Contains code for testing and configuring the network port in	*
 *	the local hub and remove routers.				*
 *									*
 \***********************************************************************/

#define TRACE_FILE_NO		5

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/mips_addrspace.h>
#include <sys/SN/SN0/IP27.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/nvram.h>
#include <sys/nic.h>
#include <libkl.h>

#include "ip27prom.h"
#include "router.h"
#include "discover.h"
#include "net.h"
#include "rtc.h"
#include "libc.h"
#include "libasm.h"

#define	HUB_100_ROUTER_97   0x12
#define	HUB_90_ROUTER_97   0x4

void check_other_hub(pcfg_t *p);

/*
 * router_lock
 *
 *   Tries to lock a router using RR_SCRATCH_REG1 as the lock register.
 *   Returns 0 if lock is achieved, a negative error code otherwise.
 *   The polling period and the timeout period are adjustable in microseconds.
 *   A timeout of zero polls a single time.
 */

int
router_lock(net_vec_t dest,
	    int poll_usec,	/* Lock poll period */
	    int timeout_usec)	/* 0 for single try */
{
    __uint64_t		lock;
    rtc_time_t		expire;
    int			r;

    TRACE(dest);
    TRACE(poll_usec);
    TRACE(timeout_usec);

    expire = rtc_time() + timeout_usec;

    TRACE(expire);

    while (1) {
	lock	= 0xffffL;

	if ((r = vector_exch(dest, 0, RR_SCRATCH_REG1, &lock)) < 0)
	    goto done;

	lock &= 0xffff;		/* Garbage above 16 bits */

	if (lock == 0)		/* Achieved lock? */
	    break;

	if (rtc_time() >= expire) {
	    r = NET_ERROR_ROUTERLOCK;
	    goto done;
	}

	rtc_sleep(poll_usec);
    }

    r = 0;

 done:

    TRACE(r);

    return r;
}

/*
 * router_unlock
 */

int
router_unlock(net_vec_t dest)
{
    TRACE(dest);

    return vector_write(dest, 0, RR_SCRATCH_REG1, 0L);
}

/*
 * router_nic_get
 *
 *   Warning:  Only one access to the router's NIC may be in progress
 *   at a time since it is definitely NOT an atomic operation.  The
 *   router should be locked.
 *
 *   This may return a NET_ERROR_* code or a NIC_* code.
 */

static int router_nic_access(nic_data_t data,
			     int pulse, int sample, int delay)
{
    __uint64_t		value;

    value = nic_get_phase_bits() | MCR_PACK(pulse, sample);

    vector_write((net_vec_t) data, 0, RR_NIC_ULAN, value);

    while (vector_read((net_vec_t) data, 0, RR_NIC_ULAN, &value) >= 0)
	if (MCR_DONE(value))
	    break;

    rtc_sleep(delay);

    return MCR_DATA(value);
}

int
router_nic_get(net_vec_t dest, __uint64_t *nic)
{
    nic_state_t		ns;
    int			r;
    __uint64_t		value;

    TRACE(dest);

    if ((r = vector_read(dest, 0, RR_STATUS_REV_ID, &value)) < 0)
	goto done;

    if (GET_FIELD(value, NSRI_CHIPID) != CHIPID_ROUTER) {
	r = NET_ERROR_VECTOR;
	goto done;
    }

    if ((r = nic_setup(&ns,
		       router_nic_access,
		       (nic_data_t) dest)) < 0)
	goto done;

    *nic = 0;				/* Clear two MS bytes */

    r = nic_next(&ns, (char *) nic + 2, (char *) 0, (char *) 0);

 done:

    if (r != 0 || *nic == 0)
	*nic = rtc_time();		/* Random number on failure */

    TRACE(*nic);
    TRACE(r);

    return r;
}

/*
 * router_led_set, router_led_get
 *
 *   Sets or gets a router's 6-bit LED display status, without affecting
 *   anything else.  Warning: rather slow.
 */

int
router_led_set(net_vec_t dest, int leds)
{
    __uint64_t		status;
    int			r;

    if ((r = vector_read(dest, 0, RR_STATUS_REV_ID, &status)) < 0)
	return r;

    status = (status & RSRI_CHIPOUT_MASK | /* Maintain chip outputs */
	      (__uint64_t) (leds & 0x3f) << RSRI_SWLED_SHFT);

    if ((r = vector_write(dest, 0, RR_STATUS_REV_ID, status)) < 0)
	return r;

    return 0;
}

int
router_led_get(net_vec_t dest, int *leds)
{
    __uint64_t		status;
    int			r;

    if ((r = vector_read(dest, 0, RR_STATUS_REV_ID, &status)) < 0)
	return r;

    *leds = (int) (status >> RSRI_SWLED_SHFT) & 0x3f;

    return 0;
}

/*
 * router_chipio_set, router_chipio_get
 *
 *   Sets or gets a router's 4-bit general chip inputs and/or 4-bit general
 *   chip outputs, without affecting anything else.  Warning: rather slow.
 */

int
router_chipio_set(net_vec_t dest, int chipout)
{
    __uint64_t		status;
    int			r;

    if ((r = vector_read(dest, 0, RR_STATUS_REV_ID, &status)) < 0)
	return r;

    status = (status & RSRI_SWLED_MASK |	/* Maintain LEDs */
	      (__uint64_t) (chipout & 0xf) << RSRI_CHIPOUT_SHFT);

    if ((r = vector_write(dest, 0, RR_STATUS_REV_ID, status)) < 0)
	return r;

    return 0;
}

int
router_chipio_get(net_vec_t dest, int *chipin, int *chipout)
{
    __uint64_t		status;
    int			r;

    if ((r = vector_read(dest, 0, RR_STATUS_REV_ID, &status)) < 0)
	return r;

    if (chipin)
	*chipin	 = (int) (status >> RSRI_CHIPIN_SHFT ) & 0xf;

    if (chipout)
	*chipout = (int) (status >> RSRI_CHIPOUT_SHFT) & 0xf;

    return 0;
}

/*
 * router_clear_tables
 *
 *   Set all meta and local next-exit table entries to 0777777 (no ports
 *   connected) in a specified remote router.
 *
 *   'Vector' should be a path that ends in a router; i.e. the last
 *   routing nybble is 0.
 */

int
router_clear_tables(net_vec_t dest)
{
    int			i, r;

    TRACE(dest);

    for (i = 0; i < 32; i++)
	if ((r = vector_write(dest, 0, RR_META_TABLE(i), 0777777L)) < 0)
	    goto done;

    TRACE(0);

    for (i = 0; i < 16; i++)
	if ((r = vector_write(dest, 0, RR_LOCAL_TABLE(i), 0777777L)) < 0)
	    goto done;

    r = 0;

 done:

    TRACE(r);

    return r;
}

/*
 * Function:		router_fence_ports
 * Args:		p -> local pcfg_t to check 
 *			fence_ports -> list of ports to be fenced
 *                      set_or_clear -> set up fences or clear fences
 *                      local_or_port -> Setup/clear fences for local blk/port
 *                      port_mask -> If port fences, port_mask
 * Returns:		0 if successful
 *			< 0 if fails
 * N.B.:		The rtr is locked before we setup fences. This is done
 *			because fences are setup by gmaster as well as pmaster
 *			(for meta-rtr intra-pttn reset tunneling)
 */

int router_fence_ports(net_vec_t vec,
			__uint64_t fence_mask,
			int set_or_clear,
			int local_or_port,
			__uint64_t port_mask)
{
   __uint64_t 	rr_reg;
   int		i, locked = 0, r = 0;

   fence_mask &= (((__uint64_t) 1 << MAX_ROUTER_PORTS) - 1);

   if ((r = router_lock(vec, 10000, 3000000)) < 0) {
      printf("*** router_fence_ports: Unable to lock rtr vec 0x%lx\n", vec);
      goto done;
   }

   locked = 1;

   if (local_or_port & LOCALB) {
      if ((r = vector_read(vec, 0, RR_PROT_CONF, &rr_reg)) < 0)
         goto done;

      /* turning off RESET_OK for ports specified */
      if (set_or_clear == SET)
         rr_reg &= ~fence_mask;
      else
         rr_reg |= fence_mask;

      if ((r = vector_write(vec, 0, RR_PROT_CONF, rr_reg)) < 0)
         goto done;
   }

   if (local_or_port & PORT) {
      for (i = 1; i <= MAX_ROUTER_PORTS; i++) {

         if (!(port_mask & (1 << (i - 1))))
            continue;

         if ((r = vector_read(vec, 0, RR_RESET_MASK(i), &rr_reg)) < 0)
            goto done;

         if (set_or_clear == SET)
            rr_reg &= ~fence_mask;
         else
            rr_reg |= fence_mask;

         if ((r = vector_write(vec, 0, RR_RESET_MASK(i), rr_reg)) < 0)
            goto done;
      }
   }

done:
   if (locked && ((r = router_unlock(vec)) < 0))
      printf("*** router_fence_ports: Unable to unlock rtr vec 0x%lx\n", vec);

   return r;
}

#ifdef MIXED_SPEEDS

/*
 * If there are mixed speed IP27 plugged into a midplane running at a higher
 * speed tweak the MAX_BURST for each port that is connected to a slower speed
 * HUB so that the faster router doesn't flood the slower HUB
 * The MAX_BURST value is a NVRAM variable (MaxBurst) that can be controlled 
 * for experimentation purposes
 * N.B.: This assumes that the midplanes are all running at a high speed.
 */

#define	ROUTER_FREQ_DEFAULT		IP27C_KHZ(97500)
#define	RRP_MAXBURST_MIN		0x1
#define	RRP_MAXBURST_MAX		0x3f0
#define	RRP_MAXBURST_MIX		0x6

void
router_setup_mixed_speeds(pcfg_t *p)
{
    int		i;

    for (i = 0; i < p->count; i++)
        if ((p->array[i].any.type == PCFG_TYPE_ROUTER) && 
		IS_RESET_SPACE_RTR(&p->array[i].router)) {
            int		port;

            for (port = 1; port < MAX_ROUTER_PORTS; port++) {
                int	conn_index = p->array[i].router.port[port].index;

                if ((conn_index != PCFG_INDEX_INVALID) && 
			(p->array[conn_index].any.type == PCFG_TYPE_HUB) &&
			(((ip27config_t *) IP27CONFIG_ADDR_NODE(
			p->array[conn_index].hub.nasid))->freq_hub <
			ROUTER_FREQ_DEFAULT)) {
                    __psunsigned_t	ioc3_base;
                    __uint64_t		rr_port_param;
                    net_vec_t		vec;
                    char		buf[NVLEN_MAX_BURST + 1];
                    u_short		max_brst;

                    vec = discover_route(p, 0, i, 0);

                    if (vector_read(vec, 0, RR_PORT_PARMS(port), 
				&rr_port_param) < 0)
                        continue;

                    if (!(ioc3_base = get_ioc3_base(get_nasid())))
                        continue;
                    _cpu_get_nvram_buf(ioc3_base + IOC3_NVRAM_OFFSET, 
			NVOFF_MAX_BURST, NVLEN_MAX_BURST, buf);

                    max_brst = (u_short) strtoull(buf, 0, 16);
                    if ((max_brst > RRP_MAXBURST_MAX) || 
				(max_brst < RRP_MAXBURST_MIN))
                        max_brst = RRP_MAXBURST_MIX;
                    SET_FIELD(rr_port_param, RPPARM_MAXBURST, max_brst);

                    vector_write(vec, 0, RR_PORT_PARMS(port), rr_port_param);
                }
            }
        }
}
#endif

int router_test(int port_no, nasid_t nasid, net_vec_t vec)
{
/* router test */
    __uint64_t		count_low;
    __uint64_t		count_high;
    __uint64_t		router_histogram = 0;
    __uint64_t		rparms;
    __uint64_t		niparms;
    __uint64_t		ratio;
    int 		hub_freq;
    int 		cpu_freq;
    int			btype_360;
    char		buf[8];

/* router test */
    db_printf("%d is port_no\n",port_no);
    db_printf("0x%x is vector\n", vec);
    hub_freq = IP27CONFIG.freq_hub/1000000;
    cpu_freq = IP27CONFIG.freq_cpu/1000000;
    db_printf("%d is hub_freq\n",hub_freq);
    if(cpu_freq == 180 || cpu_freq == 195)
	return 0;
    if(hub_freq == 100 || hub_freq == 97)
    /* set hub_max_burst to match 360 MHz backplane */
    {
	niparms = LD(LOCAL_HUB(NI_PORT_PARMS));
	niparms &= ~NPP_MAXBURST_MASK;
	niparms |= (0x4 & NPP_MAXBURST_MASK);
	SD(LOCAL_HUB(NI_PORT_PARMS),niparms);
    }

    
    if(vector_read(vec,0,RR_PORT_PARMS(port_no),&rparms) < 0)
    {
	printf("vector read failed \n");
	return -1;
    }
    rparms &= 0x3ffff;
    rparms |= 0x40000;
    if(hub_freq == 90)
    {
	rparms &= ~RPPARM_MAXBURST_MASK;
	rparms |= (HUB_90_ROUTER_97 & RPPARM_MAXBURST_MASK);
    }
    if(vector_write(vec,0,RR_PORT_PARMS(port_no),rparms) < 0)
    {
	printf("vector write failed \n");
	return -1;
    }
    count_low = get_cop0(C0_COUNT);
    if(vector_write(vec,0,RR_HISTOGRAM(port_no),router_histogram) < 0)
    {
	printf("vector write failed \n");
	return -1;
    }
    rtc_sleep(100);
    count_high = get_cop0(C0_COUNT);
    if(vector_read(vec,0,RR_HISTOGRAM(port_no),&router_histogram) < 0)
    {
	printf("vector read failed \n");
	return -1;
    }

    db_printf("%ld is count_low\n",count_low);
    db_printf("%ld is high\n",count_high);
    db_printf("0x%lx is histogram\n",router_histogram);
    db_printf("CPU cycles = %ld\n", count_high - count_low);
    db_printf("netclk cycles = %ld\n", (router_histogram >> 48) );
    if(router_histogram >> 48 == 0)
    {
	printf("WARNING: could not detect backplane type\n");
	ip27log_printf(IP27LOG_FATAL,"WARNING: could not detect backplane type\n");
	return -1;
    }

    if(hub_freq == 90)
    {
	ratio = ((count_high - count_low)%0xffffffff) * 26;
	ratio /= ((router_histogram >> 48) );
	if(cpu_freq == 225)
	{
	    /* this value should be 65 for 360 MHz and 60 for 390 MHz */
	    db_printf("ratio = %ld\n",ratio);
	    if(ratio > 62 && ratio <69)  /* tested */
		btype_360 = 1;
	    else if(ratio >56 && ratio <=62) /* tested */
		btype_360 = 0;
	    else
	    {
		printf("WARNING: could not detect backplane type\n");
		ip27log_printf(IP27LOG_FATAL,"WARNING: could not detect backplane type\n");
		return -1;
	    }
	}
	if(btype_360)
	/* need to set rr_max_burst to match 360 */
	{
	    rparms &= ~RPPARM_MAXBURST_MASK;
	    rparms |= (0x3f0 & RPPARM_MAXBURST_MASK);
            if(vector_write(vec,0,RR_PORT_PARMS(port_no),rparms) < 0)
    	    {
		printf("vector write failed \n");
		return -1;
    	    }
	}
    }
    if(hub_freq == 100 || hub_freq == 97)
    {
	ratio = ((count_high - count_low)%0xffffffff) * (6*195);
	ratio /= ((router_histogram >> 48) );
	if(cpu_freq == 250)
	{
	    db_printf("ratio = %ld\n",ratio);
	    /* ratio should be around 3000 for 390 and 3250 for 360 */
	    if(ratio > 3125 && ratio <3500)  /* tested */
		btype_360 = 1;
	    else if(ratio > 2700 && ratio < 3125)  /* tested */
		btype_360 = 0;
	    else
	    {
		printf("WARNING: could not detect backplane type\n");
		ip27log_printf(IP27LOG_FATAL,"WARNING: could not detect backplane type\n");
		return -1;
	    }
	}
	if(cpu_freq == 300)
	{
	    db_printf("ratio = %ld\n",ratio);
	    /* ratio should be around 3600 for 390 and 3900 for 360 */
	    if(ratio > 3750 && ratio <4500) /* tested */
		btype_360 = 1;
	    else if(ratio > 3100 && ratio < 3750)
		btype_360 = 0;
	    else
	    {
		printf("WARNING: could not detect backplane type\n");
		ip27log_printf(IP27LOG_FATAL,"WARNING: could not detect backplane type\n");
		return -1;
	    }
	}
	if(!btype_360)
	/* set hub_max_burst to match 390 */
	{
	    niparms &= ~NPP_MAXBURST_MASK;
	    if(hub_freq == 97)
	    	niparms |= (0x3f0 & NPP_MAXBURST_MASK);
	    else
	    	niparms |= (HUB_100_ROUTER_97 & NPP_MAXBURST_MASK);
	    SD(LOCAL_HUB(NI_PORT_PARMS),niparms);
	}
	else
	{
	   if(hub_freq == 100)  /* hub at 100 MHz and back-plane at 360 */
	   {
	    printf("WARNING: Hub at 100 MHz and router at 90 MHz is an unsupported configuration\n");
	    ip27log_printf(IP27LOG_FATAL,"WARNING: Hub at 100 MHz and router at 90 MHz is an unsupported configuration\n");
	    niparms &= ~NPP_MAXBURST_MASK;
	    niparms |= (HUB_100_ROUTER_97 & NPP_MAXBURST_MASK);
	    SD(LOCAL_HUB(NI_PORT_PARMS),niparms);
	   }
	}
    }

    if(btype_360)
    {
	printf("Recognized 360 MHz midplane\n");
    	    sprintf(buf,"360");
    	    ip27log_setenv(nasid, BACK_PLANE, buf, 0);
    }
    else
    {
	printf("Recognized 390 MHz midplane\n");
    	    sprintf(buf,"390");
    	    ip27log_setenv(nasid, BACK_PLANE, buf, 0);
    }
    return 0;
}

void router_test_update(pcfg_t *p)
{
    int  i, count_360 = 0, count_390 = 0;
    char	backplane[8];
    int		back;
    int		wrong_index[512];
    int		unknown_size = 0;
    int		cpu_freq;
    int 	hub_freq;
    pcfg_hub_t	*hub;
    if(p->count == 2 && p->array[1].any.type == PCFG_TYPE_HUB)
    {
	db_printf("possible null router\n");
	check_other_hub(p);
	return;
    }
    for(i=0;i<p->count; i++)
    {
	if(p->array[i].any.type == PCFG_TYPE_HUB && p->array[i].hub.partition == p->array[0].hub.partition)
	{
	    if(p->array[i].hub.flags & PCFG_HUB_HEADLESS)
	    {
		wrong_index[unknown_size++] = i;
		continue;
	    }
	    cpu_freq = (((ip27config_t *)(IP27CONFIG_ADDR_NODE(p->array[i].hub.nasid)))->freq_cpu)/1000000;
	    if(cpu_freq == 180 || cpu_freq == 195)
	    {
		wrong_index[unknown_size++] = i;
		continue;
	    }
	    back = 0;
    	    if(ip27log_getenv(p->array[i].hub.nasid, BACK_PLANE, backplane, "000",0) >= 0)
	    	back = atoi(backplane);
	    if(back == 360)
	    {
		count_360++;
	    	hub_freq = (((ip27config_t *)(IP27CONFIG_ADDR_NODE(p->array[i].hub.nasid)))->freq_hub)/1000000;
		if(hub_freq == 100)
		{
			hub = &p->array[i].hub;
			printf("WARNING: Board with freq %d MHz at module %d slot %d is attached to a back-plane of 360 MHz frquency\n", hub_freq, hub->module, hub->slot);
			ip27log_printf(IP27LOG_FATAL,"WARNING: Board with freq %d MHz at module %d slot %d is attached to a back-plane of 360 MHz frquency\n", hub_freq, hub->module, hub->slot);
		}
	    }
	    else if(back== 390)
		count_390++;
	    else
		wrong_index[unknown_size++] = i;
	        
	}
    }
    if(count_360 && count_390)
    {
       printf("WARNING: there is either mixed back-plane frequencies among modules or the individual boards had problems detecting their frequencies\n");
       ip27log_printf(IP27LOG_FATAL,"WARNING: there is either mixed back-plane frequencies among modules or the individual boards had problems detecting their frequencies\n");
       return;
    }
    if(count_390 && unknown_size)
    {
	char buf[] = "390";
	for(i = 0; i< unknown_size; i++)
	{
	    hub = &p->array[wrong_index[i]].hub;
	    cpu_freq = (((ip27config_t *)(IP27CONFIG_ADDR_NODE(hub->nasid)))->freq_cpu)/1000000;
	    hub_freq = (((ip27config_t *)(IP27CONFIG_ADDR_NODE(hub->nasid)))->freq_hub)/1000000;
	    if(cpu_freq == 180)
	    {
		printf("WARNING: there is an IP27 board with CPU freq. of 180 MHz attached to a 390 MHz backplane \n");
		printf("This board is in module %d slot %d\n",hub->module, hub->slot);
		continue;
	    }
	    else if(cpu_freq == 195)
		; /* do nothing */
	    else if(hub->flags & PCFG_HUB_HEADLESS)
	      /* if hub is headless, we need to determine if we need change 
		in max-bursts */
	    {
		if(hub_freq == 90)
		/* need to change router max burst */
		{
		  net_vec_t vec;
		  pcfg_port_t *pp;
		  int port_no;
		  __uint64_t rparms;
	    	  pp = &hub->port;
	    	  if((port_no = pp->port ) <= 0)
			continue;
	    	  else
	    	  {
            		if ((vec = discover_route(p, 0, pp->index, 0)) == NET_VEC_BAD)
			    continue;
    			if(vector_read(vec,0,RR_PORT_PARMS(port_no),&rparms) < 0)
    			{
				printf("vector read failed \n");
				continue;
    			}
			rparms &= ~RPPARM_MAXBURST_MASK;
			rparms |= (HUB_90_ROUTER_97 & RPPARM_MAXBURST_MASK);
    			if(vector_write(vec,0,RR_PORT_PARMS(port_no),rparms) < 0)
    			{
				printf("vector write failed \n");
				continue;
    			}
			
	    	  }
		}
		else if(hub_freq == 100)
		/* need to change hub max burst */
		{
		__uint64_t niparms;
		niparms = LD(REMOTE_HUB(hub->nasid,NI_PORT_PARMS));
		niparms &= ~NPP_MAXBURST_MASK;
		niparms |= (HUB_100_ROUTER_97 & NPP_MAXBURST_MASK);
		SD(REMOTE_HUB(hub->nasid,NI_PORT_PARMS),niparms);
		}
		else
		{
			printf("WARNING: illegal hub-cpu frequency combination\n");
			printf("cpu frequency %d MHz hub frequency %d MHz at module %d slot %d\n", cpu_freq, hub_freq);
			continue;

		}
		
	    }
	    else
	    {
		printf("Board with freq %d Mhz at module %d slot %d does not know the backplane frequency\n", cpu_freq, hub->module, hub->slot);
		continue;
	    }
    	    ip27log_setenv(hub->nasid, BACK_PLANE, buf, 0);
	}
    }
    if(count_360 && unknown_size)
    {
	char buf[] = "360";
	for(i = 0; i< unknown_size; i++)
	{
	    int hub_freq;
	    hub = &p->array[wrong_index[i]].hub;
	    cpu_freq = (((ip27config_t *)(IP27CONFIG_ADDR_NODE(hub->nasid)))->freq_cpu)/1000000;
	    hub_freq = (((ip27config_t *)(IP27CONFIG_ADDR_NODE(hub->nasid)))->freq_hub)/1000000;
	    if(cpu_freq == 195)
		; /* no action needed.. hub is actually running at 90 */
	    else if(cpu_freq == 180)
		; /* do nothing */
	    else if(hub->flags & PCFG_HUB_HEADLESS)
	      /* if hub is headless, we need to determine if we need change 
		in max-bursts */
	    {
		if(hub_freq == 90)
		    ;	/* do nothing */
		else if(hub_freq == 100)
		{
			printf("WARNING: Unsupported configuration: Board with freq %d MHz at module %d slot %d is attached to a back-plane of 360 MHz frquency\n", hub_freq, hub->module, hub->slot);
			ip27log_printf(IP27LOG_FATAL,"WARNING: Unsupported configuration: Board with freq %d MHz at module %d slot %d is attached to a back-plane of 360 MHz frquency\n", hub_freq, hub->module, hub->slot);
		}
		else
		{
			printf("WARNING: illegal hub-cpu frequency combination\n");
			printf("cpu frequency %d MHz hub frequency %d MHz at module %d slot %d\n", cpu_freq, hub_freq);
			continue;

		}
		
	    }
	    else
	    {
		printf("Board with freq %d Mhz at module %d slot %d does not know the backplane frequency\n", cpu_freq, hub->module, hub->slot);
		continue;
	    }
    	    ip27log_setenv(hub->nasid, BACK_PLANE, buf, 0);
	}
    }
}

void check_other_hub(pcfg_t *p)
{
	int hub_freq0, hub_freq1;
	int cpu_freq0, cpu_freq1;
	__uint64_t   niparms;
	int flag = 0;
	hub_freq0 = (((ip27config_t *)(IP27CONFIG_ADDR_NODE(p->array[0].hub.nasid)))->freq_hub)/1000000;
	hub_freq1 = (((ip27config_t *)(IP27CONFIG_ADDR_NODE(p->array[1].hub.nasid)))->freq_hub)/1000000;
	cpu_freq0 = (((ip27config_t *)(IP27CONFIG_ADDR_NODE(p->array[0].hub.nasid)))->freq_cpu)/1000000;
	cpu_freq1 = (((ip27config_t *)(IP27CONFIG_ADDR_NODE(p->array[1].hub.nasid)))->freq_cpu)/1000000;
	if(cpu_freq0 != 180 && cpu_freq0 != 195)
		flag = 1;
	if(cpu_freq1 != 180 && cpu_freq1 != 195)
		flag = 1;
	
	if(flag == 0 || hub_freq0 == hub_freq1)
	/* they are either both ip27 boards or they both have same hub speed
	   so need to reset max-burst to maximum */
	{
		niparms = LD(REMOTE_HUB(p->array[0].hub.nasid,NI_PORT_PARMS));
		niparms &= ~NPP_MAXBURST_MASK;
		niparms |= (0x3f0 & NPP_MAXBURST_MASK);
		SD(REMOTE_HUB(p->array[0].hub.nasid,NI_PORT_PARMS),niparms);
		niparms = LD(REMOTE_HUB(p->array[1].hub.nasid,NI_PORT_PARMS));
		niparms &= ~NPP_MAXBURST_MASK;
		niparms |= (0x3f0 & NPP_MAXBURST_MASK);
		SD(REMOTE_HUB(p->array[1].hub.nasid,NI_PORT_PARMS),niparms);
	}
	if(flag == 0)
		return;
	if(hub_freq0 > hub_freq1)
	/* hub 1 is slower.. so it can have the maximum max-burst value */
	{
		if(hub_freq0 - hub_freq1 > 3)
		{
			printf("WARNING: significant hub-frequency mismatch in null-router machine\n");
			ip27log_printf(IP27LOG_FATAL,"WARNING: significant hub-frequency mismatch in null-router machine\n");
		}
		niparms = LD(REMOTE_HUB(p->array[1].hub.nasid,NI_PORT_PARMS));
		niparms &= ~NPP_MAXBURST_MASK;
		niparms |= (0x3f0 & NPP_MAXBURST_MASK);
		SD(REMOTE_HUB(p->array[1].hub.nasid,NI_PORT_PARMS),niparms);
	}
	else if(hub_freq1 > hub_freq0)
	{
		if(hub_freq1 - hub_freq0 > 3)
		{
			printf("WARNING: significant hub-frequency mismatch in null-router machine\n");
			ip27log_printf(IP27LOG_FATAL,"WARNING: significant hub-frequency mismatch in null-router machine\n");
		}
		niparms = LD(REMOTE_HUB(p->array[0].hub.nasid,NI_PORT_PARMS));
		niparms &= ~NPP_MAXBURST_MASK;
		niparms |= (0x3f0 & NPP_MAXBURST_MASK);
		SD(REMOTE_HUB(p->array[0].hub.nasid,NI_PORT_PARMS),niparms);
		/* if other hub was headless, also make sure that its maxburst
		   is lower */
		if(p->array[1].hub.flags & PCFG_HUB_HEADLESS)
		{
		    niparms = LD(REMOTE_HUB(p->array[1].hub.nasid,NI_PORT_PARMS));
		    niparms &= ~NPP_MAXBURST_MASK;
		    niparms |= (0x12 & NPP_MAXBURST_MASK);
		    SD(REMOTE_HUB(p->array[1].hub.nasid,NI_PORT_PARMS),niparms);
		}
	}
	if(cpu_freq0 != cpu_freq1)
	{
		char buf[] = "???";
    		ip27log_setenv(p->array[0].hub.nasid, BACK_PLANE, buf, 0);
    		ip27log_setenv(p->array[1].hub.nasid, BACK_PLANE, buf, 0);
	}
}
