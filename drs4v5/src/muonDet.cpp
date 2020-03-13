/********************************************************************\

  Name:         muonDet.cpp

  Contents:     Simple application to read out a DRS4 board
  				and integrate peaks to get the charge

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

/*cern root libs*/
#include<climits>
#include "TFile.h"
#include "TTree.h"
#include "TH1.h"
#include "TPad.h"
#include "TCanvas.h"
#include "TH1F.h"
#include "TGraphErrors.h"
#include "TF1.h"
#include "TH2.h"
#include "TProfile.h"

#include "TStyle.h"
#include "TAttFill.h"
#include "TPaveStats.h"
#include "TMinuit.h"
#include "TPostScript.h"
#include "TFitResult.h"
#include "TFitResultPtr.h"
#include "TRandom.h"
#include "TPaletteAxis.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TMath.h"

/* std cpp libs*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctime>
#include <pthread.h>
#include <chrono>
#include <thread>
#include <fstream>

/*  DRS4v5 libs*/
#include "strlcpy.h"
#include "DRS.h"
#include <DRS4v5_lib.h>

#define UPADATE_STATS_INTERVAL 20
/*------------------------------------------------------------------*/




static volatile bool break_loop = false;
static volatile bool DEBUG_MODE = false;

int system_return=0;

static void* exit_loop(void*)
{
   while(!break_loop) 
   {
       if (cin.get() == 'q')
       {
           //! desired user input 'q' received
           break_loop = true;
       }
   }
   pthread_exit(NULL);
}

static void* sleep_thread(void * seconds)
{
 	int * tsecs= (int * )seconds;
	std::this_thread::sleep_for(std::chrono::milliseconds(*tsecs*1000));
	break_loop=true;
	pthread_exit(NULL);
}

int adc_mode(DRSBoard *b);
int counter_mode(DRSBoard *b);

int main()
{
   int nBoards;
   DRS *drs;
   DRSBoard *b;
   drs = new DRS(); 			/* do initial scan */
   for (int i=0 ; i<drs->GetNumberOfBoards() ; i++) 
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
   } 
   else 
   	{
    	printf(" Old version of board found !! exiting ");
    	return 0;
   	}
   	int choice;
   	cout<<"\n Enter your choice : \n";
   	cout<<"\t 1 -> ADC Mode \n";
   	cout<<"\t 2 -> Counter Mode \n";
   	cout<<"\t 0 -> Exit \n\t";
   	cin>>choice;
   	
	     if(choice==3)	return 0;
   	else if(choice==1)	adc_mode(b);
	else if(choice==2)	counter_mode(b);
   delete drs;
	
	return 0;
}

