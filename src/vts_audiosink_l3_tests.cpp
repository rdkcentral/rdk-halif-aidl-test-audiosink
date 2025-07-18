/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
 * @addtogroup HPK Hardware Porting Kit
 * @{
 *
 */

/**
 * @addtogroup AUDIO_SINK Audio Sink Module
 * @{
 *
 */

/**
 * @defgroup AUDIO_SINK_HALTEST Audio Sink HAL Tests
 * @{
 *
 */

/**
 * @defgroup AUDIO_SINK_HALTEST_L3 Audio Sink HAL Tests L3 File
 * @{
 * @parblock
 *
 * ### L3 Test Cases for Audio Sink HAL :
 *
 * This module includes Level 3 functional tests (success and failure scenarios).
 * This is to ensure that the API meets the operational requirements of the module across all vendors.
 *
 * **Pre-Conditions:** @n
 * **Dependencies:** None@n
 *
 * @endparblock
 *
 */

/**
 * @file vts_audiosink_l3_tests.c
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

#include "com/rdk/hal/avbuffer/IAVBuffer.h"
#include "com/rdk/hal/avbuffer/BnAVBufferSpaceListener.h"

#include "vts_audiosink_helper_functions.h"
#include "vts_audiosink_parse_hfp.h"
#include "vts_audiosink.h"
#include "vts_avbuffer.h"

#include "IAVBufferHelper.h"
#include "vts_audiodecoder.h"

static const char *gModuleName = "IAudioSinkManager";
static const char *gAudioModuleName = "IAudioDecoderManager";
static const char *gTestGroup = "L3";
static IAVBufferHelper* gAVbufferHelper;

sp<IAVBuffer> vts_avbuffer_getService()
{
    return
        android::interface_cast<IAVBuffer>(
            android::defaultServiceManager()->getService(
                android::String16(IAVBuffer::serviceName().c_str())
            )
        );
}

/* Test class registration with UT_Core */
UT_ADD_TEST_TO_GROUP(VTS_L3_AudioSink, UT_TESTS_L3);

extern sp<IAudioSinkManager> gAudioSinkManager;

class AVBufferSpaceListener : public BnAVBufferSpaceListener
{
public:

    AVBufferSpaceListener() {};
    virtual ~AVBufferSpaceListener() = default;

    Status onSpaceAvailable() override
    {
        return Status::ok();
    }
};

class SinkTest
{
public:
    SinkTest(IAudioSink::Id sinkId, sp<IAudioSink> sinkHandle,
             sp<AudioSinkControllerListener> controlListener,
             sp<AudioSinkEventListener> eventListener, sp<AudioDecoderControllerListener> decoderControlListener,
             sp<IAVBuffer> AVBufferManager, IAudioDecoder::Id decoderId,
             bool secureDecode, bool isTunnel, audioTestConfiguration_t config)
    {
        mSinkId = sinkId;
        mDecoderId = decoderId;
        mSinkHandle = sinkHandle;
        mControlListener = controlListener;
        mSecureBuffer = secureDecode;
        mEventListener = eventListener;
        mTestConfiguration = config;
        mAVBufferManager = AVBufferManager;
        mdecoderControlListener = decoderControlListener;
        mIsTunnel = isTunnel;
    }

    void run()
    {
        if(mIsTunnel)
        {
            audioSinkTunnelTest();
        }
        else
        {
            audioSinkNonTunnelTest();
        }
    }

private:
    IAudioSink::Id mSinkId;
    IAudioDecoder::Id mDecoderId;
    sp<IAudioSink> mSinkHandle;
    sp<AudioSinkControllerListener> mControlListener;
    sp<AudioSinkEventListener> mEventListener;
    sp<IAVBuffer> mAVBufferManager;
    sp<AudioDecoderControllerListener> mdecoderControlListener;
    bool mSecureBuffer;
    bool mIsTunnel;
    audioTestConfiguration_t mTestConfiguration;

    void audioSinkNonTunnelTest()
    {
        try {
            sp<IAudioSinkController> audioSinkController;
            int64_t audioBufferHandle;
            Pool audioPool;
            PropertyValue avSourcePropertyValue;
            sp<IAVBufferSpaceListener> bufferListener = new AVBufferSpaceListener();
            audiodecoder::FrameMetadata metadata;
            DecodedAudioFrame_t decodedFrame;
            int32_t frameCount = 0;
            int64_t firstFramePTS = -1;

            avSourcePropertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(int32_t(static_cast<int32_t>(AVSource::IP)));

            UT_LOG_INFO("[SinkID: %d] Start of Testing Stream: %s", mSinkId.value, mTestConfiguration.input.c_str());

            /* Open the Decoder */
            AUDIOSINK_OPEN_SUCCESS(mSinkHandle, mControlListener, audioSinkController, true);

            AUDIOSINK_SETAUDIODECODER_SUCCESS(mDecoderId.value, audioSinkController);

            /* Start the decoder */
            AUDIOSINK_START_SUCCESS(mSinkHandle, audioSinkController, true);

            while(mdecoderControlListener->getNextFrame(decodedFrame, true))
            {
                frameCount++;
                if (frameCount == 1)
                {
                    firstFramePTS = decodedFrame.framePTS;
                    if(!decodedFrame.isMetaAvailable)
                    {
                        UT_LOG_ERROR("[SinkID: %d] First frame received without metadata, skipping the frame", mSinkId.value);
                        UT_ASSERT_TRUE(false);
                        break;
                    }
                }
                if(decodedFrame.isMetaAvailable)
                {
                    metadata.channelTypes = decodedFrame.metadata.channelTypes;
                    metadata.numChannels = decodedFrame.metadata.numChannels;
                    metadata.sampleRate = decodedFrame.metadata.sampleRate;
                    metadata.format = decodedFrame.metadata.format;
                    metadata.planarFormat = decodedFrame.metadata.planarFormat;
                    metadata.isDolbyAtmos = decodedFrame.metadata.isDolbyAtmos;
                    metadata.trimStartNs = decodedFrame.metadata.trimStartNs;
                    metadata.trimEndNs = decodedFrame.metadata.trimEndNs;
                    metadata.lowLatency = decodedFrame.metadata.lowLatency;
                    metadata.endOfStream = decodedFrame.metadata.endOfStream;
                    metadata.discontinuity = decodedFrame.metadata.discontinuity;
                    metadata.source = decodedFrame.metadata.source;
                    metadata.sourceCodec = decodedFrame.metadata.sourceCodec;
                }
                if(mTestConfiguration.decoderDelay > 0)
                {
                    this_thread::sleep_for(chrono::milliseconds(mTestConfiguration.decoderDelay));
                }
                if(mTestConfiguration.sinkEosPts == decodedFrame.framePTS)
                {
                    UT_LOG_WARNING("[SinkID: %d] Set EOS at PTS: %lld", mSinkId.value, decodedFrame.framePTS);
                    metadata.endOfStream = true;
                }
                if(mTestConfiguration.sinkFlushPts == decodedFrame.framePTS)
                {
                    AUDIOSINK_FLUSH_SUCCESS(mSinkHandle, audioSinkController, true);
                    UT_LOG_WARNING("[SinkID: %d] Flushed the decoder at PTS: %lld", mSinkId.value, decodedFrame.framePTS);
                }
                //auto now = std::chrono::high_resolution_clock::now();
                //auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
                //UT_LOG_ERROR("[SinkID: %d] Current time: %lld us", mSinkId.value, static_cast<long long>(us));
                AUDIOSINK_QUEUEAUDIOFRAME_SUCCESS(mSinkId.value, audioSinkController, decodedFrame.framePTS, decodedFrame.frameBufferHandle, metadata);

                std::optional<PropertyValue> latencyValue;
                AUDIOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, audiosink::Property::RENDER_LATENCY_NS, latencyValue);

                UT_LOG_INFO("[SinkID: %d] Render Latency: %lld ns", mSinkId.value, latencyValue->value->get<PropertyValue::Value::Tag::longValue>());
            }

            /* Validate the callback */
            {
                /* Control listener */
                int64_t firstFrameRenderedPTS = -1;
                UT_ASSERT_TRUE(mControlListener->getNextFirstFrameRenderedPTS(firstFrameRenderedPTS));
                UT_ASSERT_EQUAL(firstFramePTS, firstFrameRenderedPTS);

                /* Event listener */
                firstFrameRenderedPTS = -1;
                UT_ASSERT_TRUE(mEventListener->getNextFirstFrameRenderedPTS(firstFrameRenderedPTS));
                UT_ASSERT_EQUAL(firstFramePTS, firstFrameRenderedPTS);
            }

            if(mTestConfiguration.decoderDelay > 0)
            {
                /* Control listener */
                int64_t resumedPTS = -1;
                UT_ASSERT_TRUE(mControlListener->getUnderFlowCount(true) > 0);
                UT_ASSERT_TRUE(mControlListener->getNextAudioResumedPTS(resumedPTS));
                UT_ASSERT_TRUE(resumedPTS >= 0);

                /* Event listener */
                resumedPTS = -1;
                UT_ASSERT_TRUE(mEventListener->getUnderFlowCount(true) > 0);
                UT_ASSERT_TRUE(mEventListener->getNextAudioResumedPTS(resumedPTS));
                UT_ASSERT_TRUE(resumedPTS >= 0);
            }
            else
            {
                /* Control listener */
                int64_t resumedPTS = -1;
                UT_ASSERT_TRUE(mControlListener->getUnderFlowCount(true) == 0);
                UT_ASSERT_FALSE(mControlListener->getNextAudioResumedPTS(resumedPTS));

                /* Event listener */
                resumedPTS = -1;
                UT_ASSERT_TRUE(mEventListener->getUnderFlowCount(true) == 0);
                UT_ASSERT_FALSE(mEventListener->getNextAudioResumedPTS(resumedPTS));
            }

            if(mTestConfiguration.sinkEosPts >= 0)
            {
                /* Control listener */
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
                UT_ASSERT_TRUE(mControlListener->getFlushCompleteCount(true));
                UT_ASSERT_TRUE(mEventListener->getFlushCompleteCount(true));
            }
            else
            {
                UT_ASSERT_FALSE(mControlListener->getFlushCompleteCount(true));
                UT_ASSERT_FALSE(mEventListener->getFlushCompleteCount(true));
            }

            /* Stop the Audio sink */
            AUDIOSINK_STOP_SUCCESS(mSinkHandle, audioSinkController, true);

            /* Close the Audio sink */
            AUDIOSINK_CLOSE_SUCCESS(mSinkHandle, audioSinkController, true);

        } catch (const std::exception& ex) {
            UT_LOG_ERROR("[SinkID: %d] Exception in audioSinkNonTunnelTest: %s", mSinkId.value, ex.what());
            UT_ASSERT_TRUE(false);
        } catch (...) {
            UT_LOG_ERROR("[SinkID: %d] Unknown exception in audioSinkNonTunnelTest", mSinkId.value);
            UT_ASSERT_TRUE(false);
        }
    }

    void audioSinkTunnelTest()
    {
        //TODO: implement the tunnel test
    }
};

