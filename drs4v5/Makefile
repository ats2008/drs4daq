########################################################
#
#  Makefile for drsosc, drscl and drs_exam 
#  executables under linux
#
#  Requires wxWidgets 2.8.9 or newer
#
########################################################

# check if wxWidgets is installed
HAVE_WX       = $(shell which wx-config)
ifeq ($(HAVE_WX),)
$(error Error: wxWidgets required to compile "drsosc")
endif

# check for OS
OS            = $(shell uname)
ifeq ($(OS),Darwin)
DOS           = OS_DARWIN
else
DOS           = OS_LINUX
endif

CFLAGS        = -g -O2 -Wall -Wuninitialized -fno-strict-aliasing -Iinclude -I/usr/local/include -D$(DOS) -DHAVE_USB -DHAVE_LIBUSB10 -DUSE_DRS_MUTEX
LIBS          = -lpthread -lutil -lusb-1.0

ifeq ($(OS),Darwin)
CFLAGS        += -stdlib=libstdc++
endif         

# wxWidgets libs and flags
WXLIBS        = $(shell wx-config --libs)
WXFLAGS       = $(shell wx-config --cxxflags)

CPP_OBJ       = DRS.o averager.o ConfigDialog.o DOFrame.o DOScreen.o DRSOsc.o MeasureDialog.o Measurement.o Osci.o InfoDialog.o DisplayDialog.o AboutDialog.o EPThread.o TriggerDialog.o rb.o
OBJECTS       = musbstd.o mxml.o strlcpy.o


ifeq ($(OS),Darwin)
all: drsosc drscl drs_exam muonDet drs_exam_multi drsinface DRSOsc.app
else
all: drsosc drscl drs_exam muonDet drs_exam_multi drsinface
endif

DRSOsc.app: drsosc
	-mkdir DRSOsc.app
	-mkdir DRSOsc.app/Contents
	-mkdir DRSOsc.app/Contents/MacOS
	-mkdir DRSOsc.app/Contents/Resources
	-mkdir DRSOsc.app/Contents/Resources/English.lproj
	echo 'APPL????' > DRSOsc.app/Contents/PkgInfo
	cp Info.plist DRSOsc.app/Contents/Info.plist
	cp DRSOsc.icns DRSOsc.app/Contents/Resources
	cp drsosc DRSOsc.app/Contents/MacOS/DRSOsc


drsosc: $(OBJECTS) $(CPP_OBJ) main.o
	$(CXX) $(CFLAGS) $(OBJECTS) $(CPP_OBJ) main.o -o drsosc $(LIBS) $(WXLIBS)

drsinface: $(OBJECTS) DRS.o averager.o drsinface.o
	$(CXX) $(CFLAGS) $(OBJECTS) DRS.o averager.o drsinface.o -o drsinface $(LIBS) $(WXLIBS)

drscl: $(OBJECTS) DRS.o averager.o drscl.o
	$(CXX) $(CFLAGS) $(OBJECTS) DRS.o averager.o drscl.o -o drscl $(LIBS) $(WXLIBS)

drs_exam: $(OBJECTS) DRS.o averager.o drs_exam.o
	$(CXX) $(CFLAGS) $(OBJECTS) DRS.o averager.o drs_exam.o -o drs_exam $(LIBS) $(WXLIBS)

muonDet: $(OBJECTS) DRS.o averager.o muonDet.o DRS4v5_lib.o
	$(CXX) $(CFLAGS) $(OBJECTS) DRS.o averager.o muonDet.o -o muonDet DRS4v5_lib.o $(LIBS) $(WXLIBS)

try: DRS4v5_lib.o try.o 
	$(CXX) $(CFLAGS)  DRS4v5_lib.o try.o -o try $(LIBS) 

drs_exam_multi: $(OBJECTS) DRS.o averager.o drs_exam_multi.o
	$(CXX) $(CFLAGS) $(OBJECTS) DRS.o averager.o drs_exam_multi.o -o drs_exam_multi $(LIBS) $(WXLIBS)

main.o: src/main.cpp include/mxml.h include/DRS.h
	$(CXX) $(CFLAGS) $(WXFLAGS) -c $<

drsinface.o: src/drsinface.cpp include/mxml.h include/DRS.h
	$(CXX) $(CFLAGS) -c $<

drscl.o: src/drscl.cpp include/mxml.h include/DRS.h
	$(CXX) $(CFLAGS) -c $<

drs_exam.o: src/drs_exam.cpp include/mxml.h include/DRS.h
	$(CXX) $(CFLAGS) -c $<

muonDet.o: src/muonDet.cpp include/mxml.h include/DRS.h
	$(CXX) $(CFLAGS) -c $<

try.o: src/try.cpp src/DRS4v5_lib.cpp include/DRS4v5_lib.h include/drsoscBinary.h
	$(CXX) $(CFLAGS) -c $<

DRS4v5_lib.o: src/DRS4v5_lib.cpp include/DRS4v5_lib.h include/drsoscBinary.h
	$(CXX) $(CFLAGS) -c $<

drs_exam_multi.o: src/drs_exam_multi.cpp include/mxml.h include/DRS.h
	$(CXX) $(CFLAGS) -c $<

$(CPP_OBJ): %.o: src/%.cpp include/%.h include/mxml.h include/DRS.h
	$(CXX) $(CFLAGS) $(WXFLAGS) -c $<

$(OBJECTS): %.o: src/%.c include/mxml.h include/DRS.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o drscl drsosc

