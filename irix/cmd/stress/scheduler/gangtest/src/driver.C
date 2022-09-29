/********************************
 This program interactively creates the data file for use
 by gtest.
 This is not meant as the ultimate interface, but rather
 a simpler interface than a text file.

 1. Enter file name
 2. Enter the number of the gangs n
 for each gang i
  Enter gang data
  copy?
   y -> make x copies such that x+i < n
   if x + i > n abort and retry
 
 3. Write to file 
 4. Ask whether the user wants to execute the proram?

********************************/

#include <iostream.h>
#include <fstream.h>
#include <iomanip.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
int main(int argc, char** argv){

 if(argc < 2) {
    cerr << " ERROR: No output file specifieid " << endl;
    exit(1);
 };
 
 ofstream out_put(argv[1]);
 if(!out_put){
    cerr << " ERROR: Could not open output file. " << endl;
    exit(1);
 }
 int num_gangs;
 cout << " Q1 : How many gangs :";
 cin >> num_gangs;
 if(num_gangs < 1 || num_gangs >  100){
    cerr << " ERROR: Range between 1 and 100. " << endl;
    exit(1);
 }
 out_put << num_gangs << endl;

 for(int i = 0; i < num_gangs; i++){
    out_put << '-' << endl;
    char out_type;
    do {
       cout << " Gang[" << i << "] Q1: Output all thread times (A) or only gang times(M):";
       cin >> out_type;
       if(out_type != 'M' && out_type != 'A'){
          cerr << "ERROR: Invalid Input!" <<endl;
          cerr << "Please enter either A for thread and gang times " << endl;
          cerr << "or M for gangs times only. " <<endl;
       }
    }while(out_type != 'M' &&  out_type != 'A');
    out_put << out_type << endl;
    char mode;
    do {
       cout << " Gang[" << i << "] Q2: What scheduling mode (E)qual,(F)ree,(G)ang,(S)ingle: ";
       cin >> mode;
       if(mode != 'E' && mode != 'F' && mode != 'G' && mode != 'S'){
          cerr << "ERROR: Invalid input, please input (E/F/G/S). " << endl;
       }
    } while(mode != 'E' && mode != 'F' && mode != 'G' && mode != 'S');
    out_put << mode << endl;
    int gang_member;
    do {
       cout << " Gang[" << i << "] Q3: How many gang members: ";
       cin >> gang_member;
       if(gang_member < 0 || (gang_member != 1 && mode == 'S')){
          cerr << "ERROR: Invalid input! " << endl 
             << "Either gang_members < 0 or more than one member specified for single mode. " << endl;
       }
    }while (gang_member < 0 || (gang_member != 1 && mode == 'S')) ;
    out_put << gang_member << endl;
    int iterations;
    do{
       cout << " Gang[" << i << "] Q4: How many iterations: ";
       cin >> iterations;
       if(iterations < 1){
          cerr << "ERROR: Invalid Input! " << endl;
          cerr << "Input a value greater than 0. " << endl;
       }
    }while(iterations < 1);
    out_put << iterations << endl;
    int delay;
    do{
       cout << " Gang[" << i << "] Q5: Delay in seconds before next gang launched: ";
       cin >> delay;
       if(delay <0){
          cerr <<"ERROR: Invalid Input!" <<endl;
          cerr <<"Enter a delay between 0->MAXINT. "<<endl;
       }
    }while(delay < 0);
    out_put << delay << endl;

    int repeat = 0;
    if(num_gangs != 1 &&  (i-num_gangs != 1)) {
    do{
       cout << " How many times do you want to repeat Gang[" << i << "]: ";
       cin >> repeat;
       if(repeat + i >= num_gangs){
          cerr << "ERROR : Invalid Input!" << endl;
          cerr << "You tried to create more gang definitions than you " << endl;
          cerr << "have gangs. "<< endl; 
       }
    }while(repeat +i >= num_gangs);
 }
    i += repeat;
    for(int x = 0; x < repeat; x++){
       out_put << '-' << endl <<  out_type << endl << mode << endl << gang_member << endl << iterations << endl << delay << endl;
    }
 }
 char answer;
 char buf[256]; 
 char *bufptr = buf;
 cout << " Do you want to execute gtest now?";
 cin >> answer;
 char** args = new char*[4];
 args[0] = new char[strlen("/usr/people/kostadis/src/gtest")+1];
 strcpy(args[0],"/usr/people/kostadis/src/gtest");
 args[1] = new char[strlen(argv[1])];
 strcpy(args[1],argv[1]);
 do {
    if (answer == 'N') {
       exit(1);
    };
    out_put.close();
       char answer2;    
    if(answer == 'Y'){

       cout << " Do you want to send output to a file (Y/N)? ";
       cin >> answer2;
    }
       if(answer2 == 'Y'){
          cout << " File name : ";
          cin >> bufptr;  
          bufptr[-1] = '\0';
          args[2] = new char[strlen(buf + 1)];
          strcpy(args[2],buf);
          args[3] = 0;
       }
       else{
          args[2] = 0;
       }
    if(execvp("gtest",&args[0]) == -1){
       cerr << "ERROR : could not exec gtest" << endl;
       exit(1); 
    } 
 
 } while(answer != 'Y' && answer != 'N');
 cerr << " DEBUG: GOT TO HERE! " << endl;
 return 1;
}

