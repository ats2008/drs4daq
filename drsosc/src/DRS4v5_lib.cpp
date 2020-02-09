/*
   Name:           read_binary.cpp
   Created by:     Stefan Ritt <stefan.ritt@psi.ch>
   Date:           July 30th, 2014

   Purpose:        Example file to read binary data saved by DRSOsc.
 
   Compile and run it with:
 
      gcc -o read_binary read_binary.cpp
 
      ./read_binary <filename>

   This program assumes that a pulse from a signal generator is split
   and fed into channels #1 and #2. It then calculates the time difference
   between these two pulses to show the performance of the DRS board
   for time measurements.

   $Id: read_binary.cpp 22321 2016-08-25 12:26:12Z ritt $
*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <drsoscBinary.h>


int get_events( const char * fname="",double * waveformOUT=NULL,int start_eventID=0,int end_evetID=-1) asm ("get_events");


int get_events(const char * fname,double * waveformOUT,int start_eventID,int end_evetID)
{
   FHEADER  fh;
   THEADER  th;
   BHEADER  bh;
   EHEADER  eh;
   TCHEADER tch;
   CHEADER  ch;
   waveformOUT[0]=10;
   waveformOUT[1]=start_eventID;
   waveformOUT[2]=end_evetID;
   unsigned int scaler;
   unsigned short voltage[1024];
   double waveform[16][4][1024], time[16][4][1024];
   float bin_width[16][4][1024];
   int i, j, b, chn, n, chn_index, n_boards;
   double t1, t2, dt;
   char filename[256];

   strcpy(filename, fname);
   // open the binary waveform file
    FILE *f = fopen(filename, "rb");
   if (f == NULL) {
      fprintf(stderr,"Cannot find file \'%s\'\n", filename);
      return 1 ;
   }
   // read file header
   fread(&fh, sizeof(fh), 1, f);
   if (fh.tag[0] != 'D' || fh.tag[1] != 'R' || fh.tag[2] != 'S') {
      fprintf(stderr,"Found invalid file header in file \'%s\', aborting.\n", filename);
      return 2 ;
   }
   if (fh.version != '2') {
      fprintf(stderr,"Found invalid file version \'%c\' in file \'%s\', should be \'2\', aborting.\n", fh.version, filename);
      return 3 ;
   }

   // read time header
   fread(&th, sizeof(th), 1, f);
   if (memcmp(th.time_header, "TIME", 4) != 0) 
   {
      fprintf(stderr,"Invalid time header in file '%s\', aborting.\n", filename);
      return 4 ;
   }
   for (b = 0 ; ; b++) 
   {
      // read board header
      fread(&bh, sizeof(bh), 1, f);
      if (memcmp(bh.bn, "B#", 2) != 0) 
      {
         // probably event header found
         fseek(f, -4, SEEK_CUR);
         break;
      }
      // read time bin widths
      memset(bin_width[b], sizeof(bin_width[0]), 0);
      for (chn=0 ; chn<4 ; chn++) 
      {
         fread(&ch, sizeof(ch), 1, f);
         if (ch.c[0] != 'C') 
         {
            // event header found
            fseek(f, -4, SEEK_CUR);
            break;
         }
         i = ch.cn[2] - '0' - 1;
         fread(&bin_width[b][i][0], sizeof(float), 1024, f);
         // fix for 2048 bin mode: double channel
         if (bin_width[b][i][1023] > 10 || bin_width[b][i][1023] < 0.01) 
         {
            for (j=0 ; j<512 ; j++)
               bin_width[b][i][j+512] = bin_width[b][i][j];
         }
      }
   }
   n_boards = b; 
   // If the datafile contains info from more than one board stop processing .. needs to be updated to board_selct option
   // if this condition is removed EVENT_SIZE_BYTES (drsoscBinary.h) should also be updated
   if (n_boards>1)
   		return 5 ;
   
   // loop over all events in the data file
   
  // double *waveformOUT =NULL;
  // waveformOUT = new double [(end_evetID-start_eventID+1)*4*1024*2];
   int waveWrite_pos=0;   
   waveformOUT[5]=-1;
	
	int start_offset=start_eventID*EVENT_SIZE_BYTES;
	int seekStatus=fseek(f,start_offset,SEEK_CUR);
	if (seekStatus)
	{
		fprintf(stderr,"Invalid start event ID (offset = %d )",start_offset);
		return 6;
	}
	
   for (n=start_eventID ; ; n++) 
   {
      // read event header
      i = (int)fread(&eh, sizeof(eh), 1, f);
      if (i < 1)
         break;
      
      // loop over all boards in data file
      for (b=0 ; b<n_boards ; b++) 
      {
         // read board header
         fread(&bh, sizeof(bh), 1, f);
         if (memcmp(bh.bn, "B#", 2) != 0) 
         {
            fprintf(stderr,"Invalid board header in file \'%s\', aborting.\n", filename);
            return 7 ;
         }
         
         // read trigger cell
         fread(&tch, sizeof(tch), 1, f);
         if (memcmp(tch.tc, "T#", 2) != 0) 
         {
            fprintf(stderr,"Invalid trigger cell header in file \'%s\', aborting.\n", filename);
            return 8 ;
         }
         // read channel data
         for (chn=0 ; chn<4 ; chn++) 
         {
            // read channel header
            fread(&ch, sizeof(ch), 1, f);
            if (ch.c[0] != 'C') 
            {
               // event header found
               fseek(f, -4, SEEK_CUR);
               break;
            }
            chn_index = ch.cn[2] - '0' - 1;
            fread(&scaler, sizeof(int), 1, f);
            fread(voltage, sizeof(short), 1024, f);
            
            for (i=0 ; i<1024 ; i++) 
            {
               								 
   			// to convert data to volts  : (voltage[i] / 65536. + eh.range/1000.0 - 0.5);
               waveform[b][chn_index][i] = (voltage[i] / 65536. + eh.range/1000.0 - 0.5);
               
           // calculate time for this cell
               for (j=0,time[b][chn_index][i]=0 ; j<i ; j++)
                  time[b][chn_index][i] += bin_width[b][chn_index][(j+tch.trigger_cell) % 1024];
            }
         }
         
         
         
         // align cell #0 of all channels
         t1 = time[b][0][(1024-tch.trigger_cell) % 1024];
         for (chn=0 ; chn<4 ; chn++) 
         {
            t2 = time[b][chn][(1024-tch.trigger_cell) % 1024];
            dt = t1 - t2;
            for (i=0 ; i<1024 ; i++)
            {
               time[b][chn][i] += dt;
               waveformOUT[waveWrite_pos++]=time[b][chn][i];
			   waveformOUT[waveWrite_pos++]=waveform[b][chn][i];
            }
           
         }
        
      }
     if(n>=end_evetID) 
     {
//		fprintf(stdout,"ending at n = %d", n);
		break;
     }
     
   }
   return 0 ;
}

/*
int main(int argc, const char * argv[])
{

   char filename[256];
   if (argc > 1)
      strcpy(filename, argv[1]);
   else {
      fprintf( stdout,"Usage: read_binary <filename>\n");
      return 5 1;
   } 
   fprintf( stdout,"read_binary <filename> %s\n",filename);
    double *wdata =NULL;
    int start=8000;
    int end=8000;
   wdata = new double [(end-start+1)*4*1024*2];
   get_events(filename,wdata,start,end);
   for (int i=0;i<10;i++)
   	fprintf( stdout,"%f,%f \n",wdata[2*i],wdata[2*i+1]);
  
  return 5 0;
}

*/

