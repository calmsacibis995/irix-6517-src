// File system stress creation program.

#include <assert.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stream.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Node actions.

enum action_t { mkDir,		// Create a directory.
		rmDir,		// Remove a directory.
		create,		// Create a plain file.
		verify,		// Verify a file.
		update,		// Update a file.
		rm,		// Remove a plain file.
		descend,	// Descend into a directory.
		accend,		// Assend from a directory.
		remove_all,	// Remove everything in a directory.
		last_action};	// Terminates list.


const int MAX_ENTRIES=32;
const int ChunkMax=400;
const char *ResourceHeader="/.HSResource";

// Basic directory node

class Node{

private:

  enum etype_t {empty,file,directory};

  static int Tally[last_action];// Action tally.
  static int Errors[last_action];// Action errors.
  static int Count;

  int	Depth;			// Depth of this node.
  int   Cnt;			// Number of entries in this node.
  char  Path[256];		// Path to this node.
  Node *Parent;		// Parent node.
  struct{
    char    Name[32];		// Name of entry.
    etype_t Type;		// Type of entry.
    mode_t  Mode;		// File/directory mode.
    Node  *Child;		// Child node.
  } entry[MAX_ENTRIES];


  void Node::RandomName();	// Generate new random entry name name.
  void Remove(int n);		// Delete an entry from the node.
  int  Select(etype_t Type);	// Select an entry of a given type.

  void MakeDirectory(void);	// Create a directory entry.
  void RemoveDirectory(int);	// Remove a directory entry.
  void CreateFile(void);	// Create a file entry.
  void VerifyFile(int);		// Verify a file entry.
  void UpdateFile(int);		// Update a file.
  void RemoveFile(int);		// Remove a file entry.

  void Review(void);		// Debugging.

  int Heads(){return random() & 1;};	// Returns 0 or 1 at random.

public:
  void RemoveAll(void);		// Remove all entries.
  void RandomAct(Node **n);	// Performs a random action.
  void Status();		// Display action status.
  void Ls();			// Directory listing.

  Node(char *Path, int Depth, Node *Parent);
  ~Node();			// Destroy a node.

};

int Node::Tally[last_action];
int Node::Errors[last_action];
int Node::Count;

int Verbose;

//////////////
// Node::~Node
//
Node::~Node(void){}


////////
// Usage
//
void Usage(){
  cout << " Usage:\tstress [-c <count>] [-v] [-s <seed> ] <path>\n";
  exit(1);
}


////////
// Error
//
void Error(char *cmd, char *txt,int errno){
  cout << '\n' << cmd << ": " << txt << " -- " << strerror(errno) << '\n';
}

void Error(char *cmd, char *txt){
  cout << cmd << ": " << txt << '\n';
}


///////////////
// Node::Remove
//
void Node::Remove(int sel){

  delete entry[sel].Child;

  memmove(&entry[sel],&entry[sel+1],(Cnt-1-sel)*sizeof(entry[0]));

  entry[Cnt-1].Name[0]=0;
  entry[Cnt-1].Type=empty;
  entry[Cnt-1].Child = 0;
}


/////////////
// Node::Node	Create a node.
//
Node::Node(char *path, int depth, Node *parent) 
: Depth(depth), Parent(parent), Cnt(0){
  strcpy(Path,path);
  memset(&entry[0], 0, sizeof this->entry);
}



//////////////////
// Node::RandomAct	Perform a random node action.
//
void Node::RandomAct(Node **newnode){
  *newnode = this;
  int sel;
  action_t act;

 again:
  switch( act = (action_t)(random()%last_action) ){

  case mkDir:
    if ((Cnt == MAX_ENTRIES) || Depth==15 )
      goto again;
    MakeDirectory();
    break;

  case rmDir:
    if ((sel = Select(directory))<0)
      goto again;
    RemoveDirectory(sel);
    break;

  case create:
    if ((Cnt == MAX_ENTRIES) || Depth==15 )
      goto again;
    CreateFile();
    break;

  case verify:
    if ((sel = Select(file))<0)
      goto again;
    VerifyFile(sel);
    break;

  case update:
    if ((sel = Select(file))<0)
      goto again;
    UpdateFile(sel);
    break;

  case rm:
    if ((sel = Select(file))<0)
      goto again;
    RemoveFile(sel);
    break;

  case descend:
    if ((sel = Select(directory))<0)
      goto again;
    *newnode = entry[sel].Child;
    break;

  case accend:
    if (Depth==0)
      goto again;
    *newnode = Parent;
    break;

  case remove_all:
    if ((random()%6)<5)
      break;
    RemoveAll();
    break;

  default:
    goto again;
  }

  Count++;
  Tally[act]++;
  cout << Count << '\r';
}


