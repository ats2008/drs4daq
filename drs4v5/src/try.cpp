#include<iostream>
#include<drsoscBinary.h> 	
#include<DRS4v5_lib.h>


using namespace std;

int main()
{

	cout<<"\n"<<1.23456<<endl;
	cout.setf(ios::fixed);
	cout.setf(ios::showpoint);
	cout.precision(2);
	cout<<"\n2   :    "<<1.23456<<endl;
	cout.precision(4);

	cout<<"\n"<<4<<"  : "<<1.23456<<endl;
	return 0;
}
