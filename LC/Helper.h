#pragma once
#include <vapoursynth/VapourSynth.h>
#include <vapoursynth/VSHelper.h>
#include <iostream>
#include <string>
class LC_BASIC_DATA
{
public:
	VSNodeRef *node;
	const VSVideoInfo *vi = nullptr;
	const VSAPI *vsapi = nullptr;
	void setError(VSMap *out, std::string error)
	{
		vsapi->setError(out, error.c_str());
	}
	void setParams(int a[3], float b)
	{
		for (int i = 0; i < 3; i++)
			threshold[i] = a[i];
		processRate = b;
	}

	int* getThreshold() const
	{
		return const_cast<int*>(threshold);
	}
	float getProcessRate() const
	{
		return processRate;
	}
private:
	int threshold[3];
	float processRate;
};

static void VS_CC filterInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
	LC_BASIC_DATA *d = (LC_BASIC_DATA *)* instanceData;
	vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef *VS_CC filterGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi);

static void VS_CC filterFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
	LC_BASIC_DATA *d = (LC_BASIC_DATA *)instanceData;
	vsapi->freeNode(d->node);
	delete d;
}

static void VS_CC LC_Create(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi)
{
	LC_BASIC_DATA *data = new LC_BASIC_DATA;
	data->vsapi = vsapi;
	data->node = vsapi->propGetNode(in, "input", 0, 0);
	data->vi = vsapi->getVideoInfo(data->node);

	if (!isConstantFormat(data->vi))
	{
		data->setError(out, "Invalid input clip, only constant format input supported");
		return;
	}
	else if (data->vi->format->colorFamily != cmYUV || (data->vi->format->bitsPerSample!=8 && data->vi->format->bitsPerSample != 16))
	{
		data->setError(out, "Invalid input clip, only YUVP8 or YUVP16 format input supported");
		return;
	}

	int error = 0;
	int n = vsapi->propNumElements(in, "threshold");
	int threshold[3] = { 32,32,32 };
	if ( n>3?3:n )
	{
		for (auto i = 0; i < n; i++)
		{
			threshold[i] = static_cast<int>(vsapi->propGetInt(in, "threshold", i, nullptr));

			if (threshold[i] < 0)
			{
				data->setError(out, "Invalid \"threshold\" assigned, must be a non-negative Integer");
				return;
			}
		}
	}
	float processRate = static_cast<float>(vsapi->propGetFloat(in, "processRate", 0, &error));
	if (error)
	{
		processRate = 1.0;
	}

	data->setParams(threshold, processRate);

	vsapi->createFilter(in, out, "Filter", filterInit, filterGetFrame, filterFree, fmParallel, 0, data, core);
}

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) 
{
	configFunc("com.vapoursynth.LC", "LC", "Cuts lights in scenes", VAPOURSYNTH_API_VERSION, 1, plugin);
	registerFunc("LC",
		"input:clip;"
		"threshold:int[]:opt;"
		"processRate:float:opt;",
		LC_Create, 0, plugin);
}
