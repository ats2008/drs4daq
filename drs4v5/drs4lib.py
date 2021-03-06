from ctypes import *
import numpy as np

drs4lib=CDLL('./lib/libdrs4.so')

def get_drsoscBinary_events(fname=None,start_eventID=0,end_evetID=0,offset_caliberation=False):
    try :
        f=open(fname,'r')
        f.close()
    except :
        print("pass a valid filename")
        return False,None

    num=(end_evetID-start_eventID+1)*4*1024*2
    if num<0:
        print("enter valid start_eventID & end_evetID ")
        return None
    s_id=c_int(start_eventID)
    e_id=c_int(end_evetID)
    offset_caliberate=c_bool(offset_caliberation)
    arr_type=c_double*num
    _waveformData=arr_type()
    status=drs4lib.get_events(fname.encode('utf-8'),_waveformData,s_id,e_id,offset_caliberate)
    print(status)
    if status!=0:
    	print("ERROR !! ecode = ",status)
    	return False,None
    waveformData=np.ctypeslib.as_array(_waveformData)
    waveformData=waveformData.reshape((end_evetID-start_eventID+1),4,1024,2)
    print(waveformData.nbytes/1024**2 ," MB occupied")
    return True,waveformData

def get_adc_events(fname=None,start_eventID=0,end_evetID=0):
    try :
        f=open(fname,'r')
        f.close()
    except :
        print("pass a valid filename")
        return False,None

    num=(end_evetID-start_eventID+1)*4*1024*2
    if num<0:
        print("enter valid start_eventID & end_evetID ")
        return None
    s_id=c_int(start_eventID)
    e_id=c_int(end_evetID)
    arr_type=c_double*num
    _waveformData=arr_type()
    status=drs4lib.get_event_adcSave(fname.encode('utf-8'),_waveformData,s_id,e_id)
    waveformData=np.ctypeslib.as_array(_waveformData)
    waveformData=waveformData.reshape((end_evetID-start_eventID+1),4,1024,2)
    return status,waveformData
    

get_energy_c=drs4lib.get_energy
get_energy_c.restype=c_double

def get_energy(waveform,tme,channel,trigger_level,neg_offset,integrate_window,frequency,falling_edge):
	wf=np.ctypeslib.as_ctypes(waveform.astype(c_float))
	tm=np.ctypeslib.as_ctypes(tme.astype(c_float))
	ch=c_int(channel)
	trig=c_double(trigger_level)
	n_off=c_double(neg_offset)
	i_window=c_double(integrate_window)
	freq=c_double(frequency)
	f_edge=c_bool(falling_edge)
	
	en=get_energy_c(wf,tm,ch,trig,n_off,i_window,freq,f_edge)
	return en


