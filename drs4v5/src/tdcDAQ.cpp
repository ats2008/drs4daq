/********************************************************************\

  Name:         muonDet.cpp

  Contents:     Simple application to read out a DRS4 board
  				and integrate peaks to get the charge

\********************************************************************/

#include <bitset>
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

/*  Root Fit Functions */

Double_t totalfunc(Double_t* x, Double_t* par);
Double_t gausX(Double_t* x, Double_t* par);
Double_t langaufun(Double_t *x, Double_t *par);
/* _______________  */


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
int read_spareRegisters(DRSBoard *b, unsigned int offSetToSpareAddr);

int main()
{

   cout.setf(ios::fixed);
   cout.setf(ios::showpoint);
   cout.precision();

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
   b->SetInputRange(0.0);			/* set input range to -0.5V ... +0.5V */

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
   	cout<<"\t 1 -> TDC Mode \n";
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
   string run_name="defaultRun",energy_str,event_str,temp_str;
   unsigned long int event_counter=500;
   int channel=3,skip_evts=2;
   bool infinite=false;
   bool save_waveform=false;
   int updates_stats_interval=UPADATE_STATS_INTERVAL;
   int updates_Fit_interval=UPADATE_STATS_INTERVAL*50;
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
	cout<<"\n\n";
	
//   b->SetTriggerPolarity(true) ;        // true :negative edge
  b->SetTriggerPolarity(true);        // false :positive edge
   
   double energy=0;	

   b->SetIndividualTriggerLevel(0, trigger_level);
   b->SetIndividualTriggerLevel(1, trigger_level);
   b->SetIndividualTriggerLevel(3, trigger_level);
   // Setting similat trig level for the channel 3 comparator
   b->SetIndividualTriggerLevel(2, trigger_level);
 
  // OR  Bit0=CH1, Bit1=CH2,  Bit2=CH3,  Bit3=CH4,  Bit4=EXT
  // AND Bit8=CH1, Bit9=CH2, Bit10=CH3, Bit11=CH4, Bit12=EXT
  
 //  b->SetTriggerSource(0xF);    //For OR logic on channel 1,2,3,4
//   b->SetTriggerSource(0xB00);  //For internal 3 fold coincidance
   b->SetTriggerSource(0x0800);  //For channel 4 coincidance
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
	
	//Fitting function for the histogram
     TF1* fity = new TF1("fitey", langaufun, 5, 258, 4);
    //    TF1* fity = new TF1("fitey", totalfunc, 0.0,258, 7);
    
    
    //parameters : {Gausian_Mean, Gaus_Sigma, Gaus_Norm, Landau_Width, Landau_MPV, Landau_Norm, Landau_Gausian_Width};
	double  pary[7];
    pary[0]=0.5; // Gausian_Mean
    pary[1]=3.0; // Gaus_Sigma
    pary[2]=5.0; // gausian Norm
    pary[3]=20.0;// Landau_Width
    pary[4]=38.0;// Landau_MPV
    pary[5]=20.0;// Landau_Norm
    pary[6]=8;// Landau_Gausian_Width
    fity->SetParameters(pary);
    TFitResultPtr FitRsltPtr(0);
	// Histograms for online plotting 
	TH1D* qADC = new TH1D("qADC", "Signal Integral", 257, -1, 256); 
	TCanvas* c1 = new TCanvas("c1", "c1", 800, 400);
	c1->cd();

    gStyle->SetOptStat(11);
    gStyle->SetOptFit(1111);

    temp_str="data/"+run_name+"/qDep.png";
    //FitRsltPtr = qADC->Fit(fity, "QBMS");
    cout<<endl;
    c1->SaveAs(temp_str.c_str());
    c1->SaveAs("Monitor.png");
	temp_str="data/"+run_name+"/event.root";
	TFile* afile= new TFile(temp_str.c_str(),"recreate");
    TTree* edepTree= new TTree("muEvents","muEvents");
    edepTree->Branch("QDep",&energy);
    temp_str="xdg-open Monitor.png &";
 	system_return=system(temp_str.c_str());
	// EVENT LOOP
   int num=0	;
   while( (infinite or (event_counter>eid)) and !break_loop) 
   {
      eid++;
      b->StartDomino();							/* start board (activate domino wave) */
      fflush(stdout);
      while (b->IsBusy());
      
      std::cout<<std::endl<<"\n";
      std::cout<<" ========================================================================== ";	
      std::cout<<" Last bit of 0x23 will have Valid signal \n";
      read_spareRegisters(b,0x24);
      std::cout<<" TDC outputs  : 0x26  \n";
      num=read_spareRegisters(b,0x26);

      file.open(energy_str.c_str(), ios::out | ios::app);
      temp_str=to_string(eid)+","+to_string(num)+"\n";
      file<<temp_str;
      file.close();
	

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
   	file.close();
   	
    if(FitRsltPtr>0)
    {
       	temp_str="data/"+run_name+"/fit_params.txt";
       	file.open(temp_str.c_str(),ios::app|ios::out);
       	file<<"Pedestal_Gausian_Mean,"<<FitRsltPtr->Parameter(0)<<"\n";
       	file<<"Pedestal_Gausian_StD,"<<FitRsltPtr->Parameter(1)<<"\n";
       	file<<"Pedestal_Gausian_Normalization,"<<FitRsltPtr->Parameter(2)<<"\n";
       	file<<"Landau_Width,"<<FitRsltPtr->Parameter(3)<<"\n";
       	file<<"Landau_MPV,"<<FitRsltPtr->Parameter(4)<<"\n";
       	file<<"Landau_Norm,"<<FitRsltPtr->Parameter(5)<<"\n";
       	file<<"Landau_Gausian_Width,"<<FitRsltPtr->Parameter(6)<<"\n";
       	file.close();
    }
   	// Histograms for online plotting
    qADC->Write();
	delete qADC ;
	delete c1 ;
	edepTree->Write();
    delete edepTree;
	afile->Close();
	delete afile;
	delete fity ;
   	event_str="chmod -R 777 data/"+run_name;
	system_return=system(event_str.c_str());
	cout<<"\n\n";
	return 0;
}
 
Double_t langaufun(Double_t *x, Double_t *par) 
{

  //Fit parameters:
  //par[0]=Width (scale) parameter of Landau density
  //par[1]=Most Probable (MP, location) parameter of Landau density
  //par[2]=Total area (integral -inf to inf, normalization constant)
  //par[3]=Width (sigma) of convoluted Gaussian function
  //
  //In the Landau distribution (represented by the CERNLIB approximation), 
  //the maximum is located at x=-0.22278298 with the location parameter=0.
  //This shift is corrected within this function, so that the actual
  //maximum is identical to the MP parameter.
  
  // Numeric constants
  Double_t invsq2pi = 0.3989422804014;   // (2 pi)^(-1/2)
  Double_t mpshift  = -0.22278298;       // Landau maximum location
  
  // Control constants
  Double_t np = 100.0;      // number of convolution steps
  Double_t sc =   5.0;      // convolution extends to +-sc Gaussian sigmas
  
  // Variables
  Double_t xx;
  Double_t mpc;
  Double_t fland;
  Double_t sum = 0.0;
  Double_t xlow,xupp;
  Double_t step;
  Double_t i;
  
  
  // MP shift correction
  mpc = par[1] - mpshift * par[0]; 
  
  // Range of convolution integral
  xlow = x[0] - sc * par[3];
  xupp = x[0] + sc * par[3];
  
  step = (xupp-xlow) / np;
  
  // Convolution integral of Landau and Gaussian by sum
  for(i=1.0; i<=np/2; i++) {
    xx = xlow + (i-.5) * step;
    fland = TMath::Landau(xx,mpc,par[0]) / par[0];
    sum += fland * TMath::Gaus(x[0],xx,par[3]);
    
    xx = xupp - (i-.5) * step;
    fland = TMath::Landau(xx,mpc,par[0]) / par[0];
    sum += fland * TMath::Gaus(x[0],xx,par[3]);
  }
  
  return (par[2] * step * sum * invsq2pi / par[3]);
}

Double_t gausX(Double_t* x, Double_t* par){
  return par[0]*(TMath::Gaus(x[0], par[1], par[2], kTRUE));
}
Double_t totalfunc(Double_t* x, Double_t* par){
  return gausX(x, par) + langaufun(x, &par[3]);
}



int read_spareRegisters(DRSBoard *b, unsigned int offSetToSpareAddr)
{
//	unsigned int offSetToSpareAddr(0x4804);
	unsigned char  registryStore[100],c(0);
	int nBytes(8);
	int binary[8]; 
        
         
//      b->TransferSpareRegisters(registryStore, T_RAM, nBytes, offSetToSpareAddr); // depricated fn . suggested to use b->Read() directly with proper OFFSET [ from T_XX ], and address and size 
//	for(int i=0;i<nBytes;i++)
//	{
//		std::cout<<"i = "<<" | "<<registryStore[i] <<"  | ";
//		
//		for(int n = 0; n < 8; n++)
//		{	
//		    binary[7-n] = (registryStore[i] >> n) & 1;
//		}
//		c+=1;
//		for(int n = 0; n < 8; n++)
//			std::cout<<binary[n];
//		std::cout<<"\n";
//
//	}

	std::cout<<" Serial number from stdandard function : "<<b->GetBoardSerialNumber()<<"\n";
        b->Read(T_STATUS, registryStore, 0x24, 2);
	int num=(static_cast < int >(registryStore[1]) << 8) + registryStore[0];
	std::cout<<" serial number : "<< num <<"\n";
	
	b->Read(REG_CONFIG, registryStore, offSetToSpareAddr, 2);
	num=(static_cast < int >(registryStore[1]) << 8) + registryStore[0];
	std::cout<<" register Address : "<< offSetToSpareAddr<<"\n";
	std::cout<<" register value : "<< std::bitset<16> (num)<<" |  descimal --> "<<num<<"\n";
	
        return num;
}

