#include "Helper.h"
#include "LC.H"

static const VSFrameRef *VS_CC filterGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi)
{
	LC_BASIC_DATA *d = (LC_BASIC_DATA *)* instanceData;

	if (activationReason == arInitial) {
		vsapi->requestFrameFilter(n, d->node, frameCtx);
	}
	else if (activationReason == arAllFramesReady) {
		VSFrameRef * result = nullptr;
		if (d->vi->format->bitsPerSample == 8)
		{
			LC_Process<BYTE> worker(*d, n, frameCtx, core, vsapi);
			worker.process();
			result = worker.GetDst();
		}
		else
		{
			LC_Process<WORD> worker(*d, n, frameCtx, core, vsapi);
			worker.process();
			result = worker.GetDst();
		}

		return result;
	}

	return 0;
}