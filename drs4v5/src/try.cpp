#include<iostream>
#include<drsoscBinary.h> 	
#include<DRS4v5_lib.h>


using namespace std;

int main()
{
	DRS_EVENT anevent[5];
	//EHEADER aheder;
	for(int k=0;k<5;k++)
	{
		strcpy(anevent[k].eheader.event_header,"ats");
		anevent[k].eheader.event_serial_number=12;
		anevent[k].eheader.year=2020;
		anevent[k].eheader.month=2;
		anevent[k].eheader.day=3;
		anevent[k].eheader.hour=4;
		anevent[k].eheader.minute=5;
		anevent[k].eheader.second=6;
		anevent[k].eheader.millisecond=7;
		anevent[k].eheader.range=0;
	}
	float * ptr;
	for(int k=0;k<5;k++)
	for(int j=0;j<4;j++)
	{
		ptr=new float [10];
		anevent[k].waveform.push_back(ptr);
		ptr=new float [10];
		anevent[k].time.push_back(ptr);
	}
	string fname="ats.dat";
	cout<<"\nwriting begins";
	int eid=0;
	int p=0;
	for(int k=0;k<5;k++)
	{
	anevent[k].eheader.event_serial_number=eid++;
		anevent[k].eheader.year=20*k;
			for(int j=0;j<4;j++)
			for(int i=0;i<10;i++)
				{
					anevent[k].waveform[j][i]=p++;
					anevent[k].time[j][i]=p++;
				}
	}
	save_event_binary(fname.c_str(),anevent,5);
	for(int k=0;k<5;k++)
		anevent[k].clear_data();
	cout<<"\nwriting ends";
	
	vector<DRS_EVENT> elist;
	cout<<"\n\n\n\n reading begins";
	fname="data/atsrun/events.dat";
	elist=read_event_binary(fname.c_str());
	cout<<"\n\n\nreading ends with  len = "<<elist.size()<<"\n";
	int l;
	l=0;
	vector<DRS_EVENT>::iterator evt_itr =elist.begin();
	vector<float *>::iterator titr,witr;
	for (;evt_itr!=elist.end();evt_itr++)
	{
		cout<<"EVENT ID  = "<<(evt_itr->eheader).event_serial_number<<"\n";
		cout<<"yr = "<<(evt_itr->eheader).year<<"\n";
		cout<<"month = "<<(evt_itr->eheader).month<<"\n";
		cout<<"day  = "<<(evt_itr->eheader).day<<"\n";
		cout<<"hr  = "<<(evt_itr->eheader).hour<<"\n";
		cout<<"min  = "<<(evt_itr->eheader).minute<<"\n";
		cout<<"sec  = "<<(evt_itr->eheader).second<<"\n";
		cout<<"millis  = "<<(evt_itr->eheader).millisecond<<"\n";
		witr=(evt_itr->waveform).begin();
		titr=(evt_itr->time).begin();
		l=0;
		for (;witr!=(evt_itr->waveform).end();titr++,witr++)
		{
			cout<<"for ch  = "<<l++<<endl;
			for(int i=0;i<10;i++)
			cout<<"("<<(*titr)[i]<<","<<(*witr)[i]<<") ,";
			cout<<"\n";
		}
		cout<<"\n\n";
		evt_itr->clear_data();
	}
	
	return 0;

}
