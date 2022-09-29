
#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <sys/SN/SN0/IP27.h>
#include <sys/SN/vector.h>
#include <sys/SN/agent.h>

#ifndef _STANDALONE
#include <hub.h>
#include <sys/systm.h>
#else
#include <libsk.h>
#include <rtc.h>
#endif

/* TRACE is for simulation.  For now, just undefine it. */
#define TRACE(x)

extern int     hub_cpu_get(void);

/*
 * vector_write_node
 *
 *   Writes a value to a remote hub NI or router register.
 *   Waits for the response before continuing.
 *
 *   Performs retries because when vector traffic is very
 *   heavy, routers may drop some requests/responses.
 *
 *   Returns 0 for success, or a negative value for failure.
 */

int
vector_write_node(net_vec_t dest, nasid_t nasid,
              int write_id, int address,
              __uint64_t value)
{
    __uint64_t          status;
    int                 try, polls;
    int                 r;
    hubreg_t		age_regaddr;
    hubreg_t            age_oldval;
    hubreg_t            age_newval;


    r = 0;

#ifdef _STANDALONE
    age_regaddr = ( hub_cpu_get() ) ? NI_AGE_CPU1_MEMORY : NI_AGE_CPU0_MEMORY;
#else
    age_regaddr = ( private.p_slice ) ? NI_AGE_CPU1_MEMORY : NI_AGE_CPU0_MEMORY;
#endif
    age_oldval  = REMOTE_HUB_L(nasid, age_regaddr);
    age_newval  = (age_oldval & ~NAGE_VCH_MASK) | (VCHANNEL_B << NAGE_VCH_SHFT);

    for (try = 0; try < VEC_RETRIES_W; try++) {
        REMOTE_HUB_L(nasid, NI_VECTOR_CLEAR);

        if (REMOTE_HUB_L(nasid, NI_VECTOR_STATUS) & NVS_VALID) {
            r = NET_ERROR_HARDWARE;
            goto done;
        }

        REMOTE_HUB_S(nasid, NI_VECTOR_DATA, value);

        REMOTE_HUB_S(nasid, NI_VECTOR, dest);
 
	/* Workaround the router bug...
           make all vector operations use VCHANNEL_B 
        */
        REMOTE_HUB_S(nasid, age_regaddr, age_newval);


        REMOTE_HUB_S(nasid, NI_VECTOR_PARMS,
           123L << NVP_PIOID_SHFT | /* Unique PIO ID number */
           (__uint64_t) write_id << NVP_WRITEID_SHFT |
           address & NVP_ADDRESS_MASK |
           PIOTYPE_WRITE);

	/*
         * Revert back to old mode of virtual channel usage.
         */
        REMOTE_HUB_S(nasid, age_regaddr, age_oldval);


        for (polls = 0; polls < VEC_POLLS_W; polls++) {
#ifndef _STANDALONE
	    us_delay(1000);
#else
	    inst_loop_delay(1000);
#endif
            status = REMOTE_HUB_L(nasid, NI_VECTOR_STATUS);
            if (status & NVS_VALID)
                break;
        }

        if (polls == VEC_POLLS_W)
            continue;           /* Timed out; retry */

        if (status & NVS_OVERRUN) { /* Overrun; retry */
            r = NET_ERROR_OVERRUN;
            continue;
        }

        switch (status & NVS_TYPE_MASK) {
        case PIOTYPE_ADDR_ERR:
            return NET_ERROR_ADDRESS;
        case PIOTYPE_CMD_ERR:
            return NET_ERROR_COMMAND;
        case PIOTYPE_PROT_ERR:
            return NET_ERROR_PROT;
        case PIOTYPE_WRITE:
            break;
        default:
            return NET_ERROR_REPLY;
        }

        if ((status & NVS_PIOID_MASK) >> NVS_PIOID_SHFT != 123 ||
            (status & NVS_WRITEID_MASK) >> NVS_WRITEID_SHFT != write_id ||
            (status & NVS_ADDRESS_MASK) != (address & NVP_ADDRESS_MASK)) {
            r = NET_ERROR_REPLY; /* Fatal */
            goto done;
        }

        break;                  /* Success */
    }

    if (try == VEC_RETRIES_W) {
        r = NET_ERROR_TIMEOUT;
        goto done;
    }

 done:

    return r;
}

