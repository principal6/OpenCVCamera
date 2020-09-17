#pragma once

#include <cstdint>


using uint8			= uint8_t;
using PixelElemenet = uint8;
using uint32		= uint32_t;
using int32			= int32_t;

struct AVFormatContext;
struct AVStream;
struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
enum AVPixelFormat;

class FfmpegVideoCapture
{
public:
							FfmpegVideoCapture();
							~FfmpegVideoCapture();

private:
	void					initialize();

public:
	bool					openVideo();
	void					closeVideo();
	bool					updateVideo();

public:
	const int32				frameWidth() const noexcept;
	const int32				frameHeight() const noexcept;
	const PixelElemenet*	frameData() const noexcept;
	const int32				frameDataSize() const noexcept;

private:
	AVFormatContext*		avFormatContext;
	AVStream*				videoStream;
	AVCodec*				videoDecoder;
	AVCodecContext*			videoDecoderContext;
	int32					outFrameByteCount;
	PixelElemenet*			outFrameRawBuffer;
	AVFrame*				outFrame;
	AVFrame*				inFrame;
	int32					inFrameWidth;
	int32					inFrameHeight;
	AVPixelFormat			inFramePixelFormat;
	SwsContext*				swsContext;
};