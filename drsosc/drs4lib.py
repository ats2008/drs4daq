from ctypes import *
import numpy as np

drs4lib=CDLL('./lib/libdrs4.so')

def get_events(fname=None,start_eventID=0,end_evetID=0):
    try :
        f=open(fname,'r')
        f.close()
    except :
        print("pass a valid filename")
        return

    num=(end_evetID-start_eventID+1)*4*1024*2
    if num<0:
        print("enter valid start_eventID & end_evetID ")
        return None
    s_id=c_int(start_eventID)
    e_id=c_int(end_evetID)
    arr_type=c_double*num
    _waveformData=arr_type()
    status=drs4lib.get_events(fname.encode('utf-8'),_waveformData,s_id,e_id)
    if status!=0:
    	print("ERROR !! ecode = ",status)
    	return False,None
    waveformData=np.ctypeslib.as_array(_waveformData)
    waveformData=waveformData.reshape((end_evetID-start_eventID+1),4,1024,2)
    print(waveformData.nbytes/1024**2 ," MB occupied")
    return True,waveformData
