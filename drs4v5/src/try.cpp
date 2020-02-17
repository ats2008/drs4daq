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
	cout<<"\nwriting begins";
	int eid=0;
	int p=0;
	for(int k=0;k<5;k++)
	{
	anevent.eheader.event_serial_number=eid++;
		anevent.eheader.year=20*k;
			for(int j=0;j<4;j++)
			for(int i=0;i<10;i++)
				{
					anevent.waveform[j][i]=p++;
					anevent.time[j][i]=p++;
				}
		save_event_binary(fname.c_str(),anevent);
	}
	cout<<"\nwriting ends";
	vector<DRS_EVENT> elist;
	cout<<"\nreading begins";
	elist=read_event_binary(fname.c_str());
	cout<<"\nreading ends with  len = "<<elist.size()<<"\n";
	int l;
	cin>>l;
	vector<DRS_EVENT>::iterator evt_itr =elist.begin();
	vector<double *>::iterator titr,witr;
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
		for (;witr!=(evt_itr->waveform).end();titr++,witr++)
		{
			for(int i=0;i<10;i++)
			cout<<"("<<(*titr)[i]<<","<<(*witr)[i]<<") ,";
			cout<<"\n";
		}
		cout<<"\n\n";
	}
	return 0;

}