class VTS_L3_AudioSink : public UTCore
{
public:
VTS_L3_AudioSink() : UTCore() {}

~VTS_L3_AudioSink() override = default;

public:
    std::vector<std::tuple<IAudioSink::Id, sp<IAudioSink>, sp<AudioSinkControllerListener>, sp<AudioSinkEventListener>>> mAudioSinkList;
    vector<tuple<IAudioDecoder::Id, sp<IAudioDecoder>, sp<AudioDecoderControllerListener>, sp<AudioDecoderEventListener>, audiodecoder::Capabilities *>> mAudioDecoderList;
    sp<IAVBuffer> mAVBufferManager;
    IAVBufferHelper* mAVbufferHelper;

    void SetUp() override
    {
        std::vector<IAudioSink::Id> sinkIds;
        vector<IAudioDecoder::Id> decoderIds;

        mAVBufferManager = vts_getAVBufferService();
        gAudioSinkTestConfiguration = vts_kvp_create_instance(AUDIO_DECODER_TEST_CONFIG_PATH);

        mAVbufferHelper = getAVBufferHelperInstance();

        /* Get the Audio Sink IDs */
        AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

        /* Get the Audio decoder IDs */
        AUDIODECODER_MANAGER_GETAUDIODECODERIDS(decoderIds);

        android::ProcessState::self()->startThreadPool();

        for(const auto& _sinkId: sinkIds)
        {
            sp<IAudioSink> audioSink;

            sp<AudioSinkEventListener> eventListener = new AudioSinkEventListener(_sinkId.value);

            sp<AudioSinkControllerListener> controlListener = new AudioSinkControllerListener(_sinkId.value);

            /* Get Audio decoder handle */
            AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

            /* Register events */
            AUDIOSINK_REGISTER_EVENT_SUCCESS(audioSink, eventListener);

            /* Get Capabilities of Audio Sink */
            //TODO: Uncomment when capabilities are implemented in the HAL
            //AUDIOSINK_GETCAPABILITIES(audioSink, capabilities);

            mAudioSinkList.push_back({_sinkId, audioSink, controlListener, eventListener});
        }
        for(const auto& _decoderId: decoderIds)
        {
            sp<IAudioDecoder> audioDecoder;

            audiodecoder::Capabilities *capabilities = new audiodecoder::Capabilities();

            sp<AudioDecoderEventListener> eventListener = new AudioDecoderEventListener();

            sp<AudioDecoderControllerListener> controlListener = new AudioDecoderControllerListener();

            controlListener->setDecoderID(_decoderId.value);
            eventListener->setDecoderID(_decoderId.value);

            /* Get Audio decoder handle */
            AUDIODECODER_MANAGER_GETAUDIODECODER(_decoderId, audioDecoder);

            /* Register events */
            AUDIODECODER_REGISTER_EVENT_SUCCESS(audioDecoder, eventListener);

            /* Get Capabilities of Audio Decoder */
            AUDIODECODER_GETCAPABILITIES(audioDecoder, capabilities);

            mAudioDecoderList.push_back({_decoderId, audioDecoder, controlListener, eventListener, capabilities});
        }
    }

