
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <math.h>
#include <fstream>
#include <iostream>
#include <vector>

#include <drsoscBinary.h>

#define EVENT_SIZE_BYTES_4channelADC 32796

#define EVENTS_IN_A_FRAME 50

#define TERMINAL_RESISTANCE 50

using namespace std;

class DRS_EVENT
{
	public:
	   EHEADER         	eheader;
	   vector<float *>	time;
	   vector<float *>	waveform;
	   void clear_data()
	   {
		   std::cout<<"here for "<<(this->eheader).event_serial_number;
		   	vector<float *>::iterator t_itr=time.begin(); 
		   	vector<float *>::iterator w_itr=waveform.begin();
		   	while (t_itr!=time.end())
		   	{
		   		delete [] *t_itr;
		   		delete [] *w_itr;
		   		t_itr++;
		   		w_itr++;
		   	} 
		   std::cout<<"  DONE !!\n";
	   }
} ;



int get_events( const char * fname="",double * waveformOUT=NULL,int start_eventID=0,int end_evetID=-1,bool offset_caliberate=false) asm ("get_events");
int get_event_adcSave(const char * fname,double * waveformOUT,int start_eventID=0,int end_evetID=-1) asm ("get_event_adcSave") ;

int do_offset_caliberation(string ofile="config/offset_cofig.dat",string configfile="drsosc.config");

int get_channel_offsets(string ofile="calib/offset_calib.dat",vector<double *> *calib_data =NULL,int channels[]=NULL);

int save_event_binary(const char * fname,DRS_EVENT events[], int event_count);
vector<DRS_EVENT> read_event_binary(const char * fname);

double get_energy(float waveform[8][1024],float time[8][1024],int channel,
						double trigger_level=-40.0,double neg_offset=20,double integrate_window=100, double freq =5.12,
									bool falling_edge=true) asm("get_energy");
