########################################################
#
#  Makefile for drsosc, drscl and drs_exam 
#  executables under linux
#
#  Requires wxWidgets 2.8.9 or newer
#
########################################################

IDIR=include
SRCDIR=src
OBJDIR=obj
SLIBDIR=lib

CXX=gcc

CFLAGS=-I $(IDIR)/ -Wall -I $(ROOTSYS)/include  -L$(ROOTSYS)/lib 


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

CFLAGS        += -g -O2 -Wall -Wuninitialized -fno-strict-aliasing -Iinclude -I/usr/local/include -D$(DOS) -DHAVE_USB -DHAVE_LIBUSB10 -DUSE_DRS_MUTEX `root-config  --cflags`
LIBS          = -lpthread -lutil -lusb-1.0 -lstdc++ -lm -lGui -lCore -lRIO -lNet -lHist -lGraf -lGraf3d -lGpad -lTree -lRint -lPostscript -lMatrix -lPhysics -lMathCore -lThread -lMultiProc -pthread -lm -ldl -rdynamic `root-config  --ldflags`


ifeq ($(OS),Darwin)
CFLAGS        += -stdlib=libstdc++
endif         


# wxWidgets libs and flags
WXLIBS        = $(shell wx-config --libs)
WXFLAGS       = $(shell wx-config --cxxflags)

_CPP_OBJ       = DRS.o averager.o 
_OBJECTS       = musbstd.o mxml.o strlcpy.o
CPP_OBJ  := $(_CPP_OBJ:%.o=$(OBJDIR)/%.o)
OBJECTS  := $(_OBJECTS:%.o=$(OBJDIR)/%.o)

see:
	echo $(CPP_OBJ) $(OBJECTS)

ifeq ($(OS),Darwin)
all: drs_exam muonDet
else
all: muonDet drs_exam
endif

drs_exam: $(OBJECTS) $(OBJDIR)/drs_exam.o
	$(CXX) $(CFLAGS) $^ -o drs_exam $(LIBS) $(WXLIBS)

muonDet: $(OBJECTS) $(CPP_OBJ) $(OBJDIR)/DRS4v5_lib.o $(OBJDIR)/muonDet.o $(OBJDIR)/musbstd.o
	$(CXX) $(CFLAGS)  $^ -o $@ $(LIBS) $(WXLIBS)

ramAcces : $(OBJECTS) $(CPP_OBJ) $(OBJDIR)/DRS4v5_lib.o $(OBJDIR)/ramAcces.o $(OBJDIR)/musbstd.o
	$(CXX) $(CFLAGS)  $^ -o $@ $(LIBS) $(WXLIBS)

tdcDAQ : $(OBJECTS) $(CPP_OBJ) $(OBJDIR)/DRS4v5_lib.o $(OBJDIR)/tdcDAQ.o $(OBJDIR)/musbstd.o
	$(CXX) $(CFLAGS)  $^ -o $@ $(LIBS) $(WXLIBS)

try: $(OBJDIR)/DRS4v5_lib.o $(OBJDIR)/try.o 
	$(CXX) $(CFLAGS)  $^ -o $@ $(LIBS) 

libdrs4: $(SRCDIR)/DRS4v5_lib.cpp
	$(CC) -shared -fPIC -o $(SLIBDIR)/$@.so  $^ $(CFLAGS) $(LIBS)

$(OBJDIR)/drs_exam.o: $(SRCDIR)/drs_exam.cpp $(IDIR)/mxml.h $(IDIR)/DRS.h
	$(CXX) $(CFLAGS) -c $< -o $@ 

$(OBJDIR)/muonDet.o: $(SRCDIR)/muonDet.cpp $(IDIR)/mxml.h $(IDIR)/DRS.h
	$(CXX) $(CFLAGS) -c $< -o $@ 

$(OBJDIR)/tdcDAQ.o: $(SRCDIR)/tdcDAQ.cpp $(IDIR)/mxml.h $(IDIR)/DRS.h
	$(CXX) $(CFLAGS) -c $< -o $@ 

$(OBJDIR)/ramAcces.o: $(SRCDIR)/ramAcces.cpp $(IDIR)/mxml.h $(IDIR)/DRS.h
	$(CXX) $(CFLAGS) -c $< -o $@ 

$(OBJDIR)/try.o: $(SRCDIR)/try.cpp $(SRCDIR)/DRS4v5_lib.cpp $(IDIR)/DRS4v5_lib.h $(IDIR)/drsoscBinary.h
	$(CXX) $(CFLAGS) -c $< -o $@

$(OBJDIR)/DRS4v5_lib.o: $(SRCDIR)/DRS4v5_lib.cpp $(IDIR)/DRS4v5_lib.h $(IDIR)/drsoscBinary.h
	$(CXX) $(CFLAGS) -c $< -o $@ 

$(CPP_OBJ):$(OBJDIR)/%.o: $(SRCDIR)/%.cpp 
	$(CXX) $(CFLAGS) $(WXFLAGS) -c $< -o $@ 

$(OBJECTS):$(OBJDIR)/%.o: $(SRCDIR)/%.c 
	mkdir -p obj	
	$(CC) $(CFLAGS) -c $< -o $@ 

clean:
	rm -f *.o obj/*.o lib/*.so drs_exam muonDet try 

