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
#define TERMINAL_RESISTANCE 50
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "strlcpy.h"
#include "DRS.h"

/*------------------------------------------------------------------*/

double get_energy(float waveform[8][1024],float time[8][1024],int channel,
						double trigger_level=-40.0,double neg_offset=20,double integrate_window=100, double freq =5.12);

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
//	while(i<1024)
//	{
//		if(time[channel][i]-time[channel][start] > integrate_window)
//			{
//				end=i;
//				break;
//			}
//		i++;
//	}
	
	printf("!! %d , %d  \n",start,end);
	//start-=int(neg_offset*freq);
	//end=start + int(integrate_window*freq);
	if (start<1) start=1;
	if (end>1024) end=1024;
	
	i=start;
	double dt=0,window=0;
	while(i<1024 and window<integrate_window)
	{
		dt=time[channel][i]-time[channel][i-1];
		window+=dt;
		integral+=waveform[channel][i]*dt;
		i++;
	}
	printf("!! %d , %d  ,%d \n",start,i,int(TERMINAL_RESISTANCE));
	return integral/TERMINAL_RESISTANCE;
}


int main()
{
   int i, j, nBoards;
   DRS *drs;
   DRSBoard *b;
   float time_array[8][1024];
   float wave_array[8][1024];
   FILE  *f;

   /* do initial scan */
   drs = new DRS();
   double energy=0;
   /* show any found board(s) */
   for (i=0 ; i<drs->GetNumberOfBoards() ; i++) {
      b = drs->GetBoard(i);
      printf("Found DRS4 evaluation board, serial #%d, firmware revision %d\n", 
         b->GetBoardSerialNumber(), b->GetFirmwareVersion());
   }

   /* exit if no board found */
   nBoards = drs->GetNumberOfBoards();
   if (nBoards == 0) {
      printf("No DRS4 evaluation board found\n");
      return 0;
   }

   /* continue working with first board only */
   b = drs->GetBoard(0);

   /* initialize board */
   b->Init();

   /* set sampling frequency */
   b->SetFrequency(5, true);

   /* enable transparent mode needed for analog trigger */
    b->SetTranspMode(1);

   /* set input range to -0.5V ... +0.5V */
   b->SetInputRange(0);

   /* use following line to set range to 0..1V */
   //b->SetInputRange(0.5);
   
   /* use following line to turn on the internal 100 MHz clock connected to all channels  */
   //b->EnableTcal(1);

   /* use following lines to enable hardware trigger on CH1 at 50 mV positive edge */
   if (b->GetBoardType() >= 8) {        // Evaluaiton Board V4&5
   
      b->EnableTrigger(1, 0);           // enable hardware trigger
     // b->SetTriggerSource(1<<0);        // set CH1 as source
      b->SetTriggerSource(0x0B00);        //AND Bit8=CH1, Bit9=CH2, Bit11=CH4 =>hex 0B00
   } 
   
   else 
   	{
    	printf(" Old version of board !! exiting ");
    	return 0;
   	}
  // b->SetTriggerLevel(0.05);            // 0.05 V
   b->SetTriggerPolarity(true);        // false :positive edge
   
   /* use following lines to set individual trigger elvels */
   float trigger_level=-0.04;
   b->SetIndividualTriggerLevel(0, trigger_level);
   b->SetIndividualTriggerLevel(2, trigger_level);
   b->SetIndividualTriggerLevel(3, trigger_level);
   b->SetTriggerSource(0xB00);
   
   b->SetTriggerDelayNs(50);             // zero ns trigger delay
   
   /* open file to save waveforms */
   f = fopen("data.txt", "w");
   fclose(f);
   if (f == NULL) {
      perror("ERROR: Cannot open file \"data.txt\"");
      return 1;
   }
   
   /* repeat ten times */
   unsigned long int eid=0;
   while(1) 
   {
		eid++;
      /* start board (activate domino wave) */
      b->StartDomino();

      /* wait for trigger */
      printf("\nWaiting for trigger...");
      
      fflush(stdout);
      while (b->IsBusy());

      /* read all waveforms */
      b->TransferWaves(0, 8);

      /* read time (X) array of first channel in ns */
      /* decode waveform (Y) array of first channel in mV */
      b->GetTime(0, 0, b->GetTriggerCell(0), time_array[0]);
      b->GetWave(0, 0, wave_array[0]);

      b->GetTime(0, 2, b->GetTriggerCell(0), time_array[1]);
      b->GetWave(0, 2, wave_array[1]);

      b->GetTime(0, 4, b->GetTriggerCell(0), time_array[2]);
      b->GetWave(0, 4, wave_array[2]);
      
      b->GetTime(0, 6, b->GetTriggerCell(0), time_array[3]);
      b->GetWave(0, 6, wave_array[3]);


      /* Save waveform: X=time_array[i], Yn=wave_array[n][i] */
	  f = fopen("data.txt", "a");
      fprintf(f, "#Event,%d \n", eid);
      if(eid>=0)
		{
		  fprintf(f, "#Waveform\n");
		  for (i=0 ; i<1024 ; i++)
		  {
		  		for (j=0 ; j<4 ; j++) 
		  			fprintf(f,"%f , %f, ", time_array[j][i],wave_array[j][i]);
		  	fprintf(f,"\n");
		  }
	   	}
	   //double get_energy(float waveform[8][1024],int channel, double trigger_level,double neg_offset,double integrate_window,double freq )
      energy=get_energy(wave_array,time_array, 2 , -30,10,50,5.12);
      printf("\tEvent #%d read successfully,  charge integral  : %f ", eid,energy, " pC\n");
      fprintf(f, "!energy , %f\n", energy);
      fclose(f);
   }

   //fclose(f);
   
   /* delete DRS object -> close USB connection */
   delete drs;
}
