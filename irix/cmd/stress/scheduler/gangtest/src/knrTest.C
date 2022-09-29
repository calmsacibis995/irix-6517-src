
#include <iostream.h>
#include <fstream.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "knrGang.H"
#include <ulocks.h>
/*********************************
  This is the test driver. It reads in a resource file and depending on the 
  input creates a number of Gangs with various properties.
**********************************/
// have to do this because stlib not yet compile with CC hmm.
extern "C" void wait(int*);

int main(int argc, char** argv){
   knrOutput* std_out = G_msg_output;
  if(argc < 1) {
    G_err_output->printf("no output file specified\n");
    exit(0);
    };
  if(argc == 3){
     std_out = new knrFileOutput(argv[2]);
  };

  ifstream rcfile(argv[1]);
  if(!rcfile){
    G_err_output->printf("output file specified could not be opened\n");
    exit(0);
    };
  usconfig(CONF_INITUSERS, 100);
  int num_gangs;
  int * members; 
  knrGang** gangs; 
  knrGangMaster::Mode* modes;
  int* delays;
  int* iterations;
  char* output_dest;
  char c;
  rcfile >> num_gangs; // read in the number of gangs 
  rcfile >> c;  // skip a filler in the file
   assert(num_gangs > 0 && num_gangs < 50);
   std_out->printf("Number of gangs: %d",num_gangs);
  members = new int[num_gangs];
  gangs = new knrGang*[num_gangs];
  modes = new knrGangMaster::Mode[num_gangs];
  delays = new int[num_gangs];
  iterations = new int[num_gangs];
  output_dest = new char[num_gangs];
 // Read in the file
  for(int i = 0; i < num_gangs; i++){
    rcfile >> output_dest[i]; // read how the user wants output
    // this is a sanity check on the file so far
    if((output_dest[i] != 'A') && (output_dest[i] != 'M')){
      G_err_output->printf("File is corrupted! %c",output_dest[i]);
      exit(1);
    } 
  
    rcfile >> c; // read in the mode for this gang
   
    switch(c){
    case 'G':
      modes[i] = knrGangMaster::GANG;
      break;
    case 'F':
      modes[i] = knrGangMaster::FREE;
      break;
   case 'S':
      modes[i] = knrGangMaster::SINGLE;
      break;
    case 'E':
      modes[i] = knrGangMaster::EQUAL;
      break;
    default: // a sanity check
      G_err_output->printf(" file corrupted \n");
      exit(0);
    }; 
  rcfile >> members[i]; // read in the number of members of this gang  
  if(members[i] > 1 && modes[i] == knrGangMaster::SINGLE){
       G_err_output->printf("In single mode can only have one member");
       exit(1);
    }
       rcfile >> iterations[i]; // read in the iterations
    if(iterations[i] < 1 ) {
       G_err_output->printf("Iterations must be greater than 0.");
       exit(1);
    }
       rcfile >> delays[i]; // read in the delays
    if(delays[i] <0){
       G_err_output->printf("Delay must be at least 0.");
       exit(1);
    };
    rcfile >> c; // read in a filler
  }
 // create the gangs
  for(i = 0; i < num_gangs; i++){
    gangs[i]= new knrGang(members[i],i,modes[i],std_out);
  }
  pid_t process;
  // start them off
  for(i=0; i < num_gangs; i++){
    G_dbg_output->printf("launching gangs \n");
    process = gangs[i]->start(iterations[i],output_dest[i] == 'A' ?
			      std_out: G_nul_output);
    sleep(delays[i]); 
    if(process == -1){
      G_dbg_output->printf(" did not fork correctly\n");
      assert(process != -1);
    };
  }
 // wait for them to finish
  for(i = 0; i < num_gangs; i++){
    G_dbg_output->printf("gangs remaining %d",num_gangs - i);
    int status;
    wait(&status);
  }

};
  






