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
 * @defgroup AUDIO_SINK_HALTEST_CLASS Audio Sink HAL Tests helper functions Header File
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
 * @file vts_audiosink_test.cpp
 *
 */

#include <ut_kvp_profile.h>
#include <ut.h>
#include <ut_log.h>
#include <ut_control_plane.h>
#include <deque>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/Status.h>
#include <utils/String16.h>
#include <vector>
#include <optional>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <unordered_set>
#include <tuple>
#include <map>

#include "vts_audiosink_helper_functions.h"
#include "vts_audiosink_parse_hfp.h"
#include "vts_audiosink.h"
#include "vts_audiosink_test.h"

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace com::rdk::hal::audiosink;

#define MAX_FRAME_WAIT_TIME 500  /* ms — timeout for decoded frame output (must survive flush gaps) */
#define MAX_RETRY_COUNT 5        /* Maximum number of retries for waiting frames */

void AudioSinkTest::setupClock()
{
    // Open the clock
    AVCLOCK_MANAGER_GETAVCLOCK_SUCCESS(mClockId, mClock);

    // Open the clock controller
    AVCLOCK_OPEN_SUCCESS(mClock, mClockCtrlListener, mClockController,true);

    // Set the clock mode to Audio Master
    AVCLOCK_SET_CLOCK_MODE_SUCCESS(mClock, mClockController, ClockMode::AUDIO_MASTER, true);

    // If backpressure test is enabled, dont start the clock at the beginning
    if(!mTestConfiguration.isSinkBackpressure)
    {
        // Start the clock
        AVCLOCK_START_SUCCESS(mClock, mClockController, true);
    }
}

void AudioSinkTest::tearDownClock()
{
    // Stop the clock
    AVCLOCK_STOP_SUCCESS(mClock, mClockController, true);

    // Close the clock controller
    AVCLOCK_CLOSE_SUCCESS(mClock, mClockController, true);

    mClockController.clear();
    mClock.clear();
    mClockCtrlListener.clear();
}

void AudioSinkTest::setupAudioSink()
{
    /* Open the Decoder */
    AUDIOSINK_OPEN_SUCCESS(mSinkHandle, ContentType::UNKNOWN, mControlListener, mAudioSinkController, true);

    // Attach the clock
    AUDIOSINK_ATTACHCLOCK_SUCCESS(mAudioSinkController, mClockId.value);

    // Set the decoder to the audio sink
    AUDIOSINK_SETAUDIODECODER_SUCCESS(mAudioSinkController, mDecoderId.value);

    /* Start the decoder */
    AUDIOSINK_START_SUCCESS(mSinkHandle, mAudioSinkController, true);

    if(!mClockInUse)
    {
        setupClock();
    }
}

void AudioSinkTest::tearDownAudioSink()
{
    // Detach the clock
    AUDIOSINK_DETACHCLOCK_SUCCESS(mAudioSinkController);

    /* Stop the Audio sink */
    AUDIOSINK_STOP_SUCCESS(mSinkHandle, mAudioSinkController, true);

    /* Close the Audio sink */
    AUDIOSINK_CLOSE_SUCCESS(mSinkHandle, mAudioSinkController, true);
    mAudioSinkController.clear();

    if(!mClockInUse)
    {
        tearDownClock();
    }
}

void AudioSinkTest::cleanupAudioDecoder()
{
    DecodedAudioFrame_t decodedFrame;
    while (mdecoderControlListener->getNextFrame(decodedFrame, 2000, true))
    {
        if(decodedFrame.frameBufferHandle != -1)
        {
            bool isBufferValid;

            UT_LOG_ERROR("[SinkID: %d] Frames received after the audio sink has been stopped", mSinkId.value);
            UT_ASSERT_TRUE(false);

            AVBUFFER_ISVALID_SUCCESS(mAVBufferManager, decodedFrame.frameBufferHandle, isBufferValid);

            if (isBufferValid)
            {
                AVBUFFER_FREE_SUCCESS(mAVBufferManager, decodedFrame.frameBufferHandle);
            }
        }
    }
    mdecoderControlListener->resetEndOfStream();
}

