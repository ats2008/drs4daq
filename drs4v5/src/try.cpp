#include<iostream>
#include<drsoscBinary.h>
#include<DRS4v5_lib.h>


using namespace std;

int main()
{
	DRS_EVENT anevent;
	//EHEADER aheder;
	strcpy(anevent.eheader.event_header,"ats");
	anevent.eheader.event_serial_number=12;
	anevent.eheader.year=2020;
	anevent.eheader.month=2;
	anevent.eheader.day=3;
	anevent.eheader.hour=4;
	anevent.eheader.minute=5;
	anevent.eheader.second=6;
	anevent.eheader.millisecond=7;
	anevent.eheader.range=0;
	double * ptr;
	for(int j=0;j<4;j++)
	{
		ptr=new double [10];
		anevent.waveform.push_back(ptr);
		ptr=new double [10];
		anevent.time.push_back(ptr);
	}
	string fname="ats.dat";
	
	for(int k=0;k<10;k++)
	{
		anevent.eheader.year=20*k;
		for(int i=0;i<10;i++)
			for(int j=0;j<4;j++)
				{
					anevent.waveform[j][i]=j*10+i+40*k;
					anevent.time[j][i]=40*k-j*10+i;
				}
		save_event_binary(fname.c_str(),anevent);
	}

//int save_event_binary(const char * fname,EHEADER event_header,int channels,double  waveform[][10],double  time[][10])

	return 0;

}
