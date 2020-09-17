#include <opencv2/core.hpp>
#include <opencv2/video.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>

#include <chrono>
#include <vector>

#include "FfmpegVideoCapture.h"

//#define USE_CV_VIDEO_CAPTURE

using int32 = int32_t;
using uint32 = uint32_t;

int main()
{
	class TimerMilli
	{
	public:
		const uint32	tick() const { return static_cast<uint32>(std::chrono::duration_cast<std::chrono::milliseconds>(_steadyClock.now().time_since_epoch()).count()); }
		void			profileBegin() const { _profilingStartTimePoint = tick(); _profiledDuration = 0; }
		void			profileEnd() const { _profiledDuration = tick() - _profilingStartTimePoint; }
		const uint32	profiledDuration() const { return _profiledDuration; }
	private:
		std::chrono::steady_clock	_steadyClock;
		mutable uint32				_profilingStartTimePoint;
		mutable uint32				_profiledDuration;
	};

	static constexpr int32 kKeyEscape = 27;
	const cv::String kWindowName{ "OpenCV Camera" };
	
	cv::namedWindow(kWindowName, cv::WINDOW_AUTOSIZE);

#if defined USE_CV_VIDEO_CAPTURE
	cv::VideoCapture videoCapture;
	//videoCapture.open(0, cv::CAP_DSHOW);
	videoCapture.open(0, cv::CAP_FFMPEG);
	//videoCapture.open(0, cv::CAP_V4L2); // Video For Linux 2
	if (false == videoCapture.isOpened())
	{
		printf("VideoCapture failed to open.");
		return 0;
	}

#pragma region Change resolution
	const int kFourCcArray[]
	{
		cv::VideoWriter::fourcc('H', '2', '6', '4'),
		cv::VideoWriter::fourcc('H', 'E', 'V', 'C'),
		cv::VideoWriter::fourcc('M', 'P', 'E', 'G'),
		cv::VideoWriter::fourcc('X', 'V', 'I', 'D'),
		cv::VideoWriter::fourcc('M', 'J', 'P', 'G')
	};
	const int kFourCcArrayCount = _countof(kFourCcArray);

	for (int fourCcIndex = 0; fourCcIndex < kFourCcArrayCount; fourCcIndex++)
	{
		const bool isFourCharacterCodeSet = videoCapture.set(cv::CAP_PROP_FOURCC, kFourCcArray[fourCcIndex]);
		if (true == isFourCharacterCodeSet)
		{
			printf("FourCc %d success!", kFourCcArray[fourCcIndex]);

			videoCapture.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
			videoCapture.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
			break;
		}
	}
#pragma endregion
#else
	FfmpegVideoCapture ffmpegVideoCapture;
	if (false == ffmpegVideoCapture.openVideo())
	{
		printf("FfmpegVideoCapture failed to open.");
		return 0;
	}
#endif

	TimerMilli timer;
	uint32 fps{};
	uint32 frameCount{};
	uint32 prevTimePoint = timer.tick();
	cv::Mat videoFrame = cv::Mat::zeros(ffmpegVideoCapture.frameHeight(), ffmpegVideoCapture.frameWidth(), CV_8UC3);
	while (true)
	{
		const uint32 atStart = timer.tick();

		{
			timer.profileBegin();

#if defined USE_CV_VIDEO_CAPTURE
			videoCapture >> videoFrame;
#else

#endif
			if (true == ffmpegVideoCapture.updateVideo())
			{
				memcpy(videoFrame.data, ffmpegVideoCapture.frameData(), ffmpegVideoCapture.frameDataSize());
			}

			timer.profileEnd();
		}

		const uint32 now = timer.tick();
		if (1000 <= now - prevTimePoint)
		{
			fps = frameCount;
			frameCount = 0;

			prevTimePoint = now;
		}

		if (false == videoFrame.empty())
		{
			cv::putText(videoFrame, cv::format("FPS: %d  Frame: %d ms", fps, now - atStart), cv::Point(10, 10), 1, 1.0, cv::Scalar(255, 0, 0));

			cv::imshow(kWindowName, videoFrame);

			++frameCount;
		}

		const int key = cv::waitKey(1);
		if (key == kKeyEscape)
		{
			break;
		}
	}
	
	cv::destroyWindow(kWindowName);

	return 0;
}