void AudioSinkTest::validateCallbacks(int64_t expectedFirstFramePTS, bool expectUnderflow, bool expectResume)
{
    /* Validate onPrimed from the AV clock in AUDIO_MASTER mode */
    if (!mClockInUse)
    {
        constexpr int64_t kPrimedPtsToleranceNs = 25 * 1000 * 1000; // 25ms
        ::com::rdk::hal::avclock::ClockTime primedClockTime{};
        const bool primedArrived = mClockCtrlListener->waitForPrimed(std::chrono::milliseconds(3000), &primedClockTime);
        UT_LOG_INFO("[SinkID: %d] onPrimed %s: clockTimeNs=%" PRId64 " sampleTimestampNs=%" PRId64
                    " (expected first audio PTS=%" PRId64 ")",
                    mSinkId.value, primedArrived ? "received" : "TIMED OUT",
                    primedClockTime.clockTimeNs, primedClockTime.sampleTimestampNs,
                    expectedFirstFramePTS);
        UT_ASSERT_TRUE(primedArrived);
        if (primedArrived)
        {
            UT_ASSERT_TRUE(primedClockTime.clockTimeNs >= 0);

            if (expectedFirstFramePTS >= 0)
            {
                int64_t deltaNs = primedClockTime.clockTimeNs - expectedFirstFramePTS;
                if (deltaNs < 0)
                {
                    deltaNs = -deltaNs;
                }

                UT_LOG_INFO("[SinkID: %d] onPrimed deltaNs=%" PRId64 " toleranceNs=%" PRId64,
                            mSinkId.value, deltaNs, kPrimedPtsToleranceNs);
                UT_ASSERT_TRUE(primedClockTime.clockTimeNs >= expectedFirstFramePTS);
                if (deltaNs > kPrimedPtsToleranceNs)
                {
                    UT_LOG_WARNING("[SinkID: %d] onPrimed delta exceeded tolerance (deltaNs=%" PRId64 ", toleranceNs=%" PRId64 ")",
                                   mSinkId.value, deltaNs, kPrimedPtsToleranceNs);
                }
            }
        }
    }

    /* Validate the callback */
    int64_t firstFrameRenderedPTS = -1;
    int64_t firstFrameRenderedPTSEvent = -1;
    bool isfirstFrameRendered = false;
    bool isfirstFrameRenderedEvent = false;
    bool expectedFirstFrameRendered = false;

    isfirstFrameRendered = mControlListener->getNextFirstFrameRenderedPTS(firstFrameRenderedPTS);
    isfirstFrameRenderedEvent = mEventListener->getNextFirstFrameRenderedPTS(firstFrameRenderedPTSEvent);

    /* if mixer not connected, we do not expect the first frame rendered callback */
    if(mIsMixerConnected)
    {
        expectedFirstFrameRendered = true;
    }
    else
    {
        expectedFirstFrameRendered = false;
        expectedFirstFramePTS = -1;
    }

    UT_ASSERT_EQUAL(isfirstFrameRendered, expectedFirstFrameRendered);
    UT_ASSERT_EQUAL(firstFrameRenderedPTS, expectedFirstFramePTS);

    UT_ASSERT_EQUAL(isfirstFrameRenderedEvent, expectedFirstFrameRendered);
    UT_ASSERT_EQUAL(firstFrameRenderedPTSEvent, expectedFirstFramePTS);

    if (expectUnderflow)
    {
        int64_t resumedPTS = -1;
        UT_ASSERT_TRUE(mControlListener->getUnderFlowCount(true) > 0);
        UT_ASSERT_TRUE(mControlListener->getNextAudioResumedPTS(resumedPTS));
        UT_ASSERT_TRUE(resumedPTS >= 0);

        resumedPTS = -1;
        UT_ASSERT_TRUE(mEventListener->getUnderFlowCount(true) > 0);
        UT_ASSERT_TRUE(mEventListener->getNextAudioResumedPTS(resumedPTS));
        UT_ASSERT_TRUE(resumedPTS >= 0);
    }

    if(mTestConfiguration.sinkEosPts >= 0)
    {
        /* Control listener — signalEndOfStream() fires onEndOfStream() with the final
         * frame's PTS. If no frames were queued IAVClock::UNDEFINED_TIME is used (negative);
         * for EOS test scenarios frames are always queued first so PTS must be >= 0. */
        int64_t endOfStreamPTS = -1;
        UT_ASSERT_TRUE(mControlListener->getNextEndOfStreamPTS(endOfStreamPTS));
        UT_ASSERT_TRUE(endOfStreamPTS >= 0);

        /* Event listener */
        endOfStreamPTS = -1;
        UT_ASSERT_TRUE(mEventListener->getNextEndOfStreamPTS(endOfStreamPTS));
        UT_ASSERT_TRUE(endOfStreamPTS >= 0);
    }

    if(mTestConfiguration.sinkFlushPts >= 0)
    {
        UT_ASSERT_EQUAL(mControlListener->getFlushCompleteCount(true), 1);
        UT_ASSERT_EQUAL(mEventListener->getFlushCompleteCount(true), 1);
    }
    else
    {
        UT_ASSERT_EQUAL(mControlListener->getFlushCompleteCount(true), 0);
        UT_ASSERT_EQUAL(mEventListener->getFlushCompleteCount(true), 0);
    }
}

