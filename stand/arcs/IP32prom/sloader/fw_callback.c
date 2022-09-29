#include <arcs/restart.h>
#include <arcs/pvector.h>
#include <arcs/spb.h>
#include <sys/IP32.h>

/***************************************************************************
 * EnterInteractiveMode, Halt, PowerDown, Restart and Reboot are called	   *
 * from the SPB. They return control to the PROM to perform the actual     *
 * operation.                                                              *
 *                                                                         *
 * Note: In every instance, the finit routines are called.  This may slow  *
 * things down somewhat but it doesn't seem worth the trouble to avoid the *
 * minor performance hit.                                                  *
 ***************************************************************************/

#define FwCallBack  (*(void(*)(int,int))0xbfc00100)

/**********************
 * EnterInteractiveMode
 */
void
EnterInteractiveMode(void) {
  FwCallBack(FW_EIM,0);
}

/******
 * Halt
 */
void
Halt(void) {
  FwCallBack(FW_HALT,0);
}

/**********
 * PownDown
 */
void
PowerDown(void) {
  FwCallBack(FW_POWERDOWN,0);
}

/*********
 * Restart
 */
void
Restart(void) {
  FwCallBack(FW_RESTART,0);
}

/********
 * Reboot
 */
void
Reboot(void) {
  FwCallBack(FW_REBOOT,0);
}