///////////
// Node::Ls
//
void Node::Ls(void){

  cout << '\n';

  cout << form("Node: %x, Count: %d, Depth: %d, Cnt: %d\n",
	       this, Count, Depth, Cnt);

  for (int i = 0; i<Cnt;i++)
    cout << '\t' <<  entry[i].Type << '\t'
	 << entry[i].Name << "   " << entry[i].Child <<'\n';

  cout << '\n';
}


//////////////////////
// Node::MakeDirectory
//
void Node::MakeDirectory(void){
  char tpath[256];

  RandomName();
  entry[Cnt].Type = directory;
  entry[Cnt].Mode = Heads() ? 0777 : 0444;

  strcpy(tpath,Path); strcat(tpath,entry[Cnt].Name);

  if (Verbose)
    cout << form("MakeDirectory: %s [%o]\n", tpath, entry[Cnt].Mode);

  if (mkdir(tpath,entry[Cnt].Mode)){
    Error("mkdir",tpath,errno);
    Errors[mkDir]++;
    return;
  }


  entry[Cnt].Child = new Node(tpath,Depth+1,this);
  Cnt++;
}


////////////////////////
// Node::RemoveDirectory
//
void Node::RemoveDirectory(int sel){
  char tpath[256];
  strcpy(tpath,Path); strcat(tpath,entry[sel].Name);

  if (Verbose)
    cout << form("RemoveDirectory: %s [%o]\n", tpath, entry[Cnt].Mode);

  if (rmdir(tpath)){
    if (errno!=EEXIST)
      Error("rmDir", tpath, errno);
    Errors[rmDir]++;
    return;
  }
  Remove(sel);
  Cnt--;
}


///////////////////
// Node::CreateFile
//
void Node::CreateFile(void){
  char tpath[256];
  int  i,j,fd,chunks= (int)random() % 15;
  int  buffer[ChunkMax];

  RandomName();
  entry[Cnt].Type = file;
  entry[Cnt].Mode = Heads() ? 0777 : 0444;

  strcpy(tpath,Path);
  if (Heads())
    strcat(tpath,ResourceHeader);
  strcat(tpath,entry[Cnt].Name);

  if (Verbose)
    cout << form("CreateFile: %s [%o]\n", tpath, entry[Cnt].Mode);

  if ((fd = open(tpath,O_RDWR|O_CREAT,entry[Cnt].Mode))<0){
    Error("create",tpath, errno);
    Errors[create]++;
    return;
  }
  for(j=0;j<chunks;j++){
    for (i=0;i<ChunkMax;i++)
      buffer[i] = (j<<16) | i;
    if (write(fd,buffer,sizeof buffer)!=sizeof buffer){
      Error(" Write error", "", errno);
      Errors[create]++;
      break;
    }
    }
  close(fd);
  Cnt++;
}


///////////////////
// Node::VerifyFile
//
void Node::VerifyFile(int sel){
  char tpath[256];
  int  i,j,fd;
  int  buffer[ChunkMax];

  strcpy(tpath,Path);
  if (Heads())
    strcat(tpath,ResourceHeader);
  strcat(tpath,entry[sel].Name);

  if (Verbose)
    cout << form("VerifyFile: %s [%o]\n", tpath, entry[sel].Mode);

  if ((fd=open(tpath,O_RDONLY))<0){
    Error("verify",tpath,errno);
    Errors[verify]++;
    return;
  }
  for (j=0;;j++)
    if ((i=read(fd,buffer,sizeof buffer))==sizeof buffer){
      for (i=0;i<ChunkMax;i++)
	if (buffer[i]!= (j<<16)+i){
	  Error("verify","");
	  Errors[verify]++;
	  goto done;
	}
    }
    else
      if (i==0)
	goto done;
      else{
	Error("verify","read error",errno);
	Errors[verify]++;
	goto done;
      }
 done:
  close(fd);
}


