#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/sss/configmon/RCS/configmon_event.c,v 1.4 1999/06/30 21:18:10 nayak Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/syslog.h>
#include <sss/configmon_api.h>
#include <eventmonapi.h>

#include "configmon_event.h"

/*
 * send_cm_event()
 */
int
send_cm_event(time_t curtime, int event_type)
{
	int etype, epri;
	char ebuffer[100];

	if (!curtime || !event_type) {
		return(1);
	}

	/* Make sure the eventmon daemon is up and running...
	 */
	if (!emapiIsDaemonStarted()) {
		return(1);
	}

	/* Set the priority to log the messages with
	 */
	epri = LOG_MAKEPRI(LOG_USER,LOG_INFO);

	/* Cycle through the event type bits to see which events need to be sent...
	 */
	if (event_type & CONFIGMON_INIT) {
		strcpy(ebuffer, "ConfigMon INIT");
		if (!emapiSendEvent(0, curtime, CME_CONFIGMON_INIT, epri, ebuffer)) {
#ifdef CM_DEBUG
			fprintf(stderr, "send_cm_event: ERROR: ConfigMon INIT\n");
#endif
			return(1);
		}
	}
	if (event_type & SYSINFO_CHANGE) {
		strcpy(ebuffer, "ConfigMon SYSINFO CHANGED");
		if (!emapiSendEvent(0, curtime, CME_SYSINFO_CHANGE, epri, ebuffer)) {
#ifdef CM_DEBUG
			fprintf(stderr, "send_cm_event: ERROR: ConfigMon SYSINFO CHANGED\n");
#endif
			return(1);
		}
	}
	if (event_type & HARDWARE_INSTALLED) {
		strcpy(ebuffer, "ConfigMon HARDWARE INSTALLED");
		if (!emapiSendEvent(0, curtime, CME_HW_INSTALLED, epri, ebuffer)) {
#ifdef CM_DEBUG
			fprintf(stderr, "send_cm_event: ERROR: ConfigMon HARDWARE INSTALLED\n");
#endif
			return(1);
		}
	}
	if (event_type & HARDWARE_DEINSTALLED) {
		strcpy(ebuffer, "ConfigMon HARDWARE DEINSTALLED");
		if (!emapiSendEvent(0, curtime, CME_HW_DEINSTALLED, epri, ebuffer)) {
#ifdef CM_DEBUG
			fprintf(stderr, "send_cm_event: ERROR: ConfigMon HARDWARE DEINSTALLED\n");
#endif
			return(1);
		}
	}
	if (event_type & SOFTWARE_INSTALLED) {
		strcpy(ebuffer, "ConfigMon SOFTWARE INSTALLED");
		if (!emapiSendEvent(0, curtime, CME_SW_INSTALLED, epri, ebuffer)) {
#ifdef CM_DEBUG
			fprintf(stderr, "send_cm_event: ERROR: ConfigMon SOFTWARE INSTALLED\n");
#endif
			return(1);
		}
	}
	if (event_type & SOFTWARE_DEINSTALLED) {
		strcpy(ebuffer, "ConfigMon SOFTWARE DEINSTALLED");
		if (!emapiSendEvent(0, curtime, CME_SW_DEINSTALLED, epri, ebuffer)) {
#ifdef CM_DEBUG
			fprintf(stderr, "send_cm_event: ERROR: ConfigMon SOFTWARE DEINSTALLED\n");
#endif
			return(1);
		}
	}
	if (event_type & SYSTEM_CHANGE) {
		strcpy(ebuffer, "ConfigMon SYSTEM CHANGE");
		if (!emapiSendEvent(0, curtime, CME_SYSTEM_CHANGE, epri, ebuffer)) {
#ifdef CM_DEBUG
			fprintf(stderr, "send_cm_event: ERROR: ConfigMon SYSTEM CHANGE\n");
#endif
			return(1);
		}
	}
	return(0);
}