/*
 * vector_read_node
 *
 *   Reads a value from a remote hub NI or router register.
 *   Waits for the response before continuing.
 *
 *   Performs retries because when vector traffic is very
 *   heavy, routers may drop some requests/responses.
 *
 *   Returns 0 for success, or a negative value for failure.
 */

int
vector_read_node(net_vec_t dest, nasid_t nasid,
             int write_id, int address,
             __uint64_t *value)
{
    __uint64_t          status;
    int                 try, polls;
    int                 r;
    hubreg_t            age_regaddr;
    hubreg_t            age_oldval;
    hubreg_t            age_newval;


    r = 0;
#ifdef _STANDALONE
    age_regaddr = ( hub_cpu_get() ) ? NI_AGE_CPU1_MEMORY : NI_AGE_CPU0_MEMORY;
#else
    age_regaddr = ( private.p_slice ) ? NI_AGE_CPU1_MEMORY : NI_AGE_CPU0_MEMORY;
#endif
    age_oldval  = REMOTE_HUB_L(nasid, age_regaddr);
    age_newval  = (age_oldval & ~NAGE_VCH_MASK) | (VCHANNEL_B << NAGE_VCH_SHFT);

    for (try = 0; try < VEC_RETRIES_R; try++) {
        REMOTE_HUB_L(nasid, NI_VECTOR_CLEAR);

        if (REMOTE_HUB_L(nasid, NI_VECTOR_STATUS) & NVS_VALID) {
            r = NET_ERROR_HARDWARE;
            goto done;
        }

#if 1
        REMOTE_HUB_S(nasid, NI_VECTOR_READ_DATA, 0xdeadbeefdeadbeef);
#endif

        REMOTE_HUB_S(nasid, NI_VECTOR, dest);
    
	/* 
	 * router bug workaround 
	 */

	REMOTE_HUB_S(nasid, age_regaddr, age_newval);

        REMOTE_HUB_S(nasid, NI_VECTOR_PARMS,
           456L << NVP_PIOID_SHFT | /* Unique PIO ID number */
           (__uint64_t) write_id << NVP_WRITEID_SHFT |
           address & NVP_ADDRESS_MASK |
           PIOTYPE_READ);

	/*
         * Revert back to old mode of virtual channel usage.
         */
        REMOTE_HUB_S(nasid, age_regaddr, age_oldval);


        for (polls = 0; polls < VEC_POLLS_R; polls++) {
            status = REMOTE_HUB_L(nasid, NI_VECTOR_STATUS);
            if (status & NVS_VALID)
                break;
        }

        if (polls == VEC_POLLS_R)
            continue;           /* Timed out; retry */

        if (status & NVS_OVERRUN) {
            r = NET_ERROR_OVERRUN;
            goto done;
        }

        switch (status & NVS_TYPE_MASK) {
        case PIOTYPE_ADDR_ERR:
            return NET_ERROR_ADDRESS;
        case PIOTYPE_CMD_ERR:
            return NET_ERROR_COMMAND;
        case PIOTYPE_PROT_ERR:
            return NET_ERROR_PROT;
        case PIOTYPE_READ:
            break;
        default:
            return NET_ERROR_REPLY;
        }

        if ((status & NVS_PIOID_MASK) >> NVS_PIOID_SHFT != 456 ||
            (status & NVS_WRITEID_MASK) >> NVS_WRITEID_SHFT != write_id ||
            (status & NVS_ADDRESS_MASK) != (address & NVP_ADDRESS_MASK)) {
            r = NET_ERROR_REPLY; /* Fatal */
            goto done;
        }

        *value = REMOTE_HUB_L(nasid, NI_VECTOR_READ_DATA);

        break;                  /* Success */
    }

    if (try == VEC_RETRIES_R) {
        r = NET_ERROR_TIMEOUT;
        goto done;
    }

 done:

    return r;
}


