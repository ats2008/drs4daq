Reopsitory for the muon trigger system using DRS4v5.

## Instalation of DRS-OSC 


## Intaling pre-requisits

you may have to intall `git` and `g++` as well. 

wx libraries for GUI
```bash
sudo apt-get install libwxgtk3.0-dev
```

usb interface libraries
```bash
sudo apt-get install libusb-1.0-0-dev
```

**ROOT** : For daq's data storage and plotting/fitting utilities

for Ubuntu 18.4  [ as of Aug 13 , 2022]
```bash
cd ~
wget https://root.cern/download/root_v6.26.06.Linux-ubuntu18-x86_64-gcc7.5.tar.gz .
tar -xzvf  root_v6.26.06.Linux-ubuntu18-x86_64-gcc7.5.tar.gz 
echo "source ~/root/bin/thisroot.sh" >>~/.bashrc
```

please refer [roots's inatallation help here](https://root.cern/install/)

Update the `ROOT` libraries to the `LD_LIBRARY_PATH` for the super user
```bash
sudo ldconfig $ROOTSYS/lib
```

## Compiling the `muonDet` module
```bash
cd drs4daq
```

```bash
make muonDet
```
### Execuing the Muon DAQ module
```bash
bash ./muondet
```
