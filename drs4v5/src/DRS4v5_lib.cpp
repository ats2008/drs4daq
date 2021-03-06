#include <DRS4v5_lib.h>

int do_offset_caliberation(string ofile,string configfile)
{
	fstream ifile;
	string l="";
	vector<string> ocalib_files;

	ifile.open(configfile.c_str(),ios::in | ios::binary);
	if(!ifile.is_open())
	{
		fprintf(stderr,"CONFIG FILE DOES NOT EXIST !!");
		return  1 ;
	}
	while (!ifile.eof())
		{	
			getline(ifile,l,'\n') ;
			if(l[0]=='#')
				continue;
			if(l[0]=='-' && l[1]=='c' && l[2]=='o')
			{
				for(int i=0;i<NUMBER_OF_CHANNELS;i++)
				{
					getline(ifile,l,'\n') ;
					ocalib_files.push_back(l);
					if(ifile.eof()) break;
				}
				break;
			}
		}
	ifile.close();
	// need to update for more than 4 channels
	for(unsigned int i=0;i<ocalib_files.size();i++)
	cout<<"i = "<<i<<" : "<<ocalib_files[i]<<endl;
	if(ocalib_files.size()!=NUMBER_OF_CHANNELS)
	{
		fprintf(stderr,"all caliberations files not found !!%d /%d \n",int(ocalib_files.size()),NUMBER_OF_CHANNELS);
	}
	
	vector<vector<double>> chanel_calib;
	double* waveform;
	waveform= new double [4*1024*2*EVENTS_IN_A_FRAME];
	
	cout<<"[4*1024*2*EVENTS_IN_A_FRAME]  = "<<4*1024*2*EVENTS_IN_A_FRAME<<"  "<<EVENTS_IN_A_FRAME<<endl;
	int fstatus=0;
	int start=0;
	int wpos=0;
	int eventCOUNT=0;
	vector<double> calib;
	for(int j=0;j<1024;j++)
			calib.push_back(0.0);
	for(unsigned int i=0;i<ocalib_files.size();i++)
	{
		start=0;
		eventCOUNT=0;
		fstatus=0;

		for(int j=0;j<1024;j++)
			calib[j]=0.0;
		for(int j=0;j<EVENTS_IN_A_FRAME;j++)
			for(int k=0;k<1024*2;k++)
				waveform[wpos]=-1;
		cout<<"Starting "<<ocalib_files[i]<<"\n";
		while(fstatus==0)
		{	
			if(start > 40) break;
			cout<<"\t\t for event ids  "<<start<<" to "<<start+EVENTS_IN_A_FRAME-1<<"\n";
			fstatus=get_events(ocalib_files[i].c_str(),waveform,start,start+EVENTS_IN_A_FRAME-1);
 			for(int j=0;j<EVENTS_IN_A_FRAME;j++)
			{	
				wpos=j*(1024*2*4)+i*(1024*2);
				/*for(int k=0;k<1024;k++)
					cout<<" i "<<i<<" , j = "<<j<<" , k = "<<k<<" , wf = "<<waveform[wpos++]<<"->"<<waveform[wpos++]<<endl;	
				continue;*/
				if(waveform[wpos]==0 and waveform[wpos+1]==0 and waveform[wpos+2]==0 and waveform[wpos+3]==0)
				{
				//	cout<<"\n breaking the loop since -1\n";
					fstatus=-1;
					break;
				}
				double to=0;
				for(int k=0;k<1024;k++)
						{
						//	cout<<" i "<<i<<" , j = "<<j<<" , k = "<<k<<" , wf = "<<waveform[wpos+1]<<endl;	
							calib[k]+=waveform[wpos+1]; //i^th channel
							waveform[wpos++]=0;
							waveform[wpos++]=0;
							to+=calib[k];
						}
				//cout<<"\n for event j = "<<j<<" calb = "<<to<<endl;
				eventCOUNT+=1;
			}
			
			start+=EVENTS_IN_A_FRAME;
		}
		cout<<ocalib_files[i]<<" done at "<<eventCOUNT<<endl;
		for(int j=0;j<1024;j++)
			calib[j]/=eventCOUNT;
		chanel_calib.push_back(calib);
	}
	for(unsigned int i=0;i<chanel_calib.size();i++)
		{
			cout<<"for  channnel  i = "<<i<<" \n\n";
			for(int j=0;j<1024;j++)
				cout<<chanel_calib[i][j]<<",";
		cout<<"\n\n\n";
		}
	
	fstream ofile_bin,ofile_txt;
	ofile_bin.open("calib/toffset_calib.dat",ios::out | ios::binary);
	ofile_txt.open("calib/toffset_calib.txt",ios::out );
	
	double val;
	for(unsigned int i=0;i<chanel_calib.size();i++)
	{
		ofile_txt<<"ch id : "<<to_string(i)<<"\n";
		ofile_bin.write((char *)(&i),sizeof(i));
		for(int j=0;j<1024;j++)
		{
			ofile_txt<<j<<","<<chanel_calib[i][j]<<"\n";
			val=chanel_calib[i][j];
			ofile_bin.write((char *)(&val),sizeof(val));
		}
	}
	ofile_bin.close();
	ofile_txt.close();
	return 0;
}