    void checkCompleteStateTransition(sp<IAudioSink> sink, sp<AudioSinkEventListener> eventListener,
                                      sp<AudioSinkControllerListener> controlListener, State oldState,
                                      State intermediateState, State newState)
    {
        ::com::rdk::hal::State currentState;

        UT_ASSERT_TRUE(eventListener->checkStateTransistion(oldState, intermediateState));
        UT_ASSERT_TRUE(controlListener->checkStateTransistion(oldState, intermediateState));

        eventListener->waitForState(newState, std::chrono::milliseconds(AUDIO_SINK_MAX_FINAL_STATE_WAIT_MS));
        controlListener->waitForState(newState, std::chrono::milliseconds(AUDIO_SINK_MAX_FINAL_STATE_WAIT_MS));

        AUDIOSINK_GET_STATE(sink, currentState);

        UT_ASSERT_EQUAL(currentState, newState);

        UT_ASSERT_TRUE(eventListener->checkStateTransistion(intermediateState, newState));
        UT_ASSERT_TRUE(controlListener->checkStateTransistion(intermediateState, newState));

        eventListener->resetStateChanges();
        controlListener->resetStateChanges();
    }

    bool isSinkUsable(const IAudioSink::Id& sinkId)
    {
        for (const auto& item : gAudiosinks_hfp)
        {
            if (std::find(item.availableIds.begin(), item.availableIds.end(), sinkId.value) != item.availableIds.end())
            {
                return true;
            }
        }
        return false;
    }

