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

//�簢�� ���� ���ϱ�
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
		surf = SURF::create(hessian); //800.0 = Hessian matrix�� determinant�� �Ÿ��� ���ذ�
	}
	template<class T>
	void operator()(const T& in, const T& mask, std::vector<cv::KeyPoint>& pts, T& descriptors, bool useProvided = false)
	//�̹���, ����ũ, Ư¡��(keypoint), �����(descriptors)����
	{
		surf->detectAndCompute(in, mask, pts, descriptors, useProvided);
	}//features2d�� detectAndCompute
};

template<class KPMatcher>
struct SURFMatcher
{
	KPMatcher matcher;
	template<class T>
	void match(const T& in1, const T& in2, std::vector<cv::DMatch>& matches)
	{
		matcher.match(in1, in2, matches);
	}//�̹���1,�̹���2�� match(DMatchŬ���� = ���� ���󿡼� ������ Ư¡�������� ��Ī ������ ǥ��)
};

//Mat:���(Matrix)����ü, �̹����� Mat���·� ��ȯ�Ͽ� �о���δ�.
static Mat drawGoodMatches(
	const Mat& img1,
	const Mat& img2,
	const std::vector<KeyPoint>& keypoints1,
	const std::vector<KeyPoint>& keypoints2,
	std::vector<DMatch>& matches,
	std::vector<Point2f>& scene_corners_,
	std::vector<Point2f>& area_corners_
	//PointŬ����: 2d��ǥ�� ǥ���ϴ� ���ø� Ŭ����(������� x,y)
)
{
	//-- Sort matches and preserve top 10% matches
	std::sort(matches.begin(), matches.end());
	//sort:matches�迭�� ��������(distance�� ���� ��)���� ����
	std::vector< DMatch > good_matches;
	double minDist = matches.front().distance;
	double maxDist = matches.back().distance;
	//distance:descriptor���� ����, �������� ����

	const int ptsPairs = std::min(GOOD_PTS_MAX, (int)(matches.size() * GOOD_PORTION));
	//point pairs ������ matches size*GOOD_PORTION�� 30�̻��̸� 30���� �� ���ϸ� �� ������
	for (int i = 0; i < ptsPairs; i++)
	{
		good_matches.push_back(matches[i]);
	}
	//push_back(a):������ ���� �ڿ� a����
	std::cout << "\nMax distance: " << maxDist << std::endl;
	std::cout << "Min distance: " << minDist << std::endl;

	std::cout << "Calculating homography using " << ptsPairs << " point pairs." << std::endl;

	// drawing the results
	Mat img_matches;


	drawMatches(img1, keypoints1, img2, keypoints2,
		good_matches, img_matches, Scalar::all(-1), Scalar::all(-1),
		//Scalar Ŭ����:4���� ��Ҹ� ���´�
		std::vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
	/*drawMatches : Ư¡���� ������ �������ִ� �Լ�
	DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS : drawMatches()�Լ��� �Բ����,
	��Ī�������� Ư¡���� �׸��� �ʴ´�.
	DEFAULT, DRAW_OVER_OUTFIT, DRAW_RICH_KEYPOINTS��ɵ� ����*/


	//-- Localize the object
	std::vector<Point2f> obj;
	std::vector<Point2f> scene;

	for (size_t i = 0; i < good_matches.size(); i++)
	{
		//-- Get the keypoints from the good matches
		obj.push_back(keypoints1[good_matches[i].queryIdx].pt);
		scene.push_back(keypoints2[good_matches[i].trainIdx].pt);
	}
	/*queryIdx:�����̹����� keypoint�� index, trainIdx:���̹����� keypoint�� index
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
	//scene�� corner4���� obj�� 4���� corner�� ����  
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
	//img2 : ���� ����, img1 : ���� �׸���, img3 : img1�� ���õ� �̹���
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

	/*csv(�׽�Ʈ�� ��ǰ��)������ 1,2...jpg�� ������� p.title�� ����
	p.image���� �̹�����, name���� ���ϸ�(xx.jpg)�� ����*/
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

	/*k max(���⼭ 10)�� ���� �׸� ����
	(�׽�Ʈ�� 10����, ���߿� ���� �� �̹��� �� ��Ÿ���� �Լ��� ����)
	�簢�� ����(area)�� ���� ū �̹����� ��ġ�ϴ� �׸��̶� ���� �� �׸��� ����� index�� ���*/
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
	//area�� 0������ ���ؼ� indexã��
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

	//������ painting[index]�� �̹����� img3�� �ְ� ��� ���
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