///////////////////
// Node::UpdateFile
//
void Node::UpdateFile(int sel){
  char tpath[256];
  int  i,j,fd;
  int  buffer[ChunkMax];
  struct stat sb;
  
  strcpy(tpath,Path);
  if (Heads())
       strcat(tpath,ResourceHeader);
  strcat(tpath,entry[sel].Name);
  
  if (Verbose)
       cout << form("UpdateFile: %s [%o]\n", tpath, entry[sel].Mode);
  
  if ((fd=open(tpath,O_RDWR))<0){
    Error("update",tpath,errno);
    Errors[update]++;
    return;
  }
  
  if (fstat(fd,&sb)){
    Error("update","stat error", errno);
    Errors[update]++;
    goto done;
  }
  i = (int)(sb.st_size / sizeof buffer);
  j = (int)random() % (i+1);
  lseek(fd, j * sizeof buffer,SEEK_SET);
  for (i=0;i<ChunkMax;i++)
       buffer[i]=(j<<16)+i;

  if (write(fd,buffer,sizeof buffer)!=sizeof buffer){
    Error("update", "write error", errno);
    Errors[update]++;
  }
  
 done:
   close(fd);
}


///////////////////
// Node::RemoveFile
//
void Node::RemoveFile(int sel){
  char tpath[256];

  strcpy(tpath,Path); strcat(tpath,entry[sel].Name);

  if (Verbose)
    cout << form("RemoveFile: %s [%o]\n", tpath, entry[sel].Mode);

  if (unlink(tpath)){
    Error("rm",tpath,errno);
    Errors[rm]++;
    return;
  }
  Remove(sel);
  Cnt--;
}


//////////////////
// Node::RemoveAll
//
void Node::RemoveAll(void){
  int i;

  for (i=Cnt;i>0;i--)
    if (entry[i-1].Type==file)
      RemoveFile(i-1);
    else{
      if (entry[i-1].Child->Cnt)
	entry[i-1].Child->RemoveAll();
      RemoveDirectory(i-1);
    }
}


///////////////
// Node::Select	Select an entry according to type.
//
int Node::Select(etype_t t){
  int v[MAX_ENTRIES];
  int max=-1;
  for (int i=0;i<Cnt;i++)
    if (entry[i].Type==t)
      v[++max]=i;
  if (max<0)
    return max;
  return v[random() % (max+1)];
}


///////////////
// Node::Status
//
void Node::Status(void){
  cout << '\n' << form("Action tally:\n"
		       "  mkDir: %4d, rmDir:   %4d, create: %4d, verify: %4d\n"
		       "  rm:    %4d, descend: %4d, accend: %4d, rm -rf: %4d\n"
		       "  update:%4d",
		       Tally[mkDir],
		       Tally[rmDir],
		       Tally[create],
		       Tally[verify],
		       Tally[rm],
		       Tally[descend],
		       Tally[accend],
		       Tally[remove_all],
		       Tally[update]
		     );
  cout << '\n' << form("Error tally:\n"
		       "  mkDir: %4d, rmDir:   %4d, create: %4d, verify: %4d\n"
		       "  rm:    %4d, descend: %4d, accend: %4d, rm -rf: %4d\n"
		       "  update:%4d\n",
		       Errors[mkDir],
		       Errors[rmDir],
		       Errors[create],
		       Errors[verify],
		       Errors[rm],
		       Errors[descend],
		       Errors[accend],
		       Errors[remove_all],
		       Errors[update]
		     );
}


///////////////////
// Node::RandomName
//
void Node::RandomName(){
  char buf[32], *cp;
  long l;

 again:
  cp=buf;
  l= 1 + random() % 13;
  *cp++ = '/';
  while (l--)
    *cp++ = (char)((random() % 26)+'A');
  *cp=0;
  for (l=0;l<Cnt;l++)
    if (strcmp(buf,entry[l].Name)==0)
      goto again;
  strcpy(entry[Cnt].Name,buf);
}


///////
// main
//
main(int argc, char **argv){
  
  Node *cur;
  long count=10;
  int c;

  // Initialize.

  while ((c=getopt(argc,argv,"c:vs:"))!=-1)
    switch(c){
    case 'c': count = strtol(optarg,NULL,0); break;
    case 'v': Verbose=1; break;
    case 's': srandom((int)strtol(optarg,NULL,0)); break;
    default: Usage();
    }
  if (optind>=argc)
    Usage();
  cur = new Node(argv[optind],0,NULL);


  // Main loop.
  // Randomly select an action and perform it.

  cout.setf(ios::unitbuf);

  for(int i=0;i<count;i++){
    errno=0;
    cur->RandomAct(&cur);
    if (errno==ENOSPC)
      cur->RemoveAll();
    if (errno==ENXIO)
      break;
  }
  cur->Status();
  return 0;
}







