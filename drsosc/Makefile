CC=g++

IDIR=include
SRCDIR=src
OBJDIR=obj
SLIBDIR=lib

CFLAGS=-I $(IDIR) -Wall

SOURCES  := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

#main_obj=$(wildcard $(SRCDIR)/*.cpp)
rb_objs=$(OBJDIR)/read_binary.o
drslib_src=$(SRCDIR)/DRS4v5_lib.cpp
main_obj=$(OBJDIR)/DRS4v5_lib.o $(OBJDIR)/main.o 

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@
	echo "Compiled "$<" successfully!"

main: $(main_obj)
	$(CC) -o $@ $^ $(CFLAGS)

read_binary: $(rb_objs)
	$(CC) -o $@ $^ $(CFLAGS)
	
libdrs4: $(drslib_src)
	$(CC) -shared -o $(SLIBDIR)/$@.so -fPIC $^ $(CFLAGS)


.PHONY: clean

clean:
	rm -f $(OBJDIR)/*.o *~ $(INCDIR)/*~  $(SLIBDIR)/*.so
