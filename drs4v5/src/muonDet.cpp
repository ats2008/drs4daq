/********************************************************************\

  Name:         drs_exam.cpp
  Created by:   Stefan Ritt

  Contents:     Simple example application to read out a DRS4
                evaluation board

  $Id: drs_exam.cpp 21308 2014-04-11 14:50:16Z ritt $

\********************************************************************/

#include <math.h>

#ifdef _MSC_VER

#include <windows.h>

#elif defined(OS_LINUX)

#define O_BINARY 0

#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DIR_SEPARATOR '/'
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctime>

#include "strlcpy.h"
#include <fstream>
#include "DRS.h"
#include <DRS4v5_lib.h>

#define UPADATE_STATS_INTERVAL 20
/*------------------------------------------------------------------*/




double get_energy(float waveform[8][1024],float time[8][1024],int channel,
						double trigger_level=-40.0,double neg_offset=20,double integrate_window=100, double freq =5.12);



int main()
{
   int i, j, nBoards;
   DRS *drs;
   DRSBoard *b;
   float time_array[4][1024];
   float wave_array[4][1024];

   fstream file;
   string run_name="atsrun",energy_str,event_str,temp_str;
   int event_counter=0,channel=3,skip_evts=2;
   bool infinite=false;
   bool save_waveform=false;
   int updates_stats_interval=UPADATE_STATS_INTERVAL;
   int event_rate;
   
   time_t start_t = time(0);
   time_t curr_t,diff;
   tm* elapsed_t ;
   char* dt=ctime(&start_t);
   cout<<"\n Current time  = "<<dt;

	save_waveform=true;
    cout<<"Enter the data run name \t:\t";
			cin>>run_name;
	 
   cout<<"Enter number of events to be recorded ( -1 for infinite loop ) : ";
   			cin>>event_counter;
  	if(event_counter == -1) 
  		infinite=true;
  		
   cout<<"Enter channenl of interest [1,2,3 or 4] : ";
   			cin>>channel;
   	if (channel<1 or channel>4)
   		{
   			cout<<"\n please enter a valid channel ID (1,2,3,4) !";
   			return 1;
   		}
   			channel--;
 
   cout<<"Enter the number of waveforms to be skipped before writing to disc ( -1 for not any saving events ) : ";
   			cin>>skip_evts;
   		if(skip_evts!=-1)
   		{
   			save_waveform=true;
			skip_evts++;
   		}
		if(skip_evts<-1)
			{
				printf("ENTER VALID RANGE (1,2,..) OR -1 !!\n");
				return 0;
			}
	char remark_option='n';
   	event_str="mkdir -p data/"+run_name;
	system(event_str.c_str());
	cout<<"\n Do you want to enter any remarks [y/n] ?\t:\t";
		cin>>remark_option;
	if(remark_option=='y' or remark_option=='Y')
		{
			temp_str="nano data/"+run_name+"/remarks.txt";
			system(temp_str.c_str());
		}
	else
	{
		temp_str="touch data/"+run_name+"/remarks.txt";
		system(temp_str.c_str());
	}
   
   energy_str="data/"+run_name+"/eDeposit.txt";
   file.open(energy_str.c_str(), ios::out);/* open file to save waveforms */
   if (!file.is_open()) 
   {
      perror("ERROR: Cannot open file \"data.txt\"");
      return 1;
   }
   file.close();
   
   event_str="data/"+run_name+"/events.dat";
   file.open(event_str.c_str(), ios::out|ios::binary);/* open file to save waveforms */
   if (!file.is_open())
   {
      perror("ERROR: Cannot open file \"data.txt\"");
      return 1;
   }
   file.close();
	system("clear");
    drs = new DRS(); 			/* do initial scan */
   
   for (i=0 ; i<drs->GetNumberOfBoards() ; i++) 
   {
      b = drs->GetBoard(i);		/* show any found board(s) */
      printf("Found DRS4 evaluation board, serial #%d, firmware revision %d\n", 
      b->GetBoardSerialNumber(), b->GetFirmwareVersion());
   }
   
   nBoards = drs->GetNumberOfBoards();
   if (nBoards == 0) /* exit if no board found */
   {
      printf("No DRS4 evaluation board found\n");
      return 0;
   }
   b = drs->GetBoard(0);		/* continue working with first board only */
   b->Init();					/* initialize board */
   b->SetFrequency(5, true);	/* set sampling frequency */
   b->SetTranspMode(1);			/* enable transparent mode needed for analog trigger */
   b->SetInputRange(0);			/* set input range to -0.5V ... +0.5V */

   if (b->GetBoardType() >= 8) 
   {           
      b->EnableTrigger(1, 0);            	//enable hardware trigger
      //b->SetTriggerSource(0x0B00);        	//AND Bit8=CH1, Bit9=CH2, Bit11=CH4 =>hex 0B00
      b->SetTriggerSource(0x0010);
   } 
   else 
   	{
    	printf(" Old version of board found !! exiting ");
    	return 0;
   	}
   	
   	start_t = time(0);
	dt=ctime(&start_t);
    cout<<"\nEvent Monitoring Stared at \t:\t   "<<dt;
    cout<<"\nenergy integrals written to  \t:\t"<<energy_str;
    if(save_waveform)
    	{
	    	cout<<"\nwaveforms are written to\t:\t"<<energy_str;
 		cout<<" [ One out of every "<<skip_evts<<" saved]";
    	}
    if(infinite)
    	cout<<"\nOn cotinious run .. pres Ctrl + c to quit\n";
 	else 
 		cout<<"\nNumber of events to be monitored :\t"<<event_counter;
 	cout<<"\nChannel to be integrated  : "<<channel+1<<"\n\n";
 	cout<<"\nRemarks \t: \n";
 	temp_str="cat data/"+run_name+"/remarks.txt";
 	system(temp_str.c_str());
 	cout<<"\n";
	
   	b->SetTriggerPolarity(true);        // false :positive edge
   
   double energy=0;	
   float trigger_level=-0.04;
   b->SetIndividualTriggerLevel(0, trigger_level);
   b->SetIndividualTriggerLevel(1, trigger_level);
   b->SetIndividualTriggerLevel(3, trigger_level);
   
   // b->SetTriggerSource(0xB00);  //For internal 3 forld coincidance
   b->SetTriggerSource(0x0010);      //For external OR Trigger
   
   b->SetTriggerDelayNs(50);             // 50 ns trigger delay
   
   unsigned long int eid=0;
   DRS_EVENT muEvent[1];
   for(int i=0;i<4;i++)
   {
	   muEvent[0].time.push_back(time_array[i]);
	   muEvent[0].waveform.push_back(wave_array[i]);
   }
   
	 curr_t = time(0);
     diff=curr_t;
	 event_rate = int(float(updates_stats_interval)/int(diff-curr_t+1));
	 dt=ctime(&curr_t);
	 fflush(stdout);
	 cout<<"\tCurrent time\t:\t"<<dt;
	 diff=curr_t-start_t;
	 elapsed_t = gmtime(&diff);
	 cout<<"\tElapsed time\t:\t"<<elapsed_t->tm_mday-1<<"days ";
	 cout<<elapsed_t->tm_hour<<"hrs ";
	 cout<<elapsed_t->tm_min<<"min ";
	 cout<<elapsed_t->tm_sec<<"sec "<<"\n";
	 cout<<"Total Number of Events\t:\t"<<eid<<"\n";
	 cout<<"Rate of events = \t:\t"<<eid<<" / min \n";
	         
   while(infinite or (event_counter>eid)) 
   {
	  eid++;
      b->StartDomino();							/* start board (activate domino wave) */
     // printf("\nWaiting for trigger...");		/* wait for trigger */
      fflush(stdout);
      while (b->IsBusy());

      b->TransferWaves(0, 8); /* read all waveforms */

      /* read time (X) array of first channel in ns */
      /* decode waveform (Y) array of first channel in mV */
      if( save_waveform) if(eid%skip_evts==0)
      {
			b->GetTime(0, 0, b->GetTriggerCell(0), time_array[0]);
			b->GetWave(0, 0, wave_array[0]);

			b->GetTime(0, 2, b->GetTriggerCell(0), time_array[1]);
			b->GetWave(0, 2, wave_array[1]);

			b->GetTime(0, 4, b->GetTriggerCell(0), time_array[2]);
			b->GetWave(0, 4, wave_array[2]);

			b->GetTime(0, 6, b->GetTriggerCell(0), time_array[3]);
			b->GetWave(0, 6, wave_array[3]);
      }
      else
      {
		b->GetTime(0, 2*channel, b->GetTriggerCell(0), time_array[channel]);
		b->GetWave(0, 2*channel, wave_array[channel]);
      }
      
	 //double get_energy(float waveform[8][1024],int channel, double trigger_level,double neg_offset,double integrate_window,double freq )
      energy=get_energy(wave_array,time_array, channel, -30,10,50,5.12);
	  file.open(energy_str.c_str(), ios::out | ios::app);
	  temp_str=to_string(eid)+","+to_string(energy)+"\n";
      file<<temp_str;
	  file.close();
	
	  if(save_waveform) if(eid%skip_evts==0)
		{
			save_event_binary(event_str.c_str(),muEvent,1);
	   	}
	   if(eid%updates_stats_interval==0)
	   {
	         cout<<"\033[F";
	         cout<<"\033[F";
	         cout<<"\033[F";
	         cout<<"\033[F"; // for moving back a line
	         diff=curr_t;
	         curr_t = time(0);
	         event_rate = (float(updates_stats_interval)*60/int(curr_t-diff));
 			 dt=ctime(&curr_t);
 			 fflush(stdout);
	         cout<<"\tCurrent time\t:\t"<<dt;
	         diff=curr_t-start_t;
	         elapsed_t = gmtime(&diff);
	         cout<<"\tElapsed time\t:\t"<<elapsed_t->tm_mday-1<<"days ";
	         cout<<elapsed_t->tm_hour<<"hrs ";
	         cout<<elapsed_t->tm_min<<"min ";
	         cout<<elapsed_t->tm_sec<<"sec "<<"\n";
	         cout<<"Total Number of Events\t:\t"<<eid<<endl;
	         cout<<"Rate of events = \t:\t"<<event_rate<<" / min \n";
	   }
      printf("\rEvent ID  %d \t\t|\tcharge : %f  pC", eid,energy);
   }
	cout<<"\n\n";
   delete drs;
}



double get_energy(float waveform[8][1024],float time[8][1024],int channel, double trigger_level,
												double neg_offset,double integrate_window,double freq )
{
	int start=0,end=1024;
	double integral=0;
	int i;
	for(i=0;i<1024;i++)
	{
		if(waveform[channel][i]<trigger_level)
			{
				start=i;
				break;
			}
	}
	while(i>0)
	{
		if(time[channel][start]-time[channel][i] > neg_offset)
			{
				start=i;
				break;
			}
		i--;
	}
	if (start<1) start=1;
	
	i=start;
	double dt=0,window=0;
	while(i<1024 and window<integrate_window)
	{
		dt=time[channel][i]-time[channel][i-1];
		window+=dt;
		integral+=waveform[channel][i]*dt;
		i++;
	}
//	printf("!! %d , %d  ,%d \n",start,i,int(TERMINAL_RESISTANCE));
	return -1*integral/TERMINAL_RESISTANCE;
}

