#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <libsc.h>

#define IOC3_PCKM 1
#include <sys/PCI/PCI_defs.h>
#include <sys/pckm.h>

#include <uif.h>
#include <pon.h>
#include "d_pci.h"
#include "d_bridge.h"
#include "d_godzilla.h"

extern void pckm_flushinp(int);
static int check_port_pon(int);

#ifdef PROM

#define ENODEV 13

#define PORT0 0
#define PORT1 1

/* flags */
#define NADA		0x0
#define KBD		0x1
#define MSE		0x2
#define KBD_ERR		0x4

/* retry variable */
#define RETRY	3

#define PCI_READ(port)	(*(volatile __uint32_t *)(IOC3_PCI_DEVIO_K1PTR+ \
			(port?IOC3_M_RD:IOC3_K_RD)))
/* 
	When using DEBUG ... diagmode == v to see all of the messages.
 */
#ifdef DEBUG
#define MSG_PRINTF(msg)		msg_printf msg
#else
#define MSG_PRINTF(msg)		((void) 0)
#endif

const struct pckm_data_info pckm_data_bits [] = {
        KM_RD_VALID_0, KM_RD_DATA_0, KM_RD_DATA_0_SHIFT, KM_RD_FRAME_ERR_0,
        KM_RD_VALID_1, KM_RD_DATA_1, KM_RD_DATA_1_SHIFT, KM_RD_FRAME_ERR_1,
        KM_RD_VALID_2, KM_RD_DATA_2, KM_RD_DATA_2_SHIFT, KM_RD_FRAME_ERR_2,
        0, 0, 0, 0,
};

/* 
 * Power on diag - used to test keyboard/mouse
 */
int
pon_ioc3_pckm(void)
{
	static char *testname = "IOC3 keyboard/mouse";
	int port0_flag, port1_flag;
	int km_csr, failflags;
	uint status_reg;
	char *cp;
	char *kbdgfx;

	msg_printf(VRB,"%s test.\n",testname);
	
	failflags = 0;
	/*
	 * The ioc3 has already be initialize, now clear the 
	 * KM_CSR with zero (per IOC3 spec 4.5).
	 */
	status_reg = KM_CSR_K_CLAMP_THREE | KM_CSR_M_CLAMP_THREE;
	*(volatile __uint32_t *)(IOC3_PCI_DEVIO_K1PTR+IOC3_KM_CSR) = status_reg; 

	km_csr = *(volatile __uint32_t *)(IOC3_PCI_DEVIO_K1PTR+IOC3_KM_CSR);

	/*
	 * We check for there two bits in the IOC3_KM_CSR ...
	 * If they are on after we attemped to clear the bits in the above
	 * statement.  The keyboard/mouse will not "transmit or
	 * receive characters" (per IOC3 spec 4.5).
	 */	
	if ((km_csr & KM_CSR_K_PULL_CLK) || (km_csr & KM_CSR_M_PULL_CLK)) {
		failflags = FAIL_PCCTRL;
	} else {
		MSG_PRINTF((VRB,"	Keyboard/Mouse diagnostic         *Started*\n"));

		port0_flag = check_port_pon(PORT0);
		port1_flag = check_port_pon(PORT1);

		/*
	 	 * Since it is possible that we could have any combination
	 	 * of mice/keyboards in the two slots.
	 	 */
		if ((port0_flag & KBD) && (port1_flag & KBD))
			failflags = FAIL_PCKBD;

		if ((port0_flag & MSE) && (port1_flag & MSE))
			failflags = FAIL_PCMS;

		if ((port0_flag & KBD_ERR) || (port1_flag & KBD_ERR))
			failflags = FAIL_PCKBD;

		/*
		 * Return no keyboard IFF the console is set to gfx.
		 */
		if (!(port0_flag & KBD) && !(port1_flag & KBD)) {
			cp = getenv("console");
			if (cp && (*cp == 'g' || *cp == 'G')) {
				/* check for nogfxkbd env */
				if ((kbdgfx = getenv("nogfxkbd")) && (*kbdgfx == '1')) 
					failflags &= ~FAIL_PCKBD;
				else
					failflags = FAIL_PCKBD;
			}
		}
	}

	if (failflags)
		sum_error(testname);
	else
		okydoky();

	return(failflags);		
}

/*
 * First check for BAT, if no KBD_BATOK or no KBD_BATFAIL then we 
 * send a CMD_ID to see the if a device in the "port".
 * After sending a CMD_ID and still nothing ... then the port is
 * empty.  This is like the routine check_port in the kernel.
 */
static int 
check_port_pon(int port)
{
	uint device_id, device_flag;
	int good, ack, id_resp, beep_id, retry;
	const struct pckm_data_info *p;

	device_id = PCI_READ(port);

	device_flag = NADA;
	good = 0;

	/* 
	 * Basic idea is to check the ports if there is valid BAT data.
	 * Only the keyboard will report a error ... the mouse is silent.
	 */	
	for (p = pckm_data_bits; p->valid_bit; p++) {
		if (PCKM_DATA_VALID(device_id)) {
			switch(PCKM_DATA_SHIFT(device_id)) {
			case KBD_BATOK :
				good++;
				break;
			case KBD_BATFAIL :
				device_flag |= KBD_ERR;
				break;
			case MOUSE_ID :	
				if (good) device_flag |= MSE; 
				break;
			}
		}
	}

	/* if not a mouse and we got a KBD_BATOK ... must be a keyboard */
	if (!(device_flag & MSE) && good)
		 device_flag |= KBD;

	/* Short cut ... found a device ... return */
	if (device_flag)
		return(device_flag);

	/*
	 * Hey ... didn't find any valid data, try again.  This time 
	 * send a CMD_ID request to the devices. If a device exists it 
	 * should ID it's self.
	 */
	retry = ack = id_resp = 0;

	pckm_flushinp(port);
	device_id = pckm_pollcmd(port, CMD_ID);

	while (!device_flag && (retry < RETRY)) {
		retry++;
		for (p = pckm_data_bits; p->valid_bit; p++) {
			if (PCKM_DATA_VALID(device_id)) {
				switch(PCKM_DATA_SHIFT(device_id)) {
				case KBD_ACK :			/* 0xfa */
					ack++;
					break;
				case CMD_ID_RESP :		/* 0xab */
					id_resp++;
					break;
				case KEYBD_ID :			/* 0x83 */
					if (ack && id_resp) 
						device_flag |= KBD;
					break;
				case KEYBD_BEEP_ID2 :		/* 0x47 */
					beep_id++;
					break;
				case KEYBD_BEEP_ID1 :		/* 0x53 */
					if (ack && beep_id)
						device_flag |= KBD;
					break;
				case MOUSE_ID :			/* 0x00 */
					if (ack) device_flag |= MSE;
					break;
				}
			}
		}

		if (!device_flag) { 
			if (ack) {
				us_delay(300000); /* delay 300,000 micro seconds */
				device_id = PCI_READ(port);
			} else {
				pckm_flushinp(port);
				device_id = pckm_pollcmd(port, CMD_ID);
			}
		}
		
	} /* End of while */

	return(device_flag);
}
#endif
