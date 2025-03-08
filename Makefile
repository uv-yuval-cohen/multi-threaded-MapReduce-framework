CC=g++
CXX=g++
AR=ar
RANLIB=ranlib

LIBSRC=MapReduceFramework.cpp Barrier.cpp
LIBOBJ=$(LIBSRC:.cpp=.o)

HEADERS=MapReduceFramework.h MapReduceClient.h JobContext.h Barrier.h

INCS=-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)
CXXFLAGS = -Wall -std=c++11 -g $(INCS)

MAPREDUCELIB = libMapReduceFramework.a
TARGETS = $(MAPREDUCELIB)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex3.tar
TARSRCS=$(LIBSRC) $(HEADERS) Makefile README $(MAPREDUCELIB)

all: $(TARGETS)

$(MAPREDUCELIB): $(LIBOBJ)
	$(AR) rcs $@ $^
	$(RANLIB) $@

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(TARGETS) $(LIBOBJ) *~ *core

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

depend:
	makedepend -- $(CFLAGS) -- $(LIBSRC)

.PHONY: all clean tar depend