    bool isDecoderUsable(const IAudioDecoder::Id& decoderId)
    {
        if (std::find(gAudiosinks_hfp[0].availableDecoderIds.begin(),
                gAudiosinks_hfp[0].availableDecoderIds.end(),
                decoderId.value) == gAudiosinks_hfp[0].availableDecoderIds.end())
        {
            return false;
        }
        return true;
    }

    void runSequentialSink(audioTestConfiguration_t& testConfig, bool secureDecode, bool isTunnel)
    {
        try {
            for (const auto& [_sinkId, sinkHandle, controlListener, eventListener] : mAudioSinkList)
            {
                if (!isSinkUsable(_sinkId))
                {
                    UT_LOG_INFO("[SinkID: %d] is not usable for this test configuration, skipping.", _sinkId.value);
                    continue;
                }

                // Find a usable decoder
                IAudioDecoder::Id decoderId;
                sp<AudioDecoderControllerListener> decoderControlListener;
                sp<IAudioDecoder> decoderHandle;
                sp<AudioDecoderEventListener> decoderEventListener;
                bool foundDecoder = false;

                for (const auto& [_decoderId, _decoderHandle, _controlListener, _eventListener, _capabilities] : mAudioDecoderList)
                {
                    if (isDecoderUsable(_decoderId))
                    {
                        decoderId = _decoderId;
                        decoderHandle = _decoderHandle;
                        decoderEventListener = _eventListener;
                        decoderControlListener = _controlListener;
                        foundDecoder = true;
                        break;
                    }
                }

                if (!foundDecoder)
                {
                    UT_LOG_WARNING("No usable decoder found for Sink ID %d, skipping.", _sinkId.value);
                    return;
                }

                HeapMetrics beforeHeapMetrics, afterHeapMetrics;
                AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, beforeHeapMetrics);

                audioDecoderTest decodeTest(decoderId, decoderHandle, decoderControlListener,
                                    decoderEventListener, mAVBufferManager, mAVbufferHelper, secureDecode, testConfig);

                std::thread decodeThread(&audioDecoderTest::run, &decodeTest, false);

                SinkTest sinkTest(_sinkId, sinkHandle, controlListener, eventListener, decoderControlListener,
                                mAVBufferManager, decoderId, secureDecode, isTunnel, testConfig);

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                sinkTest.run();

                if (decodeThread.joinable())
                    decodeThread.join();

                AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, afterHeapMetrics);

                // Check if the decoder freed all the buffer pools
                UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
                UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
                UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);
            }
        } catch (const std::exception& ex) {
            UT_LOG_ERROR("Exception in runSequentialSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
        } catch (...) {
            UT_LOG_ERROR("Unknown exception in runSequentialSink");
            UT_ASSERT_TRUE(false);
        }
    }

    void runConcurrentSink(audioTestConfiguration_t& testConfig, bool secureDecode, bool isTunnel)
    {
        try {
            HeapMetrics beforeHeapMetrics, afterHeapMetrics;
            vector<thread> decoderThreads;
            vector<thread> sinkThreads;
            vector<unique_ptr<audioDecoderTest>> decoderTests;
            vector<unique_ptr<SinkTest>> sinkTests;
            vector<pair<IAudioDecoder::Id, sp<AudioDecoderControllerListener>>> availableDecoderList;

            /* Get heap metrics */
            AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, beforeHeapMetrics);

            for (const auto& [_decoderId, decoderHandle, controlListener, eventListener, capabilities] : mAudioDecoderList)
            {
                // Check if _decoderId is available in availableDecoderIds
                if (isDecoderUsable(_decoderId))
                {
                   auto decoderTest = make_unique<audioDecoderTest>(_decoderId, decoderHandle,
                                                                controlListener, eventListener,
                                                                mAVBufferManager, mAVbufferHelper,
                                                                secureDecode, testConfig);
                    decoderThreads.emplace_back(&audioDecoderTest::run, decoderTest.get(), false);

                    decoderTests.push_back(move(decoderTest));

                    availableDecoderList.push_back({_decoderId, controlListener});
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            for(int32_t i = 0; i < availableDecoderList.size(); i++)
            {
                auto& [sinkId, sinkHandle, controlListener, eventListener] = mAudioSinkList[i];
                if(!isSinkUsable(sinkId))
                {
                    UT_LOG_INFO("[SinkID: %d] is not usable for this test configuration, skipping.", sinkId.value);
                    continue;
                }
                auto& [decoderId, decoderControlListener] = availableDecoderList[i];

                auto sinkTest = std::make_unique<SinkTest>(sinkId, sinkHandle,
                                                            controlListener, eventListener,
                                                            decoderControlListener,
                                                            mAVBufferManager, decoderId,
                                                            secureDecode, isTunnel,
                                                            testConfig);

                sinkThreads.emplace_back(&SinkTest::run, sinkTest.get());

                sinkTests.push_back(std::move(sinkTest));
            }

            for (auto& t : decoderThreads)
            {
                if (t.joinable())
                    t.join();
            }
            for (auto& t : sinkThreads)
            {
                if (t.joinable())
                    t.join();
            }
            /* Get heap metrics */
            AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, afterHeapMetrics);

            /* check if the decoder freed all the buffer pools */
            UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);

        } catch (const std::exception& ex) {
            UT_LOG_ERROR("Exception in runConcurrentSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
        } catch (...) {
            UT_LOG_ERROR("Unknown exception in runConcurrentSink");
            UT_ASSERT_TRUE(false);
        }
    }

    void runSingleProcessTest(const char *testName, bool isSecureMode, bool isTunnel, bool isConcurrentMode)
    {
        vector<audioTestConfiguration_t> testConfiguration;

        vts_audiodecoder_parse_testconfig(gAudioSinkTestConfiguration, gAudioModuleName, gTestGroup, testName, testConfiguration);

        if(isSecureMode && !gAudiosinks_hfp[0].supportsSecure)
        {
            UT_LOG_WARNING("Audio Sink does not support secure mode, skipping test");
            return;
        }

        if(isTunnel)
        {
            UT_LOG_WARNING("Audio Sink does not support tunnel mode, skipping test");
            return;
        }

        if(isConcurrentMode)
        {
            for(auto& test: testConfiguration)
            {
                runConcurrentSink(test, isSecureMode, isTunnel);
            }
        }
        else
        {
            for(auto& test: testConfiguration)
            {
                runSequentialSink(test, isSecureMode, isTunnel);
            }
        }
    }

    void TearDown() override
    {
        for (const auto& [_sinkId, sinkHandle, controlListener, eventListener] : mAudioSinkList)
        {
            /* Un-register the events */
            AUDIOSINK_UNREGISTER_EVENT_SUCCESS(sinkHandle, eventListener);
        }
        mAudioSinkList.clear();
        //TODO: check right place to call joinThreadPool
        //android::IPCThreadState::self()->joinThreadPool();
    }
};

