#ifndef __COMMON_EVENTS_H_
#define __COMMON_EVENTS_H_

#define WILDCARD_SYSID              0 /* For generic rules of SGM */

#define WILDCARD_EVENT             -1 /* 
				       * Any event with this in the class/type 
				       * field is treated as *any* event within
				       * the class if the type is a wildcard or
				       * *any* class if the class is a wildcard.
				       * This is useful in rules.
				       */
/*
 * Important: Nobody else should reuse these event class definitions.
 */
#define SSS_INTERNAL_CLASS         4004
#define CONFIG_EVENT_TYPE          0x00200300
#define ERROR_EVENT_TYPE           0x00200301

#define NUM_SSS_EVENTS             2 /* events especially for SSS. */
#define NUM_SSS_RULES              0 /* rules especially for SSS. */


/* 
 * Availmon event class. We just keep this around as this is an
 * important class and if the class->table mapping does not work we can
 * atleast use this hard-coded value for availmon event.
 */
#define AVAIL_EVENT_CLASS          4000

#endif /* __COMMON_EVENTS_H_ */

