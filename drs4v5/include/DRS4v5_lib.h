
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <math.h>
#include <fstream>
#include <drsoscBinary.h>
#include <iostream>
#include <vector>


#define EVENTS_IN_A_FRAME 50

using namespace std;

class DRS_EVENT
{
	public:
	   EHEADER         	eheader;
	   vector<double *>	time;
	   vector<double *>	waveform;
} ;



int get_events( const char * fname="",double * waveformOUT=NULL,int start_eventID=0,int end_evetID=-1,bool offset_caliberate=false) asm ("get_events");

int do_offset_caliberation(string ofile="config/offset_cofig.dat",string configfile="drsosc.config");

int save_event_binary(const char * fname,DRS_EVENT event);
