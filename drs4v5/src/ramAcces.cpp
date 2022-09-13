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

/*  Root Fit Functions */

Double_t totalfunc(Double_t* x, Double_t* par);
Double_t gausX(Double_t* x, Double_t* par);
Double_t langaufun(Double_t *x, Double_t *par);
/* _______________  */


static volatile bool break_loop = false;
static volatile bool DEBUG_MODE = false;

int system_return=0;
int read_spareRegisters(DRSBoard *b);

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
   b->Init();				/* initialize board */
   b->SetFrequency(5, true);		/* set sampling frequency */
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
   	

	read_spareRegisters(b);
	
	delete drs;
	
	return 0;
}


int read_spareRegisters(DRSBoard *b)
{
	unsigned int offSetToSpareAddr(0x4804);
	unsigned char  registryStore[100],c(0);
	int nBytes(8);
	int binary[8]; 
        
         
        b->TransferSpareRegisters(registryStore, T_RAM, nBytes, offSetToSpareAddr); // depricated fn . suggested to use b->Read() directly with proper OFFSET [ from T_XX ], and address and size 
	for(int i=0;i<nBytes;i++)
	{
		std::cout<<"i = "<<" | "<<registryStore[i] <<"  | ";
		
		for(int n = 0; n < 8; n++)
		{	
		    binary[7-n] = (registryStore[i] >> n) & 1;
		}
		c+=1;
		for(int n = 0; n < 8; n++)
			std::cout<<binary[n];
		std::cout<<"\n";

	}

	std::cout<<" Serial number from stdandard function : "<<b->GetBoardSerialNumber()<<"\n";
	offSetToSpareAddr=0x4804;
        b->Read(T_RAM, registryStore, offSetToSpareAddr, 2);
	int num=(static_cast < int >(registryStore[1]) << 8) + registryStore[0];
	std::cout<<" serial number : "<< num <<"\n";
	
        return 0;
}

