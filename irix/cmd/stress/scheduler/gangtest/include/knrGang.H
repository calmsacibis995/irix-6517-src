
#ifndef knrGANG__HEADER
#define knrGANG__HEADER
#include "knrTestSystem.H"
#include "knrGangMems.H"
/*************************************************************************
  Interface to a gang.
  To create a gang use start. As I wrote it currently start will create
  of ItGangMembers. To change that behaviour you have to override start.
**************************************************************************/
class knrGang {
  knrOutput* output_;
  knrGangMaster* master_;
  knrGang(knrGang& gang);
  knrGang& operator=(knrGang& gang);
  int gang_id_;
public:
  knrGang(){;}
  knrGang(int gang_members,
	  int gang_id,
	  knrGangMaster::Mode mode,
	  knrOutput* output = G_std_output);
  ~knrGang();
  virtual int start(int iterations,knrOutput* proc_output = G_std_output);

};

   
#endif




