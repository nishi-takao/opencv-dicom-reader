// -*- c++ -*-
//
// Time-stamp: <2015-04-22 09:59:37 zophos>
//
#define DEBUG

#include <iostream>
#include <fstream>
#include "dicom.h"

#include <opencv2/highgui/highgui.hpp>

int main(int argc,char *argv[])
{
    std::ifstream ifs(argv[1]);

    if(!ifs)
        throw("");

    VVV::Dicom d(ifs);
    ifs.close();

    cv::Mat src=d.image();
    cv::Mat dst;
    src.convertTo(dst,CV_8UC1);

    cv::namedWindow("img");
    cv::imshow("img",dst);
    cv::waitKey(0);
}
