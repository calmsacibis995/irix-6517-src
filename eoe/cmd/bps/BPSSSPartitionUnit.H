/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kostadis                                            */
/*    FILE: BPSSSPartitionUnit.H                                           */
/*    ASGN:                                                     */
/*    DATE: Sat Jul  8 15:30:17 1995                            */
/****************************************************************/


#ifndef BPSSSPartitionUnit_HEADER
#define BPSSSPartitionUnit_HEADER
#include <bps.h>
#include <Thread.H>

#include <iostream.h>
#include <bool.h>

class BPSSSPartitionUnit {
protected:
friend class BPSSSPartition;
   ncpu_t free_processors_;
   memory_t free_memory_;
   int index_;
   int index(){return index_;}
public:
   // create a unit
   BPSSSPartitionUnit(ncpu_t ncpus, int index, memory_t memory);
   // set the  memory or cpus to a new value,
   // since the unit does NOT keep track of max memory or cpu
   // it is incumbent of the client to manage that. A more intelligent unit
   // would managed that resource, but that would make  setting the new free
   // memory more complex
   bool_t setFreeMemory(const memory_t& memory);
   bool_t setFreeCPU(const ncpu_t& memory);
   // returns the current free memory
   memory_t getFreeMemory() const;
   ncpu_t getFreeCPU() const;

   // claims resource, if resource > free_resource false is returned
   bool_t claimMemory(const memory_t& memory);
   bool_t claimCPU(const ncpu_t& ncpu);

   // frees resource, does not check if overflow
   bool_t freeMemory(const memory_t& memory);
   bool_t freeCPU(const ncpu_t& ncpu);
   // returns true if two units follow each other
   bool_t isContiguous(BPSSSPartitionUnit* unit);
   // locks the unit
   virtual bool_t rlock();
   virtual bool_t wlock();
   virtual   bool_t unlock();



friend ostream& operator << (ostream& os, BPSSSPartitionUnit& unit);

   
friend bool operator==(const BPSSSPartitionUnit& unit,
                       const BPSSSPartitionUnit& unit1);
   
friend bool operator<(const BPSSSPartitionUnit& unit,
                      const BPSSSPartitionUnit& unit1);
   
friend bool operator>(const BPSSSPartitionUnit& unit,
                      const BPSSSPartitionUnit& unit1);
   
}; 


#endif
