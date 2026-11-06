/**
 *  If not stated otherwise in this file or this component's LICENSE
 *  file the following copyright and licenses apply:
 *
 *  Copyright 2024 RDK Management
 *
 *  Licensed under the Apache License, Version 2.0 (the License);
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an AS IS BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @addtogroup HPK Hardware Porting Kit
 * @{
 * @par The Hardware Porting Kit
 * HPK is the next evolution of the well-defined Hardware Abstraction Layer
 * (HAL), but augmented with more comprehensive documentation and test suites
 * that OEM or SOC vendors can use to self-certify their ports before taking
 * them to RDKM for validation or to an operator for final integration and
 * deployment. The Hardware Porting Kit effectively enables an OEM and/or SOC
 * vendor to self-certify their own Video Accelerator devices, with minimal RDKM
 * assistance.
 *
 */

/**
 * @addtogroup AUDIO_SINK Audio Sink Module
 * @{
 */

/**
 * @addtogroup AUDIO_SINK_HALTEST Audio Sink HAL Tests
 * @{
 */

/**
 * @defgroup AUDIO_SINK_HALTEST_HEADER Audio Sink HAL Tests helper functions Header File
 * @{
 * @parblock
 *
 * ### Common functions/macros required for the audio module :
 *
 * Common functions/macros required for the audio module .
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_audiosink_test.h
 *
 */

#ifndef __VTS_AUDIOSINK_TEST_H__
#define __VTS_AUDIOSINK_TEST_H__

#include <thread>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <inttypes.h>

#include "vts_audiodecoder.h"
#include "vts_audiodecoder_test.h"
#include "vts_avbuffer.h"
#include "vts_avclock.h"

#include "com/rdk/hal/audiosink/IAudioSinkManager.h"
#include "com/rdk/hal/audiosink/BnAudioSinkControllerListener.h"
#include "com/rdk/hal/audiosink/BnAudioSinkEventListener.h"
#include "com/rdk/hal/audiosink/ContentType.h"
#include "com/rdk/hal/audiodecoder/FrameMetadata.h"

#include "com/rdk/hal/avclock/IAVClock.h"
#include "com/rdk/hal/avclock/IAVClockManager.h"
#include "com/rdk/hal/avclock/BnAVClockControllerListener.h"
#include "com/rdk/hal/avclock/BnAVClockEventListener.h"

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace std;
using namespace com::rdk::hal::audiosink;
using namespace com::rdk::hal::avclock;

class AudioSinkTest
{
public:
    AudioSinkTest(IAudioSink::Id sinkId,
                  sp<IAudioSink> sinkHandle,
                  sp<IAudioDecoder> decoderHandle,
                  sp<AudioSinkControllerListener> controlListener,
                  sp<AudioSinkEventListener> eventListener,
                  sp<AudioDecoderControllerListener> decoderControlListener,
                  sp<IAVBuffer> AVBufferManager,
                  IAudioDecoder::Id decoderId,
                  IAVClock::Id clockId,
                  bool secureDecode, const testConfiguration_t& config, bool isClockInUse)
    {
        mSinkId = sinkId;
        mDecoderId = decoderId;
        mSinkHandle = sinkHandle;
        mControlListener = controlListener;
        mSecureBuffer = secureDecode;
        mDecoderHandle = decoderHandle;
        mEventListener = eventListener;
        mTestConfiguration = config;
        mAVBufferManager = AVBufferManager;
        mdecoderControlListener = decoderControlListener;
        mClockId = clockId;
        mClockInUse = isClockInUse;
        mClockCtrlListener = new AudioSinkAVClockControllerListener();
    }

    void run();
    bool hasFrameOutputModeMismatch() const { return mFrameOutputModeMismatch; }

private:
    IAudioSink::Id mSinkId;
    IAudioDecoder::Id mDecoderId;
    IAVClock::Id mClockId;
    sp<IAudioSink> mSinkHandle;
    sp<AudioSinkControllerListener> mControlListener;
    sp<AudioSinkEventListener> mEventListener;
    sp<IAVBuffer> mAVBufferManager;
    sp<AudioDecoderControllerListener> mdecoderControlListener;
    bool mSecureBuffer;
    bool mIsTunnel;
    sp<IAudioDecoder> mDecoderHandle;
    bool mClockInUse;
    bool mFrameOutputModeMismatch = false;
    testConfiguration_t mTestConfiguration;
    sp<IAudioSinkController> mAudioSinkController;
    sp<IAVClock> mClock;
    sp<IAVClockController> mClockController;
    sp<AudioSinkAVClockControllerListener> mClockCtrlListener;

    void audioSinkNonTunnelTest();
    void audioSinkTunnelTest();

    void setupClock();
    void tearDownClock();
    void setupAudioSink();
    void tearDownAudioSink();
    void validateCallbacks(int64_t expectedFirstFramePTS, bool expectUnderflow, bool expectResume);
    void copyMetaData(const audiodecoder::FrameMetadata& source, audiodecoder::FrameMetadata& destination);
    void queueDecodedFrameIfNonTunnel(const DecodedAudioFrame_t& decodedFrame,
                                      OperationalMode operationalMode,
                                      audiodecoder::FrameMetadata& metadata,
                                      bool& flushDone);
    void validateMetric();
};

#endif //__VTS_AUDIOSINK_TEST_H__

/** @} */ // End of AUDIO_SINK_HALTEST_HEADER
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK
/** @} */ // End of HPK