/*
 * vector_exch_node
 *
 *   Conditionally and atomically writes a value into a remote hub NI
 *   or router register, provided that the current content of the
 *   register is zero, and returns the value of the register prior to
 *   the exchange.  Waits for the response before continuing.
 *
 *   The value written is intended to be an ID to identify the locker
 *   when the scratch registers are being used for router locking.
 *
 *   Performs retries because when vector traffic is very
 *   heavy, routers may drop some requests/responses.
 *
 *   The caller should check the returned value.  If it's either zero
 *   or the requested value, then the lock will have been obtained.
 *   (It could equal the requested ID if a response was lost and the
 *   exchange was retried).
 *
 *   Returns 0 for success, or a negative value for failure.
 */
int
vector_exch_node(net_vec_t dest, nasid_t nasid,
	     int write_id, int address,
	     __uint64_t *value)
{
    __uint64_t		status;
    int			try, polls;
    int			r;
    hubreg_t            age_regaddr;
    hubreg_t            age_oldval;
    hubreg_t            age_newval;


    TRACE(write_id);
    TRACE(dest);
    TRACE(address);
    TRACE(value);

    r = 0;
#ifdef _STANDALONE
    age_regaddr = ( hub_cpu_get() ) ? NI_AGE_CPU1_MEMORY : NI_AGE_CPU0_MEMORY;
#else
    age_regaddr = ( private.p_slice ) ? NI_AGE_CPU1_MEMORY : NI_AGE_CPU0_MEMORY;
#endif
    age_oldval  = REMOTE_HUB_L(nasid, age_regaddr);
    age_newval  = (age_oldval & ~NAGE_VCH_MASK) | (VCHANNEL_B << NAGE_VCH_SHFT);

    for (try = 0; try < VEC_RETRIES_X; try++) {
	REMOTE_HUB_L(nasid, NI_VECTOR_CLEAR);

	if (REMOTE_HUB_L(nasid, NI_VECTOR_STATUS) & NVS_VALID) {
	    r = NET_ERROR_HARDWARE;
	    goto done;
	}

	REMOTE_HUB_S(nasid, NI_VECTOR_READ_DATA, 0xdeadbeefdeadbeef);

	REMOTE_HUB_S(nasid, NI_VECTOR_DATA, *value);

	REMOTE_HUB_S(nasid, NI_VECTOR, dest);

	/*
         * In order to workaround a router bug, we need to
         * make all craylink traffic go on VCHANNEL_A normally, and only
         * Vector operations to go on VCHANNEL_B.
         * First part is done when the system boots. As part
         * of doing the vector operation, change the PI_CPUx_MEMORY to
         * point to B instead of A, launch the Vector operation, and
         * change it back to A.
         */
        REMOTE_HUB_S(nasid, age_regaddr, age_newval);

	REMOTE_HUB_S(nasid, NI_VECTOR_PARMS,
	   789L << NVP_PIOID_SHFT |	/* Unique PIO ID number */
	   (__uint64_t) write_id << NVP_WRITEID_SHFT |
	   address & NVP_ADDRESS_MASK |
	   PIOTYPE_EXCHANGE);

	/*
         * Revert back to old mode of virtual channel usage.
         */
        REMOTE_HUB_S(nasid, age_regaddr, age_oldval);

	for (polls = 0; polls < VEC_POLLS_X; polls++) {
	    status = REMOTE_HUB_L(nasid, NI_VECTOR_STATUS);
	    if (status & NVS_VALID)
		break;
	}

	if (polls == VEC_POLLS_X)
	    continue;			/* Timed out; retry */

	if (status & NVS_OVERRUN) {
	    r = NET_ERROR_OVERRUN;
	    goto done;
	}

	switch (status & NVS_TYPE_MASK) {
	case PIOTYPE_ADDR_ERR:
	    return NET_ERROR_ADDRESS;
	case PIOTYPE_CMD_ERR:
	    return NET_ERROR_COMMAND;
	case PIOTYPE_PROT_ERR:
	    return NET_ERROR_PROT;
	case PIOTYPE_EXCHANGE:
	    break;
	default:
	    return NET_ERROR_REPLY;
	}

	if ((status & NVS_PIOID_MASK) >> NVS_PIOID_SHFT != 789 ||
	    (status & NVS_WRITEID_MASK) >> NVS_WRITEID_SHFT != write_id ||
	    (status & NVS_ADDRESS_MASK) != (address & NVP_ADDRESS_MASK)) {
	    r = NET_ERROR_REPLY;	/* Fatal */
	    goto done;
	}

	*value = REMOTE_HUB_L(nasid, NI_VECTOR_READ_DATA);

	break;				/* Success */
    }

    if (try == VEC_RETRIES_X) {
	r = NET_ERROR_TIMEOUT;
	goto done;
    }

 done:

    TRACE(r);

    return r;
}

