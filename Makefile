#!/usr/bin/make -f
#
OPENCV_LIBS=-lopencv_core -lopencv_highgui
LIBS= $(OPENCV_LIBS) -lstdc++

CC= g++
CXXFLAGS= -c -Wall -O3 -g $(INCLUDE_DIR)
#LDFLAGS= -fopenmp

#SRCS:=$(wildcard *.cc) $(wildcard *.h) $(wildcard *.cu)
#OBJS:=$(patsubst %.cc,%.o,$(SRCS))
DSTS:=dicom_test


all: $(DSTS)

dicom_test: dicom_test.o
	$(CC) $(LDFLAGS) -o $@ dicom_test.o $(LIBS)

dicom_test.o: dicom.h dicom_test.cc

clean:
	-rm *.o $(DSTS) *~