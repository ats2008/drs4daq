#!/usr/bin/ python3

import matplotlib.pyplot as plt 
import numpy as np
import sys

def find_peak(array,window=10):
    wb2=int(window/2)+1
    n=len(array)-wb2
    peaks=[]
    peak_vals=[]
    for i in range(wb2):
        m=np.amax(array[0:window])
        if (m==array[i]):
            peaks.append(i)
            peak_vals.append(array[i])
    i+=1
    while i<n-wb2:
#         print(array[i-wb2],array[i+wb2])
        m=np.amax(array[i-wb2:i+wb2])
        if (m==array[i]):
            peaks.append(i)
            peak_vals.append(array[i])
            i+=1
            continue
        i+=1
    return peaks,peak_vals

ename=sys.argv[1]
print(" the run name : ",ename)
fname="data/"+ename+"/eDeposit.txt"
try :
    f=open(fname,'r')
except:
    print("\n Unable to find event file ",fname,"\n exiting !!")
    exit()
    
l=f.readline()
eid=[]
energy=[]
while l:
    items=l[:-1].split(',')
    eid.append(int(items[0]))
    energy.append(float(items[1]))
    l=f.readline()
eid=np.array(eid)
energy=np.array(energy)

fig,ax=plt.subplots(nrows=1,ncols=2,figsize=(18,6))
x=ax[0].hist(energy,bins=200)
xl=ax[1].hist(energy,bins=200)
ax[0].set_title("Histogram of charge deposited on ADC")
ax[1].set_title("Histogram of log(charge) deposited on ADC")
ax[0].set_xlabel("charge depositrd in pC")
ax[0].set_ylabel("# count")
ax[1].set_ylabel("# count")
ax[1].set_yscale('log')
xhere=(x[1][:-1]+x[1][1:])/2
yhere=x[0]
pk,pkv=find_peak(yhere,20)
plt.savefig('data/'+ename+'/'+'hist.png',dpi=400)
f=open('data/'+ename+'/'+'peaks.txt','w')
f.write("#peaks \n#energy,count\n")
print("\t    peaks \n\tenergy\tcount")
for i,j in zip(pk,pkv):
    f.write(str(xhere[i])+","+str(j)+'\n')
    print("\t",np.round(xhere[i],2),"\t",str(j))
print("peaks saved at ",'data/'+ename+'/'+'peaks.txt')
print("histogram saved at ",'data/'+ename+'/'+'hist.png')
f.close()
pkx=xhere[pk]
ax[0].scatter(pkx,pkv,s=40,c='r',label='identifed peaks')
ax[1].scatter(pkx,pkv,s=40,c='r',label='identifed peaks')
ax[0].annotate('Total # Events = '+str(len(energy)), xy=(0.6, 0.5), xycoords='axes fraction')
ax[1].annotate('Total # Events = '+str(len(energy)), xy=(0.6, 0.5), xycoords='axes fraction')
ax[0].legend()

plt.show()