/* Same as vector_read_node but for the local node. */
int
vector_read(net_vec_t dest, int write_id, int address, __uint64_t *value)
{
	return vector_read_node(dest, get_nasid(), write_id, address, value);

}

/* Same as vector_write_node but for the local node. */
int
vector_write(net_vec_t dest, int write_id, int address, __uint64_t value)
{
	return vector_write_node(dest, get_nasid(), write_id, address, value);

}

/* Same as vector_exch_node but for the local node. */
int
vector_exch(net_vec_t dest, int write_id, int address, __uint64_t *value)
{
	return vector_exch_node(dest, get_nasid(), write_id, address, value);

}

/*
 * Routines for performing simple operations on network vectors
 */

int
vector_length(net_vec_t vec)
{
    int		len;

    for (len = 0; vec & 0xfULL << 4 * len; len++)
	;

    return len;
}

net_vec_t
vector_get(net_vec_t vec, int n)
{
    return vec >> n * 4 & 0xfULL;
}

net_vec_t
vector_prefix(net_vec_t vec, int n)
{
    return n ? vec & ~0ULL >> 64 - 4 * n : 0;
}

net_vec_t
vector_modify(net_vec_t entry, int n, int route)
{
    return entry & ~(0xfULL << 4 * n) | (net_vec_t) route << 4 * n;
}

net_vec_t
vector_reverse(net_vec_t vec)
{
    net_vec_t	result;

    for (result = 0; vec; vec >>= 4)
	result = result << 4 | vec & 0xfULL;

    return result;
}

net_vec_t
vector_concat(net_vec_t vec1, net_vec_t vec2)
{
    return vec1 | vec2 << 4 * vector_length(vec1);
}

/*
 * net_errmsg
 *
 *   Translates an NET_ERROR_code into an appropriate message string.
 */

char
*net_errmsg(int rc)
{
    switch (rc) {
    case NET_ERROR_NONE:
	return "No error";
    case NET_ERROR_HARDWARE:
	return "Hardware error";
    case NET_ERROR_OVERRUN:
	return "Extra response(s)";
    case NET_ERROR_REPLY:
	return "Reply parameters mismatch";
    case NET_ERROR_ADDRESS:
	return "Address error response";
    case NET_ERROR_COMMAND:
	return "Command error response";
    case NET_ERROR_PROT:
	return "Protection error response";
    case NET_ERROR_TIMEOUT:
	return "Vector response timeout";
    case NET_ERROR_VECTOR:
	return "Invalid vector";
    case NET_ERROR_ROUTERLOCK:
	return "Timed out locking router";
    default:
	return "Undefined error code";
    }
}
