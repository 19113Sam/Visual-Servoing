
/******************************************
 * OpenCV Tutorial: Ball Tracking using   *
 * Kalman Filter                          *
 ******************************************/

// Module "core"
#include <opencv2/core/core.hpp>

// Module "highgui"
#include <opencv2/highgui/highgui.hpp>

// Module "imgproc"
#include <opencv2/imgproc/imgproc.hpp>

// Module "video"
#include <opencv2/video/video.hpp>
#include <opencv2/features2d/features2d.hpp>

// Output
#include <iostream>
#include <stdlib.h>

// Vector
#include <vector>

//ros
#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <pcl_ros/point_cloud.h>
#include <pcl/point_cloud.h>
#include <pcl_conversions/pcl_conversions.h>

//image information msg
#include <object_detection/image_data.h>

//extended_kf
#include <kalman/ekfilter.hpp>

using namespace std;
using namespace cv;

// >>>>> Color to be tracked
//#define MIN_H_BLUE 167
//#define MAX_H_BLUE 237
//#define MIN_H_BLUE 200
//#define MAX_H_BLUE 300
// <<<<< Color to be tracked
//int vmin = 167, vmax = 237, smin = 146;
//int vmin = 86, vmax = 196, smin = 116;
//int vmin = 84, vmax = 202, smin = 152;
//int vmin = 138, vmax = 242, smin = 152;
//int vmin = 205, vmax = 255, smin = 162;
//int vmin = 204, vmax = 256, smin = 216;
int vmin = 106, vmax = 225, smin = 153;

//int vmin = 255, vmax = 255, smin = 0;
//int iLowH =3; int iHighH=15; int iLowS = 88; int iHighS =100; int iLowV = 50; int iHighV = 70;

int iLastX = 0;
int iLastY = 0;
int Pre_iLastX = 0;
int Pre_iLastY = 0;
int predict_x = 0;
int predict_y = 0;
double alpha = 0;

Mat image, imgTh;
bool backprojMode = false;
bool selectObject = false;
int trackObject = 0;
bool showHist = true;
Point origin;
Rect selection;
bool paused= false;
Mat object;
char c;
double xc, yc = 0.0;


static void onMouse( int event, int x, int y, int, void* )
{
    if( selectObject )
    {
        selection.x = MIN(x, origin.x);
        selection.y = MIN(y, origin.y);
        selection.width = std::abs(x - origin.x);
        selection.height = std::abs(y - origin.y);

        selection &= Rect(0, 0, image.cols, image.rows);

    }

    switch( event )
    {
    case EVENT_LBUTTONDOWN:
        origin = Point(x,y);
        selection = Rect(x,y,0,0);

        selectObject = true;
        break;
    case EVENT_LBUTTONUP:
        selectObject = false;
        if( selection.width > 0 && selection.height > 0 )
            trackObject = -1;   // Set up CAMShift properties in main() loop
        break;
    }
}

void MyFilledCircle( Mat img, Point center , int r,int g, int b)
{
    int thickness = 5;
    int lineType = 8;

    circle( img,
            center,
            4,
            Scalar( r, g, b ),
            thickness,
            lineType );
}

void imageCallback(const sensor_msgs::ImageConstPtr& msg)
{
    try
    {
        image = cv_bridge::toCvShare(msg, "bgr8")->image;

    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("Could not convert from '%s' to 'bgr8'.", msg->encoding.c_str());
    }

}


void ptCloudcallBack(const sensor_msgs::PointCloud2::ConstPtr &input, pcl::PointCloud<pcl::PointXYZRGB>::Ptr ptCloud, bool &flag)
{

    pcl::fromROSMsg( *input, *ptCloud );
    flag = true;
}


