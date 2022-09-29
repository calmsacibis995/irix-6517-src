/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kostadis                                            */
/*    FILE: BPSSSPartition.H                                    */
/*    ASGN:                                                     */
/*    DATE: Sat Jul  8 15:33:05 1995                            */
/****************************************************************/


#ifndef BPSSSPARTITION_HEADER
#define BPSSSPARTITION_HEADER
#include <bps.h>
#include <BPSSSPartitionUnit.H>
#include <BPSServerThread.H>
#include <Thread.H>
#include <stl.h>
#include <BPS_lib.H>
class BPSAuthenticator;

/****************************************************************
  CLASS NAME : BPSSSPartition 

  The BPS, conceptually, manages allocations out of a rectangular of
  space/time. The space is the amount of memory and number of cpus
  physically available at any one instant in time. The length of the
  rectangular, the time, represents how long any allocation can be in
  time. The rectangular that the BPS manages can be further subdivided
  into soft system partitions, the BPSSSPartition, which have the same
  length in time, but whose space allocation changes over time. Since
  computers are digital, the rectangular is divided into constant
  discreet block of space/time. It is from these intervals of space
  time that allocations are actually made

  The BPSSSPartition, is responsible for managing a block of
  space/time from which jobs can allocate space/time to run. The
  allocations are made from discreet time intervals (digital vs analog
  machines you see) modelled as an arry of of BPSSSPartitionUnits. The
  BPSSSPartition gets its allocation from the BPSSSPartManager. It
  reads in its configuration from a file. The current format of the
  file for the BPSSSPartition is:

  <interval> <duration > <number of cpus> <memory> 

  For example, a BPSSSPartition that had an interval of 60 seconds, a
  duration of 1 day and laid claim to 5 cpus and 100 megabytes would
  look like this (all times in seconds):

    60 86400 5 100

  Note that the duration represents the maximum length of a job. The
  duration can not be larger than the global time, the interval can
  not be smaller than the global interval, the partition can not ask
  for more physical memory than is currently available or globally
  available and it can not ask for more cpus than are currently or
  physically available.



  The BPSSSPartitionUnit's can be accessed directly, but to get a unit
  you need to use the BPSSSPartitionIterator. An iterator handles the
  traversing of the data structure. It also represents an index into
  the data structure. The BPSSSPartitionIterator manages the
  conversion from time to an array index and ultimately unit.  The
  expected use of the BPSSSPartition is that of a wrapper around the
  units.

  The BPSSSPartition has the following components:
   
  a current time
  an interval 
  a max duration for a job
  an array size
  an array of BPSSSPartitionUnit
  an authenticator 

  The BPSSSPartition supports the following:

  the ability to query a segment of the array of BPSSSPartitionUnits
  the ability to authenticate one's self using the authenticator
  the ability to clone one's self
  the ability to modify the duration size, up to time length of the
    global pool
  given a unit you can get a time
  you can reset the time forward
  the ability to claim a block of space/time given a start unit and an
    end unit
  you can get the interval, duration, current time and the partition id

  NB: The types of partition that can be added to, by the user. 
  ****************************************************************/




class BPSSSPartition {
protected:
friend class BPSSSPartitionIterator;
friend class BPSSSPartManager;

   typedef vector<BPSPtrWrapper<BPSSSPartitionUnit> > sspuvec;
   static BPSSSPartition* list_; 
   BPSSSPartition* next_;
   
   BPSAuthenticator* authenticator_; // Handles authentication of principals
   bps_sspartition_id_t part_id_; // id of the partition
   bps_sspartition_id_t type_id_; // id of the type of the partition
   
   sspuvec *sspart_unit_; // partition unit array
   pthread_cond_t cond_; // used when deleting
   pthread_mutex_t mutex_; // used when deleting
   RWLock_reader rw_lock_; // used to lock the partition to prevent it
                           // from being deleted
   RWLock_writer val_lock_; // used to protect the internal data structures
   long array_size_;  // size of the sspu array
   int size_; // size of the sspu array
   long csecs_; // curtime in secs
   long dsecs_; // max length of a job 
   long isecs_; // interval that an sspu'
   Mutex refmut_; // mutex to protect the reference count
   int refcnt_; // reference count
   ncpu_t ncpu_; // max number of cpus (actual at any time can be different) 
   memory_t memory_; // max amount of memory (actual at any time can be different)
   
   // used to modify the memory available
   virtual error_t resetMemory(const memory_t& memory,
                               BPSSSPartManager* manager);
   
   // used to modify the amount of cpus available
   virtual error_t resetCPU(const ncpu_t& ncpus,
                            BPSSSPartManager* manager);
   // used to reset the maximum length of a job, can not be longer than
   // the length of the partition manager
   virtual error_t resetDuration(const bps_time_t& duration,
                                 BPSSSPartManager* manager);
public:
   BPSSSPartition(char* type_id);
   BPSSSPartition(const bps_sspartdata_t& data,long array_size);
   // deallocation of resources before deleting the object
   // simply deleting the object will not release the resource to the
   // manager. 
   virtual error_t releaseResources(BPSSSPartManager* manager);
   
   // authentication code
   virtual error_t authenticate(const bps_authentication_data_t& data);
   virtual error_t changePermissions(const bps_permissions_msg_t& data);
   
   // creates a copy of this type of partition using the 
   // filename as an input file
   virtual error_t clone(const bps_sspartition_id_t& type_id,
                         char* filename,
                         BPSSSPartition*& partition,
                         BPSSSPartManager* manager);
   
   // deletes the sspuvec_, and release all mutexes
   ~BPSSSPartition();
   virtual error_t query(bps_info_request_t& req,bps_info_reply_t* reply);
   
   // Add the number of CPU etc to time before.
   virtual error_t modify(caddr_t data, unsigned long size,BPSSSPartManager* manager);
 
   // the time that the current unit represents
   virtual bps_time_t unitTime(BPSSSPartitionUnit* unit);

   // resets the current time moving it forward and deallocating any resources
   // that were used.
   virtual void resetTime(bps_time_t time);
   // allocates a block of space/time starting at the start unit
   // and ending at the end_unit and sets the  bps_job_t parameters accordingly
   virtual error_t claimBlock(BPSSSPartitionUnit* start_unit,
                              BPSSSPartitionUnit* end_unit,
                              ncpu_t ncpus,
                              memory_t memory,
                              bps_job_t& jobt);

   // use to protect the internal values of the partition0
   bool_t values_rlock();
   bool_t values_wlock();
   bool_t values_unlock();
 
   // use to protect the partition from being deleted while you are using it.
   bool_t rlock();
   bool_t wlock();
   bool_t unlock();
   // accessors
   virtual ncpu_t getNcpu() const;
   virtual memory_t getMemory() const;
   virtual bps_time_t interval();
   virtual bps_time_t duration();
   virtual bps_time_t curTime();
   virtual void getID(bps_sspartition_id_t& id);

   // need this because different partitions can have different iterators
   BPSSSPartitionIterator* getIterator();
};

class BPSSSPartitionIterator {
protected:
   BPSSSPartition* controller_;
   long csecs_;
   BPSSSPartition::sspuvec::iterator index_;
   BPSSSPartition::sspuvec::iterator start_;
public:
   BPSSSPartitionIterator(BPSSSPartition* controller);
   ~BPSSSPartitionIterator();

   // what point in time to start iterating from
   virtual bool_t start(const bps_time_t& time);
   // what point to stop iterating at 
   virtual bool_t stop(const bps_time_t& time);
   // returns the current unit
   virtual BPSSSPartitionUnit* current();
   // get the next unit.
   virtual bool_t next();
};
  

#endif








