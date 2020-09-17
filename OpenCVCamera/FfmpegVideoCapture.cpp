#include "FfmpegVideoCapture.h"

#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}


FfmpegVideoCapture::FfmpegVideoCapture()
	: avFormatContext{}
	, videoStream{}
	, videoDecoder{}
	, videoDecoderContext{}
	, outFrameByteCount{}
	, outFrameRawBuffer{}
	, outFrame{}
	, inFrame{}
	, inFrameWidth{}
	, inFrameHeight{}
	, inFramePixelFormat{ AVPixelFormat::AV_PIX_FMT_NONE }
	, swsContext{}
{
	initialize();
}

FfmpegVideoCapture::~FfmpegVideoCapture()
{
	closeVideo();
}

void FfmpegVideoCapture::initialize()
{
	avdevice_register_all();

	std::vector<AVInputFormat*> avInputFormatArray{};
	{
		AVInputFormat* avInputFormat{};
		while (true)
		{
			avInputFormat = av_input_video_device_next(avInputFormat);
			if (nullptr == avInputFormat)
			{
				break;
			}
			avInputFormatArray.push_back(avInputFormat);
		}
	}
}

bool FfmpegVideoCapture::openVideo()
{
	closeVideo();

	avFormatContext = avformat_alloc_context();

	// Webcam 열기
	AVInputFormat* const avInputFormatDshow = av_find_input_format("dshow");
	AVDictionary* avDictionary{};
	av_dict_set(&avDictionary, "input_format", "h264", 0);
	av_dict_set(&avDictionary, "video_size", "1280x720", 0);
	av_dict_set(&avDictionary, "framerate", "30", 0);
	if (0 != avformat_open_input(&avFormatContext, "video=Webcam SC-10HDD12636P", avInputFormatDshow, &avDictionary))
	{
		printf("ERROR: 카메라에 접근할 수 없습니다. 설정을 확인해보세요.\n");
		return false;
	}

	if (0 != avformat_find_stream_info(avFormatContext, nullptr))
	{
		printf("ERROR: Stream 정보가 없습니다!\n");
		return false;
	}

	const uint32 streamCount = avFormatContext->nb_streams;
	for (uint32 streamIndex = 0; streamIndex < streamCount; streamIndex++)
	{
		if (AVMediaType::AVMEDIA_TYPE_VIDEO == avFormatContext->streams[streamIndex]->codecpar->codec_type)
		{
			videoStream = avFormatContext->streams[streamIndex];
			break;
		}
	}
	if (nullptr == videoStream)
	{
		printf("ERROR: VideoStream 이 없습니다!\n");
		return false;
	}

	videoDecoder = avcodec_find_decoder(videoStream->codecpar->codec_id);
	if (nullptr == videoDecoder)
	{
		printf("ERROR: Video 의 Decoder 를 찾기 실패했습니다!\n");
		return false;
	}
	printf("INFO: Video Decoder: %s\n", videoDecoder->name);


	videoDecoderContext = avcodec_alloc_context3(videoDecoder);
	inFrameWidth = videoStream->codecpar->width;
	inFrameHeight = videoStream->codecpar->height;
	inFramePixelFormat = static_cast<AVPixelFormat>(videoStream->codecpar->format);

#if defined PROCESS_FOR_RAW_VIDEO
	if (videoDecoder->id == AVCodecID::AV_CODEC_ID_RAWVIDEO)
	{
		printf("INFO: Video 가 RawVideo 입니다!\n");
		videoDecoderContext->width = inFrameWidth;
		videoDecoderContext->height = inFrameHeight;
		videoDecoderContext->pix_fmt = static_cast<AVPixelFormat>(videoStream->codecpar->format); // AV_PIX_FMT_YUYV422
	}
#endif

	if (0 != avcodec_open2(videoDecoderContext, videoDecoder, nullptr))
	{
		printf("ERROR: Video 의 Decoder Codec Context 를 열지 못했습니다!\n");
		return false;
	}
	printf("INFO: Video Frame[%d, %d]", inFrameWidth, inFrameHeight);

	const AVPixelFormat outPixelFormat = AV_PIX_FMT_BGR24;
	outFrameByteCount = av_image_get_buffer_size(outPixelFormat, inFrameWidth, inFrameHeight, 1);
	outFrameRawBuffer = reinterpret_cast<PixelElemenet*>(av_malloc(outFrameByteCount * sizeof(PixelElemenet)));

	outFrame = av_frame_alloc();
	av_image_fill_arrays(outFrame->data, outFrame->linesize, outFrameRawBuffer, outPixelFormat, inFrameWidth, inFrameHeight, 1);

	swsContext = sws_getCachedContext(nullptr, inFrameWidth, inFrameHeight, inFramePixelFormat, inFrameWidth, inFrameHeight, outPixelFormat, SWS_BICUBIC, nullptr, nullptr, nullptr);

	inFrame = av_frame_alloc();

	return true;
}

void FfmpegVideoCapture::closeVideo()
{
	if (swsContext != nullptr)
	{
		sws_freeContext(swsContext);
		swsContext = nullptr;
	}

	if (inFrame != nullptr)
	{
		av_frame_free(&inFrame);
	}

	if (outFrame != nullptr)
	{
		av_frame_free(&outFrame);
	}

	if (outFrameRawBuffer != nullptr)
	{
		av_free(outFrameRawBuffer);
		outFrameRawBuffer = nullptr;
	}
	outFrameByteCount = 0;

	inFramePixelFormat = AVPixelFormat::AV_PIX_FMT_NONE;
	inFrameHeight = 0;
	inFrameWidth = 0;
	if (videoDecoderContext != nullptr)
	{
		avcodec_close(videoDecoderContext);
		avcodec_free_context(&videoDecoderContext);
	}

	videoDecoder = nullptr;
	videoStream = nullptr;

	if (avFormatContext != nullptr)
	{
		avformat_close_input(&avFormatContext);
	}
}

bool FfmpegVideoCapture::updateVideo()
{
	if (videoStream == nullptr || inFrame == nullptr || outFrame == nullptr)
	{
		return false;
	}

	AVPacket packet;
	if (0 == av_read_frame(avFormatContext, &packet))
	{
		if (packet.stream_index == videoStream->index)
		{
			if (0 == avcodec_send_packet(videoDecoderContext, &packet))
			{
				if (0 == avcodec_receive_frame(videoDecoderContext, inFrame))
				{
					sws_scale(swsContext, inFrame->data, inFrame->linesize, 0, inFrameHeight, outFrame->data, outFrame->linesize);
					av_packet_unref(&packet);
					return true;
				}
			}
		}
		av_packet_unref(&packet);
	}
	return false;
}

const int32 FfmpegVideoCapture::frameWidth() const noexcept
{
	return inFrameWidth;
}

const int32 FfmpegVideoCapture::frameHeight() const noexcept
{
	return inFrameHeight;
}

const PixelElemenet* FfmpegVideoCapture::frameData() const noexcept
{
	return outFrameRawBuffer;
}

const int32 FfmpegVideoCapture::frameDataSize() const noexcept
{
	return outFrameByteCount;
}