int main(int argc, char ** argv)
{
    //VideoCapture cap;
    Rect trackWindow;
    int hsize = 16;
    float hranges[] = {0,180};
    const float* phranges = hranges;
    vector<vector<cv::Point> > contours;

    // >>>> Kalman Filter
    int stateSize = 6;
    int measSize = 4;
    int contrSize = 0;

    unsigned int type = CV_32F;
    cv::KalmanFilter kf(stateSize, measSize, contrSize, type);

    cv::Mat state(stateSize, 1, type);  // [x,y,pre_x,pre_y]
    cv::Mat meas(measSize, 1, type);    // [x,y]
    //cv::Mat procNoise(stateSize, 1, type)
    // [E_x,E_y,pre_E_x,pre_E_y]

    // Transition State Matrix A
    // Note: set dT at each processing step!
    // [ 1 0 dT 0  0 0 ]
    // [ 0 1 0  dT 0 0 ]
    // [ 0 0 1  0  0 0 ]
    // [ 0 0 0  1  0 0 ]
    // [ 0 0 0  0  1 0 ]
    // [ 0 0 0  0  0 1 ]
    // >>>> Matrix A
    //cv::setIdentity(kf.transitionMatrix);
    kf.transitionMatrix = cv::Mat::zeros(6, 6, type);

    kf.transitionMatrix.at<float>(0) = 2.0;
    kf.transitionMatrix.at<float>(2) = -1.0;
    kf.transitionMatrix.at<float>(7) = 2.0;
    kf.transitionMatrix.at<float>(9) = -1.0;
    kf.transitionMatrix.at<float>(12) = 1.0;
    kf.transitionMatrix.at<float>(19) = 1.0;
    kf.transitionMatrix.at<float>(28) = 1.0;
    kf.transitionMatrix.at<float>(35) = 1.0;

    // Measure Matrix H
    // [ 1 0 0 0 0 0 ]
    // [ 0 1 0 0 0 0 ]
    // [ 0 0 0 0 1 0 ]
    // [ 0 0 0 0 0 1 ]
    kf.measurementMatrix = cv::Mat::zeros(measSize, stateSize, type);
    kf.measurementMatrix.at<float>(0) = 1.0f;
    kf.measurementMatrix.at<float>(7) = 1.0f;
    kf.measurementMatrix.at<float>(16) = 1.0f;
    kf.measurementMatrix.at<float>(23) = 1.0f;

    // Process Noise Covariance Matrix Q
    // [ Ex   0   0     0     0    0  ]
    // [ 0    Ey  0     0     0    0  ]
    // [ 0    0   Ev_x  0     0    0  ]
    // [ 0    0   0     Ev_y  0    0  ]
    // [ 0    0   0     0     Ew   0  ]
    // [ 0    0   0     0     0    Eh ]
    cv::setIdentity(kf.processNoiseCov, cv::Scalar(1e-2));

    //Measures Noise Covariance Matrix R
    cv::setIdentity(kf.measurementNoiseCov, cv::Scalar(1e-1));
    // <<<< Kalman Filter

    // Camera Index
    //    int idx = 0;

    // Camera Capture
    //    cv::VideoCapture cap;
    //    cap.set(CV_CAP_PROP_FRAME_WIDTH, 200);
    //    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 320);

    // >>>>> Camera Settings
    //    if (!cap.open(idx))
    //    {
    //        cout << "Webcam not connected.\n" << "Please verify\n";
    //        return EXIT_FAILURE;
    //    }

    ros::init(argc, argv, "object_detect");
    ros::NodeHandle handle1;
    image_transport::ImageTransport img_trans(handle1);
    bool flag_ptcloud;
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr ptcloud(new pcl::PointCloud<pcl::PointXYZRGB>());

    ros::Publisher pub = handle1.advertise<object_detection::image_data>("/object_points", 10);
    object_detection::image_data data;

    namedWindow( "Histogram", 0 );
    namedWindow( "CamShift Demo", 0 );
    image_transport::Subscriber sub_img1 = img_trans.subscribe("/camera/rgb/image_raw",1,imageCallback);
    sleep(1);
    ros::spinOnce();

    namedWindow( "Histogram", 0 );
    namedWindow( "CamShift Demo", 0 );
    namedWindow("Control", CV_WINDOW_AUTOSIZE);

    setMouseCallback( "CamShift Demo", onMouse, 0 );
    //createTrackbar("LowH", "Control", &iLowH, 179);
    //createTrackbar("HighH", "Control", &iHighH,179);
    //createTrackbar("LowS", "Control", &iLowS, 255);
    //createTrackbar("HighS", "Control", &iHighS,255);
    //createTrackbar("LowV", "Control", &iLowV, 255);
    //createTrackbar("HighV", "Control", &iHighV,255);
    createTrackbar("Vmin", "Control", &vmin,255);
    createTrackbar("Vmax", "Control", &vmax, 255);
    createTrackbar("Smin", "Control", &smin,255);

    while(image.empty())
    {
        ros::spinOnce();
        cout << "empty image" << endl;
    }

    ros::Subscriber sub = handle1.subscribe<sensor_msgs::PointCloud2>("/camera/depth/points", 1, boost::bind(ptCloudcallBack, _1, boost::ref(ptcloud),boost::ref(flag_ptcloud)));
    sleep(1);
    ros::spinOnce();

    //file for writing image features
    ofstream f1, f2;
    f1.open("/home/mithun/ur5_visual_servoing/src/object_detection/src/results/camshiftfeatures.txt",std::ios_base::trunc);
    f2.open("/home/mithun/ur5_visual_servoing/src/object_detection/src/results/kalmanfeatures.txt",std::ios_base::trunc);

    Mat frame, hsv, hue, mask, mask2, hist, histimg = Mat::zeros(200, 320, CV_8UC3), backproj;
    bool paused = false;

    int notFoundCount = 0;
    double ticks = 0;
    bool found = false;
    double area;
    double processedImages = 0;
    bool startProcessingImage = false;
    double time, total_time = 10.0;

    //ros::Rate rate(10);

    while(ros::ok())
    {
        pcl::PointXYZRGB pt;
        double precTick = ticks;
        ticks = (double) cv::getTickCount();
        double dT = (ticks - precTick) / cv::getTickFrequency(); //seconds

        ros::spinOnce();

        if( !paused )
        {
            // image = frame;
            if( image.empty() )
                break;
        }

        //        frame.copyTo( image );
        if(found)
        {
            // >>>> Matrix Q

            // >> Occlusion Ratio (alpha)

            double alphaX=0.0,alphaY=0.0;
            cout << "ilasx and prex " << iLastX << " " << Pre_iLastX << endl;
            cout << "ilasy and prey " << iLastY << " " << Pre_iLastY << endl;

            double alpha_ratioX = abs((iLastX/Pre_iLastX)-1.0);
            double alpha_ratioY = abs((iLastY/Pre_iLastY)-1.0);

            cout << "alpha " << alpha_ratioX << " " << alpha_ratioY << endl;

            if ((alpha_ratioX<=1.0) && (alpha_ratioY<=1.0))
            {
                alphaX = alpha_ratioX;
                alphaY = alpha_ratioY;
            }
            else if ((alpha_ratioX>1.0) && (alpha_ratioY>1.0))
            {
                alphaX = 1.0;
                alphaY = 1.0;
            }
            else
            {
                cout<<"unexpected alpha"<<endl;
                alphaX = alphaY = 0.0;
            }

            kf.processNoiseCov.at<float>(0) = 1.0 - alphaX;
            kf.processNoiseCov.at<float>(7) = 1.0 - alphaY;
            kf.processNoiseCov.at<float>(14) = 1.0 - alphaX;
            kf.processNoiseCov.at<float>(21) = 1.0 - alphaY;
            kf.processNoiseCov.at<float>(28) = 1.0;
            kf.processNoiseCov.at<float>(35) = 1.0;

            state = kf.predict();
            cout << "State predicted:" << endl << state << endl;
            //cout << "State pre:" << endl << kf.statePre << endl;
            //cout << "State corrected:" << endl << kf.statePost << endl;

            cv::Rect predRect;
            predRect.width = state.at<float>(4);
            predRect.height = state.at<float>(5);
            predRect.x =state.at<float>(0) - predRect.width/ 2;
            predRect.y =state.at<float>(1) - predRect.height / 2;

            cv::Point center;
            center.x = state.at<float>(0);
            center.y = state.at<float>(1);
            cv::circle(image, center, 2, CV_RGB(0,255,0), -1);
            cv::rectangle(image, predRect, CV_RGB(0,255,0), 2);

        }

        if( !paused )
        {
            cvtColor(image, hsv, COLOR_BGR2HSV);

            if( trackObject )
            {
                int _vmin = vmin, _vmax = vmax;
                //inRange(hsv, Scalar(iLowH, iLowS, iLowV), Scalar(iHighH, iHighS, iHighV), mask);
                inRange(hsv, Scalar(0, smin, MIN(_vmin,_vmax)),
                        Scalar(180, 256, MAX(_vmin, _vmax)), mask);
                cv::imshow("Mask", mask);

                mask.copyTo(mask2);
                cv::imshow("Mask", mask);
                cv::waitKey(20);
                cv::findContours(mask2, contours, CV_RETR_EXTERNAL,
                                 CV_CHAIN_APPROX_NONE);
                cout << "Countour size "<< contours.size() <<"\n";
                cv::imshow("Mask2", mask);
                cv::waitKey(20);

                int ch[] = {0, 0};
                hue.create(hsv.size(), hsv.depth());
                mixChannels(&hsv, 1, &hue, 1, ch, 1);

                if( trackObject < 0 )
                {
                    // Object has been selected by user, set up CAMShift search properties once
                    Mat roi(hue, selection), maskroi(mask, selection);
                    calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);
                    normalize(hist, hist, 0, 255, NORM_MINMAX);

                    trackWindow = selection;
                    trackObject = 1; // Don't set up again, unless user selects new ROI
                    startProcessingImage = true;
                    time = ros::Time::now().toSec();

                    histimg = Scalar::all(0);
                    int binW = histimg.cols / hsize;
                    Mat buf(1, hsize, CV_8UC3);
                    for( int i = 0; i < hsize; i++ )
                        buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180./hsize), 255, 255);
                    cvtColor(buf, buf, COLOR_HSV2BGR);

                    for( int i = 0; i < hsize; i++ )
                    {
                        int val = saturate_cast<int>(hist.at<float>(i)*histimg.rows/255);
                        rectangle( histimg, Point(i*binW,histimg.rows),
                                   Point((i+1)*binW,histimg.rows - val),
                                   Scalar(buf.at<Vec3b>(i)), -1, 8 );
                    }
                }

                if(contours.size()!=0)
                {

                    // Perform CAMShift
                    calcBackProject(&hue, 1, 0, hist, backproj, &phranges);
                    backproj &= mask;

                    RotatedRect trackBox = CamShift(backproj, trackWindow,
                                                    TermCriteria( TermCriteria::EPS | TermCriteria::COUNT, 10, 1 ));

                    cout << "Object centre from cam shift " << trackBox.center.x << " " << trackBox.center.y << endl;
                    iLastX = trackBox.center.x;
                    iLastY = trackBox.center.y;
                    MyFilledCircle( image, Point(iLastX, iLastY),0,0,255 );

                    if( trackWindow.area() <= 1 )
                    {
                        int cols = backproj.cols, rows = backproj.rows, r = (MIN(cols, rows) + 5)/6;
                        trackWindow = Rect(trackWindow.x - r, trackWindow.y - r,
                                           trackWindow.x + r, trackWindow.y + r) &
                                Rect(0, 0, cols, rows);
                        ROS_INFO("out of camera scope");
                    }

                    if( backprojMode )
                        cvtColor( backproj, image, COLOR_GRAY2BGR );
                    ellipse( image, trackBox, Scalar(0,0,255), 3, CV_AA );
                }
            }
        }
        else if( trackObject < 0 )
            paused = false;

        if( selectObject && selection.width > 0 && selection.height > 0 )
        {
            Mat roi(image, selection);
            bitwise_not(roi, roi);
        }

        // >>>>> Kalman Update
        if (contours.size() == 0)
        {
            notFoundCount++;
            cout << "notFoundCount:" << notFoundCount << endl;
            if( notFoundCount >= 1000 )
            {
                found = false;
            }
            cout << "out of range" << endl;
            //                else
            //                kf.statePost = state;
        }
        else
        {
            notFoundCount = 0;

            meas.at<float>(0) = iLastX;
            meas.at<float>(1) = iLastY;
            //meas.at<float>(2) = Pre_iLastX;
            //meas.at<float>(3) = Pre_iLastY;
            meas.at<float>(2) = selection.width;
            meas.at<float>(3) = selection.height;
            //                meas.at<float>(0) = bBox.x + bBox.width/2;
            //                meas.at<float>(1) = bBox.y + bBox.height/2;
            //                meas.at<float>(2) = bBox.width;
            //                meas.at<float>(3) = bBox.height;


            if (!found) // First detection!
            {
                // >>>> Initialization
                kf.errorCovPre.at<float>(0) = 1; // px
                kf.errorCovPre.at<float>(5) = 1; // px
                kf.errorCovPre.at<float>(10) = 1;
                kf.errorCovPre.at<float>(15) = 1;
                kf.errorCovPre.at<float>(28) = 1; // px
                kf.errorCovPre.at<float>(35) = 1; // px

                state.at<float>(0) = meas.at<float>(0);
                state.at<float>(1) = meas.at<float>(1);
                state.at<float>(2) = Pre_iLastX;
                state.at<float>(3) = Pre_iLastY;
                Pre_iLastX = meas.at<float>(0);
                Pre_iLastY = meas.at<float>(1);
                state.at<float>(4) = meas.at<float>(2);
                state.at<float>(5) = meas.at<float>(3);

                // <<<< Initialization

                kf.statePost = state;
                found = true;
            }
            else
                kf.correct(meas); // Kalman Correction

            cout << "Measure matrix:" << endl << meas << endl;
        }
        imshow( "CamShift Demo", image );
        imshow( "Histogram", histimg );
        if (contours.size()==0 && trackWindow.area()<=1)
        {
            xc = state.at<float>(0);
            yc = state.at<float>(1);
            double m[2];
            m[0] = (xc-320)/(531.15/640);    //(u-u0)/px
            m[1] = (yc-240)/(531.15/480);    //(v-v0)/py50
            if (trackObject)
                f2 << m[0] << "\t" << m[1] << endl;
        }
        else
        {
            xc = iLastX;
            yc = iLastY;
            double m[2];
            m[0] = (xc-320)/(531.15/640);    //(u-u0)/px
            m[1] = (yc-240)/(531.15/480);    //(v-v0)/py50
            if (trackObject)
                f1 << m[0] << "\t" << m[1] << endl;
        }

        char c = (char)waitKey(10);
        if( c == 27 )
            break;
        switch(c)
        {
        case 'b':
            backprojMode = !backprojMode;
            break;
        case 'c':
            trackObject = 0;
            histimg = Scalar::all(0);
            break;
        case 'h':
            showHist = !showHist;
            if( !showHist )
                destroyWindow( "Histogram" );
            else
                namedWindow( "Histogram", 1 );
            break;
        case 'p':
            paused = !paused;
            break;
        default:
            ;
        }

        if((xc< 0 || xc > 640 )|| (yc <0 || yc >480))
        {
            cout << "out of range" << endl;
            pt.z = 0.7;
        }
        else
            //                    pt = ptcloud ->at(predict_x, predict_y);
            pt = ptcloud ->at(xc, yc);

        //ROS_INFO("%d , %d ", iLastX, iLastY);
        //ROS_INFO("%d , %d ", ptcloud->width, ptcloud->height);
        double depth = double(pt.z);
        //Centroid in image coordinates
        double m[2];
        m[0] = (xc-320)/(531.15/640);    //(u-u0)/px
        m[1] = (yc-240)/(531.15/480);    //(v-v0)/py50

        float new_depth;
        new_depth = float(depth);
        cout << "depth = " << new_depth<<endl;
        cout << "centroid (pixel) " << xc << " " << yc << endl;
        cout << "centroid (image) " << m[0] << " " << m[1] << endl;

        //writing to a msg
        data.detected_point_x.data = xc;
        data.detected_point_y.data = yc;
        data.detected_point_depth.data =(float)pt.z;

        //cout <<"Publishing Image data"<<endl;
        if (startProcessingImage)
        {
            double duration = ros::Time::now().toSec() -time ;
            cout << "Duration: " << duration << endl;
            processedImages++;
            //cout << "Processed Images: " << processedImages << endl;

            if(duration>total_time)
            {
                //cout << "Processed Images in "<< total_time << " sec = " << processedImages << endl;
                //cout << "Processed Images in 1.0 sec = " << processedImages/total_time << endl;
                //break;
            }
        }
        pub.publish(data);
        cout <<"Published Image data"<<endl;
        ros::spinOnce();
        //rate.sleep();
    }
    return 0;
}
