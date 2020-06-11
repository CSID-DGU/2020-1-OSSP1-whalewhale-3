#include <iostream>
#include <fstream>
#include "opencv2/core.hpp"
#include "opencv2/core/utility.hpp"
#include "opencv2/core/ocl.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/xfeatures2d.hpp"

using namespace cv;
using namespace cv::xfeatures2d;
using namespace std;

const int LOOP_NUM = 10;
const int GOOD_PTS_MAX = 30;
const float GOOD_PORTION = 0.15f;
const int NUM_IMG = 36;
int64 work_begin = 0;
int64 work_end = 0;

static void workBegin()
{
	work_begin = getTickCount();
}

static void workEnd()
{
	work_end = getTickCount() - work_begin;
}

static double getTime()
{
	return work_end / ((double)getTickFrequency()) * 1000.;
}

//사각형 면적 구하기
static double getArea(double x1, double x2, double x3, double x4, 
					double y1, double y2, double y3, double y4) 
{
	long long sum = 0;
	double ans;

	vector<int> X, Y;
	for (int i = 0; i < 4; i++) {
		X.push_back(x1);
		X.push_back(x2);
		X.push_back(x3);
		X.push_back(x4);
		Y.push_back(y1);
		Y.push_back(y2);
		Y.push_back(y3);
		Y.push_back(y4);

	}
	X.push_back(X.front());
	Y.push_back(Y.front());
	long long yy = 0;
	for (int i = 1; i < 4; i++) {
		yy += Y[i] - Y[i - 1];
		sum += (X[i + 1] - X[i - 1]) * yy;
	}
	ans = sum / (double)2;
	return abs(ans);
}
class Painting{
public:
	string name;
	string title;
	Mat image;
	vector<Point> points;
};

struct SURFDetector
{
	Ptr<Feature2D> surf;
	SURFDetector(double hessian = 800.0)
	{
		surf = SURF::create(hessian); //800.0 = Hessian matrix의 determinant를 거르는 기준값
	}
	template<class T>
	void operator()(const T& in, const T& mask, std::vector<cv::KeyPoint>& pts, T& descriptors, bool useProvided = false)
	//이미지, 마스크, 특징점(keypoint), 기술자(descriptors)형성
	{
		surf->detectAndCompute(in, mask, pts, descriptors, useProvided);
	}//features2d의 detectAndCompute
};

template<class KPMatcher>
struct SURFMatcher
{
	KPMatcher matcher;
	template<class T>
	void match(const T& in1, const T& in2, std::vector<cv::DMatch>& matches)
	{
		matcher.match(in1, in2, matches);
	}//이미지1,이미지2의 match(DMatch클래스 = 여러 영상에서 추출한 특징점사이의 매칭 정보를 표시)
};