int get_event_adcSave(const char * fname,double * waveformOUT,int start_eventID,int end_evetID)
{
	DRS_EVENT  anevent;
	//EHEADER anevent.eheader;
	int channels;
	float * db_buffr_t,* db_buffr_w;
	
	fstream ifile;
	ifile.open(fname,ios::in | ios::binary);
	if(!ifile.is_open())
	{
		fprintf(stderr,"\n ERROR HAPPEND !! FILE DOES NOT EXIST !! \n");
		fprintf(stderr,"fname : ");
		return -1;
	}
	int id=start_eventID;
	int waveform_id=0;
	ifile.seekg(0,ios_base::end);
	int y=ifile.tellg();
	if(start_eventID*EVENT_SIZE_BYTES_4channelADC >= y)
		return -2;
	ifile.seekg(start_eventID*EVENT_SIZE_BYTES_4channelADC,ios_base::beg);
	ifile.read((char *)(&anevent.eheader),sizeof(anevent.eheader));
	while(!ifile.eof() and id<end_evetID)
	{
		id++;
		ifile.read((char *)(&channels),sizeof(channels));
		anevent.waveform.clear();
		anevent.time.clear();
		for( int i=0;i<channels;i++)
		{
			db_buffr_t=new float[1024];
			ifile.read((char *)(db_buffr_t),1024*sizeof(float));
			anevent.time.push_back(db_buffr_t);
			db_buffr_w=new float[1024];
			ifile.read((char *)(db_buffr_w),1024*sizeof(float));
			anevent.waveform.push_back(db_buffr_w);
			for(int j=0;j<1024;j++)
			{
				waveformOUT[waveform_id++]=db_buffr_t[j];
				waveformOUT[waveform_id++]=db_buffr_w[j];
			}
		}
		ifile.read((char *)(&anevent.eheader),sizeof(anevent.eheader));
	}
	ifile.close();
	if (id<end_evetID) return id;
	
	return 0;
}