/**
* @brief Test the audio sink functionality (sequential, non-tunnel, non-secure)
*
* This test verifies the basic decode functionality.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 001@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_NonTunnel_NonSecure)
{
    const char *testName = "Decode";
    bool isSecureMode = false;
    bool isConcurrentMode = false;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (sequential, non-tunnel, non-secure)
*
* This test verifies the flush scenario.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 002@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_NonTunnel_NonSecure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = false;
    bool isConcurrentMode = false;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (sequential, non-tunnel, non-secure)
*
* This test verifies the EOS scenario.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 003@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_NonTunnel_NonSecure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = false;
    bool isConcurrentMode = false;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (sequential, non-tunnel, non-secure)
*
* This test verifies the underflow scenario.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 004@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_NonTunnel_NonSecure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = false;
    bool isConcurrentMode = false;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink functionality (sequential, non-tunnel, secure)
*
* This test verifies the basic decode functionality in secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 005@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_NonTunnel_Secure)
{
    const char *testName = "Decode";
    bool isSecureMode = true;
    bool isConcurrentMode = false;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (sequential, non-tunnel, secure)
*
* This test verifies the flush scenario in secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 006@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_NonTunnel_Secure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = true;
    bool isConcurrentMode = false;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (sequential, non-tunnel, secure)
*
* This test verifies the EOS scenario in secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 007@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_NonTunnel_Secure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = true;
    bool isConcurrentMode = false;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (sequential, non-tunnel, secure)
*
* This test verifies the underflow scenario in secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 008@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_NonTunnel_Secure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = true;
    bool isConcurrentMode = false;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink functionality (sequential, tunnel, non-secure)
*
* This test verifies the basic decode functionality in tunnel mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 009@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_Tunnel_NonSecure)
{
    const char *testName = "Decode";
    bool isSecureMode = false;
    bool isConcurrentMode = false;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (sequential, tunnel, non-secure)
*
* This test verifies the flush scenario in tunnel mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 010@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_Tunnel_NonSecure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = false;
    bool isConcurrentMode = false;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (sequential, tunnel, non-secure)
*
* This test verifies the EOS scenario in tunnel mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 011@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_Tunnel_NonSecure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = false;
    bool isConcurrentMode = false;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (sequential, tunnel, non-secure)
*
* This test verifies the underflow scenario in tunnel mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 012@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_Tunnel_NonSecure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = false;
    bool isConcurrentMode = false;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink functionality (sequential, tunnel, secure)
*
* This test verifies the basic decode functionality in tunnel and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 013@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_Tunnel_Secure)
{
    const char *testName = "Decode";
    bool isSecureMode = true;
    bool isConcurrentMode = false;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (sequential, tunnel, secure)
*
* This test verifies the flush scenario in tunnel and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 014@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_Tunnel_Secure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = true;
    bool isConcurrentMode = false;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (sequential, tunnel, secure)
*
* This test verifies the EOS scenario in tunnel and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 015@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_Tunnel_Secure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = true;
    bool isConcurrentMode = false;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (sequential, tunnel, secure)
*
* This test verifies the underflow scenario in tunnel and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 016@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Sequential_Tunnel_Secure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = true;
    bool isConcurrentMode = false;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink functionality (concurrent, non-tunnel, non-secure)
*
* This test verifies concurrent decode functionality.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 017@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_NonTunnel_NonSecure)
{
    const char *testName = "Decode";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (concurrent, non-tunnel, non-secure)
*
* This test verifies the flush scenario in concurrent mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 018@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_NonTunnel_NonSecure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (concurrent, non-tunnel, non-secure)
*
* This test verifies the EOS scenario in concurrent mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 019@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_NonTunnel_NonSecure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (concurrent, non-tunnel, non-secure)
*
* This test verifies the underflow scenario in concurrent mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 020@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_NonTunnel_NonSecure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink functionality (concurrent, non-tunnel, secure)
*
* This test verifies concurrent decode functionality in secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 021@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_NonTunnel_Secure)
{
    const char *testName = "Decode";
    bool isSecureMode = true;
    bool isConcurrentMode = true;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (concurrent, non-tunnel, secure)
*
* This test verifies the flush scenario in concurrent and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 022@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_NonTunnel_Secure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = true;
    bool isConcurrentMode = true;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (concurrent, non-tunnel, secure)
*
* This test verifies the EOS scenario in concurrent and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 023@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_NonTunnel_Secure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = true;
    bool isConcurrentMode = true;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (concurrent, non-tunnel, secure)
*
* This test verifies the underflow scenario in concurrent and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 024@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_NonTunnel_Secure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = true;
    bool isConcurrentMode = true;
    bool isTunnel = false;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink functionality (concurrent, tunnel, non-secure)
*
* This test verifies concurrent decode functionality in tunnel mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 025@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_Tunnel_NonSecure)
{
    const char *testName = "Decode";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (concurrent, tunnel, non-secure)
*
* This test verifies the flush scenario in concurrent tunnel mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 026@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_Tunnel_NonSecure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (concurrent, tunnel, non-secure)
*
* This test verifies the EOS scenario in concurrent tunnel mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 027@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_Tunnel_NonSecure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (concurrent, tunnel, non-secure)
*
* This test verifies the underflow scenario in concurrent tunnel mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 028@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_Tunnel_NonSecure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink functionality (concurrent, tunnel, secure)
*
* This test verifies concurrent decode functionality in tunnel and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 029@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_Tunnel_Secure)
{
    const char *testName = "Decode";
    bool isSecureMode = true;
    bool isConcurrentMode = true;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (concurrent, tunnel, secure)
*
* This test verifies the flush scenario in concurrent tunnel and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 030@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_Tunnel_Secure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = true;
    bool isConcurrentMode = true;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (concurrent, tunnel, secure)
*
* This test verifies the EOS scenario in concurrent tunnel and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 031@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_Tunnel_Secure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = true;
    bool isConcurrentMode = true;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (concurrent, tunnel, secure)
*
* This test verifies the underflow scenario in concurrent tunnel and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 032@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, Sink_Concurrent_Tunnel_Secure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = true;
    bool isConcurrentMode = true;
    bool isTunnel = true;

    runSingleProcessTest(testName, isSecureMode, isTunnel, isConcurrentMode);
}

/** @} */ // End of AUDIO_SINK_HALTEST L3 File
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK Module
/** @} */ // End of HPK