//Mat:행렬(Matrix)구조체, 이미지를 Mat형태로 변환하여 읽어들인다.
static Mat drawGoodMatches(
	const Mat& img1,
	const Mat& img2,
	const std::vector<KeyPoint>& keypoints1,
	const std::vector<KeyPoint>& keypoints2,
	std::vector<DMatch>& matches,
	std::vector<Point2f>& scene_corners_,
	std::vector<Point2f>& area_corners_
	//Point클래스: 2d좌표를 표현하는 템플릿 클래스(멤버변수 x,y)
)
{
	//-- Sort matches and preserve top 10% matches
	std::sort(matches.begin(), matches.end());
	//sort:matches배열을 오름차순(distance가 작은 순)으로 정렬
	std::vector< DMatch > good_matches;
	double minDist = matches.front().distance;
	double maxDist = matches.back().distance;
	//distance:descriptor간의 간격, 작을수록 좋다

	const int ptsPairs = std::min(GOOD_PTS_MAX, (int)(matches.size() * GOOD_PORTION));
	//point pairs 개수를 matches size*GOOD_PORTION가 30이상이면 30으로 그 이하면 그 값으로
	for (int i = 0; i < ptsPairs; i++)
	{
		good_matches.push_back(matches[i]);
	}
	//push_back(a):마지막 원소 뒤에 a삽입
	std::cout << "\nMax distance: " << maxDist << std::endl;
	std::cout << "Min distance: " << minDist << std::endl;

	std::cout << "Calculating homography using " << ptsPairs << " point pairs." << std::endl;

	// drawing the results
	Mat img_matches;


	drawMatches(img1, keypoints1, img2, keypoints2,
		good_matches, img_matches, Scalar::all(-1), Scalar::all(-1),
		//Scalar 클래스:4개의 요소를 갖는다
		std::vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
	/*drawMatches : 특징점을 선으로 연결해주는 함수
	DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS : drawMatches()함수와 함께사용,
	매칭되지않은 특징점은 그리지 않는다.
	DEFAULT, DRAW_OVER_OUTFIT, DRAW_RICH_KEYPOINTS기능도 있음*/


	//-- Localize the object
	std::vector<Point2f> obj;
	std::vector<Point2f> scene;

	for (size_t i = 0; i < good_matches.size(); i++)
	{
		//-- Get the keypoints from the good matches
		obj.push_back(keypoints1[good_matches[i].queryIdx].pt);
		scene.push_back(keypoints2[good_matches[i].trainIdx].pt);
	}
	/*queryIdx:기준이미지의 keypoint의 index, trainIdx:비교이미지의 keypoint의 index
	.pt:*/
	//-- Get the corners from the image_1 ( the object to be "detected" )
	std::vector<Point2f> obj_corners(4);
	obj_corners[0] = Point(0, 0);
	obj_corners[1] = Point(img1.cols, 0);
	obj_corners[2] = Point(img1.cols, img1.rows);
	obj_corners[3] = Point(0, img1.rows);

	std::vector<Point2f> scene_corners(4);
	std::vector<Point2f> area_corners(4);

	area_corners[0] = Point2f((float)img1.cols, 0) + Point2f(0, 0);
	area_corners[1] = Point2f((float)img1.cols, 0) + Point2f(img2.cols, 0);
	area_corners[2] = Point2f((float)img1.cols, 0) + Point2f(img2.cols, img2.rows);
	area_corners[3] = Point2f((float)img1.cols, 0) + Point2f(0, img2.rows);

	Mat H = findHomography(obj, scene, RANSAC);
	perspectiveTransform(obj_corners, scene_corners, H);
	//scene의 corner4개에 obj의 4개의 corner를 대응  
	scene_corners_ = scene_corners;
	area_corners_ = area_corners;

	//-- Draw lines between the corners (the mapped object in the scene - image_2 )
	line(img_matches,
		scene_corners[0] + Point2f((float)img1.cols, 0), scene_corners[1] + Point2f((float)img1.cols, 0),
		Scalar(0, 255, 0), 2, LINE_AA);
	line(img_matches,
		scene_corners[1] + Point2f((float)img1.cols, 0), scene_corners[2] + Point2f((float)img1.cols, 0),
		Scalar(0, 255, 0), 2, LINE_AA);
	line(img_matches,
		scene_corners[2] + Point2f((float)img1.cols, 0), scene_corners[3] + Point2f((float)img1.cols, 0),
		Scalar(0, 255, 0), 2, LINE_AA);
	line(img_matches,
		scene_corners[3] + Point2f((float)img1.cols, 0), scene_corners[0] + Point2f((float)img1.cols, 0),
		Scalar(0, 255, 0), 2, LINE_AA);

	return img_matches;
}

////////////////////////////////////////////////////
// This program demonstrates the usage of SURF_OCL.
// use cpu findHomography interface to calculate the transformation matrix
int main(int argc, char* argv[])
{
	//img2 : 찍은 사진, img1 : 비교할 그림들, img3 : img1중 선택된 이미지
	UMat img1, img2, img3;
	
	std::string outpath = "output.jpg";
	/*std::string leftName = "object.jpg";
	imread(leftName, IMREAD_COLOR).copyTo(img1);
	if (img1.empty())
	{
		std::cout << "Couldn't load " << leftName << std::endl;
		return EXIT_FAILURE;
	}*/

	std::string rightName = "scene.jpg";
	imread(rightName, IMREAD_COLOR).copyTo(img2);
	if (img2.empty())
	{
		std::cout << "Couldn't load " << rightName << std::endl;
		return EXIT_FAILURE;
	}

	double surf_time = 0.;
	string str_buf;

	std::vector<Painting> paintings;
	Painting p;
	std::fstream fs;
	fs.open("GOGH Vincent van.csv", ios::in);

	/*csv(테스트라 작품명만)내용을 1,2...jpg에 순서대로 p.title에 저장
	p.image에는 이미지가, name에는 파일명(xx.jpg)이 저장*/
	for (int i = 0; i < NUM_IMG; i++) {
		getline(fs, str_buf, ',');
		string painting_name = to_string(i+1) + ".jpg";	
		p.image = imread("img/" + painting_name, IMREAD_COLOR);
		p.title = str_buf;
		p.name = painting_name;
		paintings.push_back(p);
	}
	//declare input/output
	std::vector<KeyPoint> keypoints1, keypoints2;
	std::vector<DMatch> matches;

	UMat _descriptors1, _descriptors2;
	Mat descriptors1 = _descriptors1.getMat(ACCESS_RW),
		descriptors2 = _descriptors2.getMat(ACCESS_RW);

	//instantiate detectors/matchers
	SURFDetector surf;
	SURFMatcher<BFMatcher> matcher;
	std::vector<Point2f> corner;
	std::vector<Point2f> area_corner;
	Mat img_matches;
	vector<double> area;

	size_t mi = 0;
	int index;
	float area_;
	float max;
	float x_min, x_max, y_min, y_max;
	
	workBegin();

	/*k max(여기서 10)은 비교할 그림 개수
	(테스트라 10개만, 나중에 파일 속 이미지 수 나타내는 함수로 수정)
	사각형 면적(area)이 제일 큰 이미지를 일치하는 그림이라 보고 그 그림이 저장된 index로 출력*/
	for (int k = 0; k < NUM_IMG; k++) {
		paintings[k].image.copyTo(img1);

		surf(img1.getMat(ACCESS_READ), Mat(), keypoints1, descriptors1);
		surf(img2.getMat(ACCESS_READ), Mat(), keypoints2, descriptors2);
		matcher.match(descriptors1, descriptors2, matches);
		img_matches = drawGoodMatches(img1.getMat(ACCESS_READ), img2.getMat(ACCESS_READ), keypoints1, keypoints2, matches, corner, area_corner);

		std::cout << corner[0] + Point2f((float)img1.cols, 0) << std::endl;
		std::cout << corner[1] + Point2f((float)img1.cols, 0) << std::endl;
		std::cout << corner[2] + Point2f((float)img1.cols, 0) << std::endl;
		std::cout << corner[3] + Point2f((float)img1.cols, 0) << std::endl;

		x_min = area_corner[0].x;
		x_max = area_corner[1].x;
		y_min = area_corner[0].y;
		y_max = area_corner[2].y;

		if (x_min < corner[0].x + Point2f((float)img1.cols, 0).x &&
			x_max > corner[0].x + Point2f((float)img1.cols, 0).x &&
			y_min < corner[0].y + Point2f((float)img1.cols, 0).y &&
			y_max > corner[0].y + Point2f((float)img1.cols, 0).y &&
			x_min < corner[1].x + Point2f((float)img1.cols, 0).x &&
			x_max > corner[1].x + Point2f((float)img1.cols, 0).x &&
			y_min < corner[1].y + Point2f((float)img1.cols, 0).y &&
			y_max > corner[1].y + Point2f((float)img1.cols, 0).y &&
			x_min < corner[2].x + Point2f((float)img1.cols, 0).x &&
			x_max > corner[2].x + Point2f((float)img1.cols, 0).x &&
			y_min < corner[0].y + Point2f((float)img1.cols, 0).y &&
			y_max > corner[2].y + Point2f((float)img1.cols, 0).y &&
			x_min < corner[3].x + Point2f((float)img1.cols, 0).x &&
			x_max > corner[3].x + Point2f((float)img1.cols, 0).x &&
			y_min < corner[3].y + Point2f((float)img1.cols, 0).y &&
			y_max > corner[3].y + Point2f((float)img1.cols, 0).y){
			area_ = getArea(corner[0].x, corner[1].x, corner[2].x, corner[3].x,
				corner[0].y, corner[1].y, corner[2].y, corner[3].y);
		}
		else area_ = -2.5;

		std::cout << "Area is = " << area_ << std::endl;
		area.push_back(area_);
		printf("%d", k);
	}
	//area을 0번부터 비교해서 index찾기
	max = area[0];
	
	for (int i = 0; i < NUM_IMG; i++) {
		if (area[i] > max) {
			max = area[i];
		}
	}
	for (int i = 0; i < NUM_IMG; i++) {
		if (area[i] == max) {
			index = i;
			break;
		}
	}
	printf("index is : %d\n", index);

	//결정된 painting[index]의 이미지를 img3에 넣고 결과 출력
	paintings[index].image.copyTo(img3);
	
	
	surf(img3.getMat(ACCESS_READ), Mat(), keypoints1, descriptors1);
	surf(img2.getMat(ACCESS_READ), Mat(), keypoints2, descriptors2);
	matcher.match(descriptors1, descriptors2, matches);

	workEnd();
	std::cout << "FOUND " << keypoints1.size() << " keypoints on first image" << std::endl;
	std::cout << "FOUND " << keypoints2.size() << " keypoints on second image" << std::endl;

	surf_time = getTime();
	std::cout << "SURF run time: " << surf_time / LOOP_NUM << " ms" << std::endl << "\n";

	img_matches = drawGoodMatches(img3.getMat(ACCESS_READ), img2.getMat(ACCESS_READ), keypoints1, keypoints2, matches, corner, area_corner);

	//-- Show detected matches
	namedWindow("surf matches", 0);
	imshow("surf matches", img_matches);
	imwrite(outpath, img_matches);

	
	line(img2, corner[0], corner[1], Scalar(255, 0, 0), 2, LINE_AA);
	line(img2, corner[1], corner[2], Scalar(255, 0, 0), 2, LINE_AA);
	line(img2, corner[2], corner[3], Scalar(255, 0, 0), 2, LINE_AA);
	line(img2, corner[3], corner[0], Scalar(255, 0, 0), 2, LINE_AA);
	imshow("draw square", img2);
	
	waitKey(0);
	return EXIT_SUCCESS;
}