int counter_mode(DRSBoard *b)
{
	system_return=system("clear");
	cout<<"\n COUNTER MODE \n";
	int choice=-1;
	unsigned int tsecs=100;
	double trig_level_ch[4]={-30.0,-30.0,-30.0,-30.0};
	unsigned long int scount=0,dcount=0;
	cout<<"\n Enter the Trigger logic : "<<endl;
	cout<<"1  -> "<<" Single Channel Trigger  [ ch 2 ]"<<endl;
	cout<<"2  -> "<<" Two Fold Coincidance Counter  [ ch 2 AND ch 3 ]"<<endl;
	cout<<"3  -> "<<" Three Fold Coincidance Counter  [ ch 2 AND ch 3  AND ch 4]"<<endl;
	cout<<"4  -> "<<" Four Fold Coincidance Counter  [ ch 1 AND ch 2 AND ch 3  AND ch 4]"<<endl;
	cout<<"0  -> "<<" Exit"<<endl;
	cin>>choice;
	if(choice<0) 
	{
		DEBUG_MODE=!DEBUG_MODE;
		choice*=-1;
	}
	unsigned int *tsleep=&tsecs;
	switch(choice)
	{
		// Set trigger configuration
      // OR  Bit0=CH1, Bit1=CH2,  Bit2=CH3,  Bit3=CH4,  Bit4=EXT
      // AND Bit8=CH1, Bit9=CH2, Bit10=CH3, Bit11=CH4, Bit12=EXT
      // TRANSP Bit15
      	case 0 : return 0;
		case 1 : 
				{
					cout<<"\n Enter the Trigger value for Channel 2 [in mV , with sign, falling edge ]  : ";
					cin>>trig_level_ch[1];
					b->SetIndividualTriggerLevel(2,trig_level_ch[1]/1000);
					b->SetTriggerSource(0x0200);
					if (DEBUG_MODE) b->SetTriggerSource(0x0010);
				}
				break;
		case 2 : 
				{
					cout<<"\n Enter the Trigger value for Channel 2 [in mV , with sign, falling edge ]  : ";
					cin>>trig_level_ch[1];
					cout<<"\n Enter the Trigger value for Channel 3 [in mV , with sign, falling edge ]  : ";
					cin>>trig_level_ch[2];
					b->SetIndividualTriggerLevel(2,trig_level_ch[1]/1000);
					b->SetIndividualTriggerLevel(3,trig_level_ch[2]/1000);
					b->SetTriggerSource(0x0E00);
					if (DEBUG_MODE) b->SetTriggerSource(0x0010);        //Ext triger

				}
				break;
		case 3 : 
				{
					cout<<"\n Enter the Trigger value for Channel 2 [in mV , with sign, falling edge ]  : ";
					cin>>trig_level_ch[1];
					cout<<"\n Enter the Trigger value for Channel 3 [in mV , with sign, falling edge ]  : ";
					cin>>trig_level_ch[2];
					cout<<"\n Enter the Trigger value for Channel 4 [in mV , with sign, falling edge ]  : ";
					cin>>trig_level_ch[3];
					b->SetIndividualTriggerLevel(2,trig_level_ch[1]/1000);
					b->SetIndividualTriggerLevel(3,trig_level_ch[2]/1000);
					b->SetIndividualTriggerLevel(4,trig_level_ch[3]/1000);
					b->SetTriggerSource(0x600);
					if (DEBUG_MODE) b->SetTriggerSource(0x0010);        //Ext triger
				}
				break;
		case 4 : 
				{
					cout<<"\n Enter the Trigger value for Channel 1 [in mV , with sign, falling edge ]  : ";
					cin>>trig_level_ch[0];
					cout<<"\n Enter the Trigger value for Channel 2 [in mV , with sign, falling edge ]  : ";
					cin>>trig_level_ch[1];
					cout<<"\n Enter the Trigger value for Channel 3 [in mV , with sign, falling edge ]  : ";
					cin>>trig_level_ch[2];
					cout<<"\n Enter the Trigger value for Channel 4 [in mV , with sign, falling edge ]  : ";
					cin>>trig_level_ch[3];
					b->SetIndividualTriggerLevel(1,trig_level_ch[0]/1000);
					b->SetIndividualTriggerLevel(2,trig_level_ch[1]/1000);
					b->SetIndividualTriggerLevel(3,trig_level_ch[2]/1000);
					b->SetIndividualTriggerLevel(4,trig_level_ch[3]/1000);
					b->SetTriggerSource(0x0F00);
					if (DEBUG_MODE)  b->SetTriggerSource(0x0010);        //Ext triger
					
				}
				break;
		default :
				{
					cout<<"\n enter a valid choise !! \n";
					return 1;
				}
		
	}
	
	b->SetTriggerPolarity(true);        // false :positive edge
	cout<<"\n Enter the counter duration in seconds\t: ";	cin>>tsecs;

	//For exiting after n secs
	 pthread_t tId;
	 (void) pthread_create(&tId, 0, sleep_thread, (void *)tsleep);
	 //For exiting on 'q' 
	 pthread_t tId2;
	 (void) pthread_create(&tId2, 0, exit_loop, 0);
    
     cout<<"\n The counter has started \n\n";
	 while(!break_loop)
	 {
		b->StartDomino();
		while (b->IsBusy());
		scount++;
		printf("\033[F \r Count = %lu \n ",scount);
	 }
	cout<<"\n\n";
	return 0;

}
int adc_mode(DRSBoard *b)
{
    float time_array[4][1024];
    float wave_array[4][1024];
    float trigger_level=-0.04;

   vector<double*> calib_data;
   int calib_channel[4]={0,1,2,3};
   int calib_channel_id=0;

   fstream file;
   string run_name="atsrun",energy_str,event_str,temp_str;
   unsigned long int event_counter=500;
   int channel=3,skip_evts=2;
   bool infinite=false;
   bool save_waveform=false;
   int updates_stats_interval=UPADATE_STATS_INTERVAL;
   int event_rate;
   
   time_t start_t = time(0);
   time_t curr_t,diff,etime;
   tm* elapsed_t ;
   char* dt=ctime(&start_t);
   
   system_return=system("clear");
   cout<<"\n\t\t\t ADC MODE \n";
	cout<<" CONFIGURATION  : trigger : ch1 AND ch2 AND ch4 , TUT : ch3";
   cout<<"\n\n\n\t\tCurrent time  : "<<dt;

	save_waveform=true;
   cout<<"Enter the data run name \t:\t";   cin>>run_name;
    cout<<"Enter the trigger level in mV  ( with sign )\t:\t";
			cin>>trigger_level;
								if(trigger_level==9999) // For debug mode
								{
									DEBUG_MODE=!DEBUG_MODE;
									trigger_level=0.0;
								}
			trigger_level/=1000;
	 
   cout<<"Enter number of events to be recorded ( -1 for infinite loop ) : ";
   			cin>>event_rate;
  	if(event_rate <=-1) 
  		infinite=true;
  	else
  		event_counter=event_rate;
  	event_rate=0;
  	
   channel=3;	
   cout<<"Enter channenl of interest [1,2,3 or 4] : "<<channel<<"\n";
   			//cin>>channel;
   	if (channel<1 or channel>4)
   		{  TH1F* histadc = new TH1F("histadc", "histadc", 512, -0.125, 511.125);
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
	system_return=system(event_str.c_str());
	event_str="chmod -R 777 data/"+run_name;
	system_return=system(event_str.c_str());
	
	cout<<"\n Do you want to enter any remarks [y/n] ?\t:\t";
		cin>>remark_option;
	if(remark_option=='y' or remark_option=='Y')
		{
			temp_str="nano data/"+run_name+"/remarks.txt";
			system_return=system(temp_str.c_str());
		}
	else
	{ 
		temp_str="touch data/"+run_name+"/remarks.txt";
		system_return=system(temp_str.c_str());
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
	system_return=system("clear");
	cout<<"\n\t\t\t ADC MODE \n";
   	start_t = time(0);
	dt=ctime(&start_t);
    cout<<"\nEvent Monitoring Stared at \t:\t   "<<dt;
    cout<<"\nenergy integrals written to  \t:\t"<<energy_str;
    if(save_waveform)
    	{
	    	cout<<"\nwaveforms are written to\t:\t"<<event_str;
 		cout<<" [ One out of every "<<skip_evts<<" saved]";
    	}
    if(infinite)
    	cout<<"\nOn cotinious run .. input 'q' then 'enter' to quit\n";
 	else 
 		cout<<"\nNumber of events to be monitored :\t"<<event_counter;
 	cout<<"\nChannel to be integrated  : "<<channel+1<<"\n\n";
 	cout<<"\nRemarks \t: \n\n";
 	temp_str="cat data/"+run_name+"/remarks.txt";
 	system_return=system(temp_str.c_str());
	cout<<"\n\n\n";
	
    b->SetTriggerPolarity(true) ;        // true :negative edge
//  b->SetTriggerPolarity(false);        // false :positive edge
   
   double energy=0;	

   b->SetIndividualTriggerLevel(0, trigger_level);
   b->SetIndividualTriggerLevel(1, trigger_level);
   b->SetIndividualTriggerLevel(3, trigger_level);
 
  // OR  Bit0=CH1, Bit1=CH2,  Bit2=CH3,  Bit3=CH4,  Bit4=EXT
  // AND Bit8=CH1, Bit9=CH2, Bit10=CH3, Bit11=CH4, Bit12=EXT
  
   b->SetTriggerSource(0xB00);  //For internal 3 fold coincidance
   
   if (DEBUG_MODE)  
   {
   	cout<<"\n in DEBUG_MODE .. setting ext trigg source\n";
	   b->SetTriggerSource(0x0010);      //For external OR Trigger
   }
   b->SetTriggerDelayNs(50);             // 50 ns trigger delay
   
   
	   unsigned long int eid=0;
	 curr_t = time(0);
     diff=curr_t;
	 event_rate = int(float(updates_stats_interval)/int(diff-curr_t+1));
	 dt=ctime(&curr_t);
	 fflush(stdout);
	get_channel_offsets("calib/offset_calib.dat",&calib_data,calib_channel);
    cout<<"offset caliberation read for "<<calib_data.size()<<"  channels ";
	vector<double*>::iterator ditr=calib_data.begin();
	for(int k=0;ditr!=calib_data.end();ditr++)
		{	
			cout<<calib_channel[k++]<<"  ";
			for (int j=0;j<1024;j++)
				(*ditr)[j]*=1000;
		}
	 cout<<"\n\n";
	 cout<<"\tCurrent time\t:\t"<<dt;
	 diff=curr_t-start_t;
	 elapsed_t = gmtime(&diff);
	 cout<<"\tElapsed time\t:\t"<<elapsed_t->tm_mday-1<<"days ";
	 cout<<elapsed_t->tm_hour<<"hrs ";
	 cout<<elapsed_t->tm_min<<"min ";
	 cout<<elapsed_t->tm_sec<<"sec "<<"\n";
	 cout<<"Total Number of Events\t:\t"<<eid<<"\n";
	 cout<<"Rate of events = \t:\t"<<eid<<" / min \n";
	
		//For exiting on 'q' 
		 pthread_t tId;
		 (void) pthread_create(&tId, 0, exit_loop, 0);
   
   DRS_EVENT muEvent[1];
   
   for(int i=0;i<4;i++)
   {
	   muEvent[0].time.push_back(time_array[i]);
	   muEvent[0].waveform.push_back(wave_array[i]);
   }
	
	for(int i=0;i<4;i++)
		if(calib_channel[i]==channel)
			calib_channel_id=i;
	
	strcpy(muEvent[0].eheader.event_header,"muT");
	muEvent[0].eheader.millisecond=0;
	muEvent[0].eheader.range=0;
	int save_to_disc_count=0;
	
	// Histograms for online plotting
	TH1D* qADC = new TH1D("qADC", "Signal Integral", 257, -1, 256); 
	TCanvas* c1 = new TCanvas("c1", "c1", 800, 400);
	
	// EVENT LOOP
	
   while( (infinite or (event_counter>eid)) and !break_loop) 
   {
	  eid++;
      b->StartDomino();							/* start board (activate domino wave) */
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
			
			etime = time(0);
			elapsed_t = localtime(&etime);

			muEvent[0].eheader.event_serial_number=eid;
			muEvent[0].eheader.year=elapsed_t->tm_year;
			muEvent[0].eheader.month=elapsed_t->tm_mon;
			muEvent[0].eheader.day=elapsed_t->tm_mday;
			muEvent[0].eheader.hour=elapsed_t->tm_hour;
			muEvent[0].eheader.minute=elapsed_t->tm_min;
			muEvent[0].eheader.second=elapsed_t->tm_sec;
			
			for(int i=0;i<4;i++)
			for(int j=0;j<1024;j++)
			{
				wave_array[calib_channel[i]][j]-=calib_data[calib_channel[i]][j];
			}
			save_event_binary(event_str.c_str(),muEvent,1);
			save_to_disc_count++;
      }
      else
      {
		b->GetTime(0, 2*channel, b->GetTriggerCell(0), time_array[channel]);
		b->GetWave(0, 2*channel, wave_array[channel]);
		for(int i=0;i<1024;i++)
			{
				wave_array[channel][i]-=calib_data[calib_channel_id][i];
			}
      }
      
	 //double get_energy(float waveform[8][102TCanvas* c1 = new TCanvas("c1", "c1", 800, 400);4],int channel, double trigger_level,double neg_offset,double integrate_window,double freq )
      
      energy=get_energy(wave_array,time_array, channel, -40,10,50,5.12);
      qADC->Fill(energy);
      
	  file.open(energy_str.c_str(), ios::out | ios::app);
	  temp_str=to_string(eid)+","+to_string(energy)+"\n";
      file<<temp_str;
	  file.close();
	
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
	         cout<<"\tElapsed time\t:\t"<<elapsed_t->tm_yday<<"days ";
	         cout<<elapsed_t->tm_hour<<"hrs ";
	         cout<<elapsed_t->tm_min<<"min ";
	         cout<<elapsed_t->tm_sec<<"sec "<<"\n";
	         cout<<"Total Number of Events\t:\t"<<eid<<endl;
	         cout<<"Rate of events = \t:\t"<<event_rate<<" / min \n";
	         qADC->Draw();
	   }
	  printf("\r\t\t\t\t\t\t\t\t\t!!");
      printf("\rEvent ID  %lu \t\t|\tcharge : %f  pC", eid,energy);
   }
   
   break_loop=true;
   (void) pthread_join(tId, NULL);
   	temp_str="data/"+run_name+"/remarks.txt";
   	file.open(temp_str.c_str(),ios::app|ios::out);
   	file<<"\n-------------------------------------------------\n";
   	dt=ctime(&start_t);
   	file<<"Run started at : "<<dt;
   	curr_t=time(0);
   	dt=ctime(&curr_t);
   	file<<"Run stopped at : "<<dt;
   	file<<"Triggers for channels :"<<trigger_level<<endl;
   	file<<"Channel integrated:"<<channel+1<<endl;
   	file<<"Number of events to be recorded [-1-> continious mode]: "<<event_counter<<endl;
   	file<<"Number of events recorded : "<<eid<<endl;
   	file<<"Number of events skipped at a stretch : "<<skip_evts-1<<endl;
   	file<<"Number of events saved to disc : "<<save_to_disc_count<<endl;
   	file<<"\n-------------------------------------------------\n";
   	
   	event_str="chmod -R 777 data/"+run_name;
	system_return=system(event_str.c_str());
	cout<<"\n\n";
	return 0;
}