int get_events(const char * fname,double * waveformOUT,int start_eventID,int end_evetID,bool offset_caliberate)
{
   FHEADER  fh;
   THEADER  th;
   BHEADER  bh;
   EHEADER  eh;
   TCHEADER tch;
   CHEADER  ch;
   unsigned int scaler;
   unsigned short voltage[1024];
   double waveform[16][4][1024], time[16][4][1024];
   float bin_width[16][4][1024];
   int i, j, b, chn, n, chn_index, n_boards;
   double t1, t2, dt;
   char filename[256];
	double ch_offset[4][1024];
   strcpy(filename, fname);
   // open the binary waveform file
    FILE *f = fopen(filename, "rb");
    fstream ifile;
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
   if (fh.version != '2') 
   {
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
   
   int waveWrite_pos=0;   
	
	int start_offset=start_eventID*EVENT_SIZE_BYTES;
	int seekStatus=fseek(f,start_offset,SEEK_CUR);
	if (seekStatus)
	{
		fprintf(stderr,"Invalid start event ID (offset = %d )",start_offset);
		return 6;
	}
	
	
	// Read the channel offsets
	if (offset_caliberate)
	{
		ifile.open("calib/offset_calib.dat",ios::in | ios::binary);
		if(!ifile.is_open())
		{
			fprintf(stderr,"CONFIG FILE DOES NOT EXIST !!");
			return  6 ;
		}
		int bid;
		while(!ifile.eof())
			{
				ifile.read((char *)(&bid),sizeof(bid));
				if(ifile.eof()) break;
				for(int j=0;j<1024;j++)
					{
						ifile.read((char *)(&ch_offset[bid][j]),sizeof(double));
					}
			}
		ifile.close();
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
            i=fread(&scaler, sizeof(int), 1, f);
            i=fread(voltage, sizeof(short), 1024, f);
            
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
               
               if(!offset_caliberate) ch_offset[chn][i]=0.0;
               waveformOUT[waveWrite_pos++]=waveform[b][chn][i]-ch_offset[chn][i];
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

int save_event_binary(const char * fname,DRS_EVENT anevent[],int num_events)
{
	fstream ofile;
	ofile.open(fname,ios::out | ios::binary |ios::app );
	for(int j=0;j<num_events;j++)
	{
		int channels=anevent[j].waveform.size();
		ofile.write((char *)(&anevent[j].eheader),sizeof(anevent[j].eheader));
		ofile.write((char *)(&channels),sizeof(channels));
		//cout<<" chn = "<<channels<<"\n";
		for( int i=0;i<channels;i++)
		{
			ofile.write((char *)(anevent[j].time[i]),1024*sizeof(anevent[j].time[i][0]));
			ofile.write((char *)(anevent[j].waveform[i]),1024*sizeof(anevent[j].waveform[i][0]));
		}
	}
	ofile.close();
}

vector<DRS_EVENT> read_event_binary(const char * fname)
{
	vector<DRS_EVENT> eventList;
	DRS_EVENT  anevent;
	//EHEADER anevent.eheader;
	int channels;
	float * db_buffr;
	
	fstream ifile;
	ifile.open(fname,ios::in | ios::binary);
	if(!ifile.is_open())
	{
		cout<<"\n ERROR HAPPEND !! FILE DOES NOT EXIST !! \n";
		cout<<"fname : "<<fname;
		exit(0);
	}
	int id=0;
	ifile.read((char *)(&anevent.eheader),sizeof(anevent.eheader));
	while(!ifile.eof())
	{
		id++;
		cout<<"at loop count = "<<id<<"\n";
		//if(id>10) break;
		ifile.read((char *)(&channels),sizeof(channels));
		cout<<"EVENT ID  = "<<anevent.eheader.event_serial_number<<"\n";
		cout<<"yr = "<<anevent.eheader.year<<"\n";
		cout<<"month = "<<anevent.eheader.month<<"\n";
		cout<<"day  = "<<anevent.eheader.day<<"\n";
		cout<<"hr  = "<<anevent.eheader.hour<<"\n";
		cout<<"min  = "<<anevent.eheader.minute<<"\n";
		cout<<"sec  = "<<anevent.eheader.second<<"\n";
		cout<<" chn = "<<channels<<"\n";
		
		anevent.waveform.clear();
		anevent.time.clear();
		for( int i=0;i<channels;i++)
		{
			db_buffr=new float[1024];
			ifile.read((char *)(db_buffr),1024*sizeof(float));
			anevent.time.push_back(db_buffr);
			db_buffr=new float[1024];
			ifile.read((char *)(db_buffr),1024*sizeof(float));
			anevent.waveform.push_back(db_buffr);
		}
		eventList.push_back(anevent);
		ifile.read((char *)(&anevent.eheader),sizeof(anevent.eheader));
	}
	ifile.close();
	return eventList;
}



int get_channel_offsets(string fname,vector<double *> *calib_data,int channels[])
{
	fstream ifile;
	int ch;
	double *db_buffr;
	calib_data->clear();
	int idx=0;
	ifile.open(fname.c_str(),ios::in | ios::binary);
	if(!ifile.is_open())
	{
		cout<<"\n ERROR HAPPEND !! FILE DOES NOT EXIST !! \n";
		cout<<"fname : "<<fname;
		exit(0);
	}
	ifile.read((char *)(&ch),sizeof(int));
	while(!ifile.eof())
	{
		channels[idx]=ch;idx++;
		db_buffr=new double[1024];
		ifile.read((char *)(db_buffr),1024*sizeof(double));
		calib_data->push_back(db_buffr);
		ifile.read((char *)(&ch),sizeof(ch));
	}
	return 0;
}




double get_energy(float waveform[8][1024],float time[8][1024],int channel, double trigger_level,
												double neg_offset,double integrate_window,double freq, bool falling_edge )
{
	float trig_sign=1;
	if(!falling_edge) trig_sign=-1;
	int start=0;
	double integral=0;
	int i;
	i=0;
	double dt=0,window=0;
	for(i=0;i<1024;i++)
	{
		if(trig_sign*waveform[channel][i]<trig_sign*trigger_level)
			{
				start=i;
				break;
			}
	}
	if(i==1024)
	{
		return 0.0;
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
	dt=0;
	window=0;
	while(i<1024 and window<integrate_window)
	{
		dt=time[channel][i]-time[channel][i-1];
		window+=dt;
		integral+=waveform[channel][i]*dt;
		i++;
	}
	return -1*integral/TERMINAL_RESISTANCE;
}

