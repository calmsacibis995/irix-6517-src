#ifndef __COMMON_SSDB_H_
#define __COMMON_SSDB_H_
/* 
 * Defines for the SSDB.
 */
#define SSS_SSDB_DBNAME            "ssdb"
#define SSS_SSDB_USERNAME          ""
#define SSS_SSDB_PASSWORD          ""
#define SSDB_EVENT_DATA_TABLE      "event"
#define SSDB_EVENT_TYPES_TABLE     "event_type"
#define SSDB_EVENT_CLASS_TABLE     "event_class"
#define SSDB_RULE_TABLE            "event_action"
#define SSDB_EVENT_RULE_TABLE      "event_actionref"
#define SSDB_ACTION_TAKEN_TABLE    "actions_taken"
#define SSDB_SYSTEM_DATA_TABLE     "system_data"
#define SSDB_TEST_DATA_TABLE       "test_data"
#define SSDB_AVAIL_DATA_TABLE      "availdata"
#define SSDB_TOOL_TABLE            "tool"
#define SSDB_SQL_DEFAULT_SIZE      512 /* Default size of sql strings. */
#define TOOLNAME                   "SEM"
#define GLOBAL_LOGGING             "GLOBAL_LOGGING_FLAG"
#define GLOBAL_THROTTLING          "GLOBAL_THROTTLING_FLAG"
#define GLOBAL_ACTION              "GLOBAL_ACTION_FLAG"

#define SSDB_RECORD_DELIMITER      "\07" /* delimiter between records. */


#endif /* __COMMON_SSDB_H_ */