void AudioSinkTest::validateMetric()
{
    //TODO: Currently no metrics are defined in the interface. Once metrics are added,
    // implement the validation here based on the test configuration.
}

void AudioSinkTest::copyMetaData(const audiodecoder::FrameMetadata& source, audiodecoder::FrameMetadata& destination)
{
    destination.type = source.type;
    destination.sourceCodec = source.sourceCodec;
    destination.isDolbyAtmos = source.isDolbyAtmos;
    destination.trimStartNs = source.trimStartNs;
    destination.trimEndNs = source.trimEndNs;
    destination.lowLatency = source.lowLatency;
    destination.bitstreamEOS = source.bitstreamEOS;

    destination.discontinuity = source.discontinuity;
    destination.source = source.source;
    // Manually copy optional PCMMetadata
    if (source.metadata.has_value())
    {
        audiodecoder::PCMMetadata pcm;
        pcm.numChannels = source.metadata->numChannels;
        pcm.channelTypes = source.metadata->channelTypes;
        pcm.sampleRate = source.metadata->sampleRate;
        pcm.format = source.metadata->format;
        pcm.planarFormat = source.metadata->planarFormat;
        // do NOT copy pcm.extension
        destination.metadata.emplace(std::move(pcm));
    }
    destination.SoCPrivate = source.SoCPrivate;
}

bool AudioSinkTest::validateSinkBackpressure(const DecodedAudioFrame_t& decodedFrame,
                                             audiodecoder::FrameMetadata& metadata)
{
    bool result = false;
    mTestConfiguration.isSinkBackpressure = false;

    AUDIOSINK_QUEUEAUDIOFRAME_FAILURE(mAudioSinkController, decodedFrame.framePTS, decodedFrame.frameBufferHandle, metadata, Status::EX_NONE);
    AUDIOSINK_QUEUEAUDIOFRAME_FAILURE(mAudioSinkController, decodedFrame.framePTS, decodedFrame.frameBufferHandle, metadata, Status::EX_NONE);
    AUDIOSINK_QUEUEAUDIOFRAME_FAILURE(mAudioSinkController, decodedFrame.framePTS, decodedFrame.frameBufferHandle, metadata, Status::EX_NONE);

    // Start the clock to allow queued frames to be processed and free up space in the queue.
    AVCLOCK_START_SUCCESS(mClock, mClockController, true);
    result = mControlListener->waitForFrameBufferAvailableEvent(std::chrono::milliseconds(1000));
    UT_ASSERT_TRUE(result);

    result = false;
    AUDIOSINK_QUEUEAUDIOFRAME_SUCCESS(mAudioSinkController, decodedFrame.framePTS,
                                      decodedFrame.frameBufferHandle, metadata, result);
    UT_ASSERT_TRUE(result);
    return result;
}

