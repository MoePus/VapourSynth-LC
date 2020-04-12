#pragma once
#include "Helper.h"
#include <iostream>
#include <vector>  
#include <algorithm> 
#include <math.h>
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned int   DWORD;

template<typename pointType>
class LC_Process
{
private:
	VSFrameContext *frameCtx = nullptr;
	VSCore *core = nullptr;
	const VSAPI *vsapi = nullptr;
	const LC_BASIC_DATA &d;

	const VSFrameRef *src = nullptr;
	VSFrameRef *dst = nullptr;
	const VSFormat *frameFormat = nullptr;
	int dst_stride[3];
	int dst_height[3];
	int dst_width[3];
	int dst_pcount[3];

	int height, width;
	int planes;

	int frame;
	constexpr static float levelRange = 1 << (sizeof(pointType) * 8);
	constexpr static float levelMiddle = static_cast<float>((static_cast<uint8_t>(~0)) >> 1);
	template < typename funcType >
	void LoopPoint(int width, int height, int stride, funcType func)
	{
		for (int y = 0; y < height; ++y)
		{
			int lp = y * stride;

			for (const int upper = lp + width; lp < upper; ++lp)
			{
				func(lp);
			}
		}
	}
	template < typename funcType >
	void LoopLine(int width, int height, int stride, funcType func)
	{
		for (int y = 0; y < height; ++y)
		{
			int lp = y * stride;
			func(lp, lp + width);
		}
	}
public:
	LC_Process(const LC_BASIC_DATA &_d, int n, VSFrameContext *_frameCtx, VSCore *_core, const VSAPI *_vsapi)
		:core(_core), frameCtx(_frameCtx), vsapi(_vsapi), d(_d) 
	{
		frame = n;
		src = vsapi->getFrameFilter(n, d.node, frameCtx);
		frameFormat = vsapi->getFrameFormat(src);

		height = vsapi->getFrameHeight(src, 0);
		width = vsapi->getFrameWidth(src, 0);
		planes = 3;

		int planeRef[3] = { 0,1,2 };
		const VSFrameRef *planes4cp[3] = { src,src,src };
		dst = vsapi->newVideoFrame2(frameFormat, width, height, planes4cp, planeRef, src, core);

		for (int i = 0; i < 3; ++i)
		{
			dst_height[i] = vsapi->getFrameHeight(dst, i);
			dst_width[i] = vsapi->getFrameWidth(dst, i);
			dst_stride[i] = vsapi->getStride(dst, i) / frameFormat->bytesPerSample;
			dst_pcount[i] = dst_height[i] * dst_stride[i];
		}
	};

	~LC_Process()
	{
		vsapi->freeFrame(src);
	}
	VSFrameRef *GetDst()
	{
		return dst;
	}

	void process()
	{
		const int* threshold = d.getThreshold();
		const float processRate = d.getProcessRate();
		for (int i = 0; i < 3; i++)
		{
			if (threshold[i] == 0) continue;
			pointType * dstPlane = reinterpret_cast<pointType *>(vsapi->getWritePtr(dst, i));
			std::vector<pointType> points;
			int pointNum = dst_width[i] * dst_height[i];
			points.reserve(pointNum);

			LoopLine(dst_width[i], dst_height[i], dst_stride[i], [&](int start,int end)
			{
				std::vector<pointType> tmp(&dstPlane[start], &dstPlane[end]);
				points.insert(points.end(), tmp.begin(), tmp.end());
			});

			if(i==0) 
			{
				std::partial_sort(points.begin(), points.begin() + static_cast<DWORD>(0.01f*pointNum), points.end());
				float minV= points.back();
				if (minV < 0.5f) continue;
				if (minV < threshold[i])
				{
					float processV = minV * std::sinf(((threshold[i] - minV) / static_cast<float>(threshold[i]) * 90) / 180.0f * 3.14159f) * processRate;
					float enhanceRate = 1 / ((levelRange - processV) / levelRange);
					LoopPoint(dst_width[i], dst_height[i], dst_stride[i], [&](int lp)
					{
						dstPlane[lp] = static_cast<pointType>(std::max<float>(0, (static_cast<float>(dstPlane[lp]) - processV)*enhanceRate));
					});
				}
			}
			else
			{
				std::partial_sort(points.begin(), points.begin() + 1, points.end(),[&](const pointType &v1, const pointType &v2)
				{
					return std::abs(v1 - levelMiddle) < std::abs(v2 - levelMiddle);
				});
				float minV = points[0] - levelMiddle;
				auto minVabs = std::fabs(minV);
				if (minVabs < 0.5f) continue;
				if (minVabs < threshold[i])
				{
					float processV = minV * std::sinf(((threshold[i] - minVabs) / static_cast<float>(threshold[i]) * 90) / 180.0f * 3.14159f) * processRate;
					float enhanceRate = 1 / ((levelRange / 2.0f - processV) / (levelRange / 2.0f));
					LoopPoint(dst_width[i], dst_height[i], dst_stride[i], [&](int lp)
					{
						dstPlane[lp] = static_cast<pointType>((static_cast<float>(dstPlane[lp]) - processV - levelMiddle)*enhanceRate + levelMiddle);
					});
				}
			}

		}
	}
};