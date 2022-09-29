/****************************************************************
 *    NAME: 
 *    ACCT: kostadis
 *    FILE: BPSSSPartitionUnit.C
 *    DATE: Mon Jul 10 17:50:15 PDT 1995
 ****************************************************************/

#include "BPSSSPartitionUnit.H"
#include <assert.h>
#include <bps.h>
/*************************************************************************
 * Function Name: getFreeMemory
 * Parameters: 
 * Returns: memory_t 
 * Effects: 
 *************************************************************************/
memory_t 
BPSSSPartitionUnit::getFreeMemory() const
{
   return free_memory_;
}


/*************************************************************************
 * Function Name: getFreeCPU
 * Parameters: 
 * Returns: ncpu_t 
 * Effects: 
 *************************************************************************/
ncpu_t 
BPSSSPartitionUnit::getFreeCPU() const
{
   return free_processors_;
}


/*************************************************************************
 * Function Name: claimMemory
 * Parameters: const memory_t& memory
 * Returns: bool_t 
 * Effects: 
 *************************************************************************/
bool_t 
BPSSSPartitionUnit::claimMemory(const memory_t& memory)
{
   assert(free_memory_ - memory >= 0);
   free_memory_ -= memory;
   return 1;
}


/*************************************************************************
 * Function Name: freeMemory
 * Parameters: const memory_t& memory
 * Returns: bool_t 
 * Effects: 
 *************************************************************************/
bool_t 
BPSSSPartitionUnit::freeMemory(const memory_t& memory)
{
   free_memory_ += memory;
   return 1;
}


/*************************************************************************
 * Function Name: claimCPU
 * Parameters: const npu_t& ncpu
 * Returns: bool_t 
 * Effects: 
 *************************************************************************/
bool_t 
BPSSSPartitionUnit::claimCPU(const ncpu_t& ncpu)
{
   assert( free_processors_ - ncpu >= 0);
   free_processors_ -= ncpu;
   return 1;

}


/*************************************************************************
 * Function Name: freeCPU
 * Parameters: const npu_t& ncpu
 * Returns: bool_t 
 * Effects: 
 *************************************************************************/
bool_t 
BPSSSPartitionUnit::freeCPU(const ncpu_t& ncpu)
{
   free_processors_ += ncpu;
   return 1;
}


/*************************************************************************
 * Function Name: rlock
 * Parameters: 
 * Returns: bool_t 
 * Effects: 
 *************************************************************************/
bool_t 
BPSSSPartitionUnit::rlock()
{

   return 1;
}
/*************************************************************************
 * Function Name: wlock
 * Parameters: 
 * Returns: bool_t 
 * Effects: 
 *************************************************************************/
bool_t 
BPSSSPartitionUnit::wlock()
{

   return 1;
}


/*************************************************************************
 * Function Name: unlock
 * Parameters: 
 * Returns: bool_t 
 * Effects: 
 *************************************************************************/
bool_t 
BPSSSPartitionUnit::unlock()
{

  return 1;
}



/*************************************************************************
 * Function Name: setFreeMemory
 * Parameters: 
 * Returns: bool_t 
 * Effects:  assumes that free_memory_ < memory
 *************************************************************************/
bool_t 
BPSSSPartitionUnit::
setFreeMemory(const memory_t& memory){

   free_memory_ = memory;
   return 1;
}
/*************************************************************************
 * Function Name: setFreeCPU
 * Parameters: 
 * Returns: bool_t 
 * Effects:  assumes that free_memory_ < memory
 *************************************************************************/
bool_t 
BPSSSPartitionUnit::
setFreeCPU(const ncpu_t& ncpu){
   free_processors_ = ncpu;
   return 1;
};

bool_t BPSSSPartitionUnit::isContiguous(BPSSSPartitionUnit* unit){
   if((1 == unit->index_ - index_) || (-1 ==  unit->index_-index_)){
      return 1;
   }
   return 0;
};




BPSSSPartitionUnit::BPSSSPartitionUnit(ncpu_t ncpus,
                                            int index,
                                            memory_t memory){
   free_processors_ = ncpus;
   index_ = index;
   free_memory_ = memory;
};
   



ostream& operator << (ostream& os, BPSSSPartitionUnit& unit){
   os << "UNIT [" << unit.index_ << "] memory " << unit.free_memory_ << " cpus " << unit.free_processors_;
   return os;
};

          
bool operator==(const BPSSSPartitionUnit&,
                const BPSSSPartitionUnit&){
   assert(1==0);
   return 1;
};

bool operator<(const BPSSSPartitionUnit& ,
               const BPSSSPartitionUnit& ){
   assert(1==0);
   return 0;
};

bool operator>(const BPSSSPartitionUnit&,
               const BPSSSPartitionUnit&){
   assert(1==0);
   return 0;
};