void AudioSinkTest::queueDecodedFrameIfNonTunnel(const DecodedAudioFrame_t& decodedFrame,
                                                  OperationalMode operationalMode,
                                                  audiodecoder::FrameMetadata& metadata,
                                                  bool& flushDone,
                                                  bool isFirstFrame)
{
    if(operationalMode == OperationalMode::NON_TUNNELLED && decodedFrame.frameBufferHandle == -1)
    {
        UT_LOG_ERROR("[SinkID: %d] Non-tunnelled frame received with invalid buffer handle, skipping the frame", mSinkId.value);
        UT_ASSERT_TRUE(false);
        return;
    }
    if (operationalMode == OperationalMode::TUNNELLED && decodedFrame.frameBufferHandle != -1)
    {
        bool isBufferValid;

        UT_LOG_ERROR("[SinkID: %d] Tunnelled frame received with valid buffer handle, skipping the frame", mSinkId.value);
        UT_ASSERT_TRUE(false);

        AVBUFFER_ISVALID_SUCCESS(mAVBufferManager, decodedFrame.frameBufferHandle, isBufferValid);

        if (isBufferValid)
        {
            AVBUFFER_FREE_SUCCESS(mAVBufferManager, decodedFrame.frameBufferHandle);
        }
        return;
    }

    if (mTestConfiguration.sinkFlushPts > 0 &&
        decodedFrame.framePTS >= mTestConfiguration.sinkFlushPts && !flushDone)
    {
        AUDIOSINK_FLUSH_SUCCESS(mSinkHandle, mAudioSinkController, true);
        flushDone = true;
    }

    if (decodedFrame.isMetaAvailable)
    {
        copyMetaData(*decodedFrame.frameMetadata, metadata);
    }

    if(decodedFrame.frameBufferHandle != -1)
    {
        bool result = false;
        AUDIOSINK_QUEUEAUDIOFRAME_SUCCESS(mAudioSinkController, decodedFrame.framePTS,
                                          decodedFrame.frameBufferHandle, metadata, result);
        if (!result)
        {
            if(mTestConfiguration.isSinkBackpressure)
            {
                if(validateSinkBackpressure(decodedFrame, metadata))
                {
                    return;
                }
            }

            UT_LOG_WARNING("[SinkID: %d] Failed to queue frame to audio sink, PTS: %lld, Buffer Handle: 0x%016" PRIx64,
                mSinkId.value, decodedFrame.framePTS, decodedFrame.frameBufferHandle);
            AVBUFFER_FREE_SUCCESS(mAVBufferManager, decodedFrame.frameBufferHandle);
        }
        else
        {
            if(mTestConfiguration.isSinkBackpressure && isFirstFrame)
            {
                UT_LOG_ERROR("[SinkID: %d] queueAudioFrame sucessful even though clock is not started", mSinkId.value);
                UT_ASSERT_TRUE(false);
            }
        }
    }
}

