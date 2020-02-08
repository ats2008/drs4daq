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

#define EVENT_SIZE_BYTES 2088 

typedef struct {
   char           tag[3];
   char           version;
} FHEADER;

typedef struct {
   char           time_header[4];
} THEADER;

typedef struct {
   char           bn[2];
   unsigned short board_serial_number;
} BHEADER;

typedef struct {
   char           event_header[4];
   unsigned int   event_serial_number;
   unsigned short year;
   unsigned short month;
   unsigned short day;
   unsigned short hour;
   unsigned short minute;
   unsigned short second;
   unsigned short millisecond;
   unsigned short range;
} EHEADER;

typedef struct {
   char           tc[2];
   unsigned short trigger_cell;
} TCHEADER;

typedef struct {
   char           c[1];
   char           cn[3];
} CHEADER;

/*-----------------------------------------------------------------------------*/


void get_events( const char * fname="",double * waveformOUT=NULL,int start_eventID=0,int end_evetID=-1) asm ("get_events");

void get_events(const char * fname,double * waveformOUT,int start_eventID,int end_evetID)
{
	int jenga=0;
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
   FILE *fout = fopen("log", "w");

   // open the binary waveform file
    FILE *f = fopen(filename, "rb");
   if (f == NULL) {
      fprintf(  fout,"Cannot find file \'%s\'\n", filename);
      fclose(fout);
      waveformOUT[3]=-1;
      return ;
   }
   // read file header
   fread(&fh, sizeof(fh), 1, f);
   fprintf(  fout,"fh Tag : %s \n",fh.tag);
   if (fh.tag[0] != 'D' || fh.tag[1] != 'R' || fh.tag[2] != 'S') {
      fprintf(  fout,"Found invalid file header in file \'%s\', aborting.\n", filename);
      return ;
   }
   if (fh.version != '2') {qqqQqqqqsstt
      fprintf(  fout,"Found invalid file version \'%c\' in file \'%s\', should be \'2\', aborting.\n", fh.version, filename);
      return ;
   }

   // read time header
   fread(&th, sizeof(th), 1, f);
    fprintf(  fout,"time header : %s \n",th.time_header);
   if (memcmp(th.time_header, "TIME", 4) != 0) 
   {
      fprintf(  fout,"Invalid time header in file '%s\', aborting.\n", filename);
      return ;
   }
   for (b = 0 ; ; b++) 
   {
      // read board header
      fread(&bh, sizeof(bh), 1, f);
    fprintf(  fout,"board header :  %s \n",bh.bn);
      if (memcmp(bh.bn, "B#", 2) != 0) 
      {
         // probably event header found
         fseek(f, -4, SEEK_CUR);
         break;
      }
      fprintf(  fout,"Found data for board #%d\n", bh.board_serial_number);
      // read time bin widths
      memset(bin_width[b], sizeof(bin_width[0]), 0);
      for (chn=0 ; chn<4 ; chn++) 
      {
         fread(&ch, sizeof(ch), 1, f);
         fprintf(  fout,ch.c,ch.cn,'\n');
         if (ch.c[0] != 'C') 
         {
            // event header found
            fseek(f, -4, SEEK_CUR);
            break;
         }
         i = ch.cn[2] - '0' - 1;
         fprintf(  fout,"Found timing calibration for channel #%d\n", i+1);
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
   // if this condition is removed EVENT_SIZE_BYTES should also be updated
   if (n_boards>1)
   		return ;
   
   // loop over all events in the data file
   
  // double *waveformOUT =NULL;
  // waveformOUT = new double [(end_evetID-start_eventID+1)*4*1024*2];
   int waveWrite_pos=0;   
   waveformOUT[5]=-1;
	
	int start_offset=start_eventID*EVENT_SIZE_BYTES;
	int seekStatus=fseek(f,start_offset,SEEK_CUR);
	
   for (n=start_eventID ; ; n++) 
   {
      // read event header
      i = (int)fread(&eh, sizeof(eh), 1, f);
      if (i < 1)
         break;
      
       //fprintf(  fout,"Found event #%d %d %d\t \n", eh.event_serial_number, eh.second, eh.millisecond);
      // loop over all boards in data file
      for (b=0 ; b<n_boards ; b++) 
      {
         // read board header
         fread(&bh, sizeof(bh), 1, f);
         if (memcmp(bh.bn, "B#", 2) != 0) 
         {
            fprintf(  fout,"Invalid board header in file \'%s\', aborting.\n", filename);
            return ;
         }
         
         // read trigger cell
         fread(&tch, sizeof(tch), 1, f);
         if (memcmp(tch.tc, "T#", 2) != 0) 
         {
            fprintf(  fout,"Invalid trigger cell header in file \'%s\', aborting.\n", filename);
            return ;
         }

         if (n_boards > 1)
            fprintf(  fout,"Found data for board #%d\n", bh.board_serial_number);
         
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
         t1 = time[b][1][(1024-tch.trigger_cell) % 1024];
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
		fprintf(fout,"ending at n = %d", n);
     	fclose(fout);
     	return;
     }
     
   }
   fclose(fout);
   return ;
}

/*
int main(int argc, const char * argv[])
{

   char filename[256];
   if (argc > 1)
      strcpy(filename, argv[1]);
   else {
      fprintf( stdout,"Usage: read_binary <filename>\n");
      return 1;
   } 
   fprintf( stdout,"read_binary <filename> %s\n",filename);
    double *wdata =NULL;
    int start=8000;
    int end=8000;
   wdata = new double [(end-start+1)*4*1024*2];
   get_events(filename,wdata,start,end);
   for (int i=0;i<10;i++)
   	fprintf( stdout,"%f,%f \n",wdata[2*i],wdata[2*i+1]);
  
  return 0;
}

*/