void AudioSinkTest::run()
{
    try
    {
        Pool audioPool;
        PropertyValue avSourcePropertyValue;
        audiodecoder::FrameMetadata metadata;
        DecodedAudioFrame_t decodedFrame;
        int32_t frameCount = 0;
        int64_t firstFramePTS = -1;
        bool flushDone = false;
        int32_t retryCount = 0;
        avSourcePropertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(int32_t(static_cast<int32_t>(AVSource::IP)));
        OperationalMode operationalMode = OperationalMode::NON_TUNNELLED;

        UT_LOG_INFO("[SinkID: %d] Start of Testing Stream: %s", mSinkId.value, mTestConfiguration.input.c_str());

        setupAudioSink();

        while (true)
        {
            if(!mdecoderControlListener->getNextFrame(decodedFrame, MAX_FRAME_WAIT_TIME, true))
            {
                /* If frame not received within the wait time, check if EOS has been reached */
                if (mdecoderControlListener->waitForEndOfStream(MAX_FRAME_WAIT_TIME))
                {
                    /* EOS has been reached, no more frames expected */
                    break;
                }

                /* EOS has not been reached, continue waiting for frames */
                retryCount++;
                UT_LOG_ERROR("[Audio DecoderID: %d, SinkID: %d] decoded frame not received within %d ms",
                                     mDecoderId.value, mSinkId.value, MAX_FRAME_WAIT_TIME);

                /* Report failure only if test is not underflow test */
                if(mTestConfiguration.decoderDelay < MAX_FRAME_WAIT_TIME)
                    UT_ASSERT_TRUE(false);

                if(retryCount < MAX_RETRY_COUNT)
                {
                    continue;
                }
                else
                {
                    UT_LOG_ERROR("[Audio DecoderID: %d, SinkID: %d] Max retry count reached, exiting", mDecoderId.value, mSinkId.value);
                    break;
                }
            }

            /* Reset retry count for the next frame */
            retryCount = 0;

            frameCount++;

            if (frameCount == 1)
            {
                /* Get the operational mode of the decoder, which can be queried only after the decoder start */
                AUDIODECODER_GET_OPERATIONALMODE_SUCCESS(mDecoderHandle, operationalMode);
                firstFramePTS = decodedFrame.framePTS;
                if(!decodedFrame.isMetaAvailable)
                {
                    UT_LOG_ERROR("[SinkID: %d] First frame received without metadata, skipping the frame", mSinkId.value);
                    UT_ASSERT_TRUE(false);
                    break;
                }

                /* Start the clock if the decoder is tunnelled and sink backpressure is enabled */
                if (operationalMode == OperationalMode::TUNNELLED && mTestConfiguration.isSinkBackpressure)
                {
                    AVCLOCK_START_SUCCESS(mClock, mClockController, true);
                }
            }

            queueDecodedFrameIfNonTunnel(decodedFrame, operationalMode, metadata, flushDone, (frameCount == 1));

            std::optional<PropertyValue> latencyValue;
            AUDIOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, audiosink::Property::RENDER_LATENCY_NS, latencyValue);
            UT_LOG_INFO("[SinkID: %d] Render Latency: %lld ns", mSinkId.value, latencyValue->value->get<PropertyValue::Value::Tag::longValue>());
        }

        /* Signal EOS to the sink after the decoder has completed and all frames were queued. */
        AUDIOSINK_SIGNAL_ENDOFSTREAM_SUCCESS(mAudioSinkController);

        validateCallbacks(firstFramePTS, mTestConfiguration.decoderDelay > 0, mTestConfiguration.decoderDelay > 0);

        /* Validate the metrics */
        validateMetric();

        /* Stop the Audio sink */
        tearDownAudioSink();

        /* cleanup if any buffers received from the decoder after closing sink */
        cleanupAudioDecoder();
    }
    catch (const std::exception& ex)
    {
        UT_LOG_ERROR("[SinkID: %d] Exception in audioSinkNonTunnelTest: %s", mSinkId.value, ex.what());
        UT_ASSERT_TRUE(false);
    }
    catch (...)
    {
        UT_LOG_ERROR("[SinkID: %d] Unknown exception in audioSinkNonTunnelTest", mSinkId.value);
        UT_ASSERT_TRUE(false);
    }
}

/** @} */ // End of AUDIO_SINK_HALTEST_CLASS
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK
/** @} */ // End of HPK
