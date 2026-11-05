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
#include <memory>
#include <tuple>
#include <map>

#include "com/rdk/hal/avbuffer/IAVBuffer.h"
#include "com/rdk/hal/avbuffer/BnAVBufferSpaceListener.h"

#include "vts_audiodecoder_parse_testconfig.h"

#include "vts_audiosink_helper_functions.h"
#include "vts_audiosink_parse_hfp.h"
#include "vts_audiosink.h"

static const char *gModuleName = "audiosink";
const char *gAudioModuleName = "audiodecoder";
static const char *gTestGroup = "L3";

static sp<IAVBuffer> vts_avbuffer_getService()
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

class VTS_L3_AudioSink : public UTCore
{
public:
    VTS_L3_AudioSink() : UTCore(), mAVbufferHelper(nullptr) {}

    ~VTS_L3_AudioSink() override = default;

public:
    std::vector<std::tuple<IAudioSink::Id, sp<IAudioSink>, sp<AudioSinkControllerListener>, sp<AudioSinkEventListener>, audiosink::Capabilities*>> mAudioSinkList;
    std::vector<std::tuple<IAudioDecoder::Id, sp<IAudioDecoder>, sp<AudioDecoderControllerListener>, sp<AudioDecoderEventListener>, audiodecoder::Capabilities*>> mAudioDecoderList;
    sp<IAVBuffer> mAVBufferManager;
    IAVBufferHelper* mAVbufferHelper;

    void SetUp() override
    {
        std::vector<IAudioSink::Id> sinkIds;
        vector<IAudioDecoder::Id> decoderIds;
        if(gAudioSinkManager == NULL)
        {
            UT_FAIL_FATAL("AudioSink Service not available");
        }
        if(gAudioDecoderManager== NULL)
        {
            UT_FAIL_FATAL("AudioDecoder Service not available");
        }

        mAVBufferManager = vts_avbuffer_getService();
        if(mAVBufferManager == NULL)
        {
            UT_FAIL_FATAL("AVBufferManager Service not available");
        }

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
            audiosink::Capabilities *capabilities = new audiosink::Capabilities();

            /* Get Audio decoder handle */
            AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

            /* Register events */
            AUDIOSINK_REGISTER_EVENT_SUCCESS(audioSink, eventListener);

            /* Get Capabilities of Audio Sink */
            AUDIOSINK_GETCAPABILITIES(audioSink, capabilities);

            mAudioSinkList.push_back({_sinkId, audioSink, controlListener, eventListener, capabilities});
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
                                      sp<AudioSinkControllerListener> controlListener, ::com::rdk::hal::audiosink::State oldState,
                                      ::com::rdk::hal::audiosink::State intermediateState, ::com::rdk::hal::audiosink::State newState)
    {
        ::com::rdk::hal::audiosink::State currentState;

        UT_ASSERT_TRUE(eventListener->checkStateTransistion(oldState, intermediateState));
        UT_ASSERT_TRUE(controlListener->checkStateTransistion(oldState, intermediateState));

        eventListener->waitForState(newState, std::chrono::milliseconds(MAX_FINAL_STATE_WAIT_MS));
        controlListener->waitForState(newState, std::chrono::milliseconds(MAX_FINAL_STATE_WAIT_MS));

        AUDIOSINK_GET_STATE(sink, currentState);

        UT_ASSERT_EQUAL(currentState, newState);

        UT_ASSERT_TRUE(eventListener->checkStateTransistion(intermediateState, newState));
        UT_ASSERT_TRUE(controlListener->checkStateTransistion(intermediateState, newState));

        eventListener->resetStateChanges();
        controlListener->resetStateChanges();
    }

    bool isSinkUsable(const IAudioSink::Id& sinkId, audiosink::Capabilities* capabilities, bool secureDecode)
    {
        if (secureDecode && !capabilities->supportsSecure)
            return false;
        return true;
    }

    bool isDecoderUsable(const IAudioDecoder::Id& decoderId,
                         audiodecoder::Capabilities* capabilities,
                         const testConfiguration_t& testConfig,
                         bool secureDecode)
    {
        auto it = std::find_if(gAudiodecoders_hfp.begin(), gAudiodecoders_hfp.end(), [decoderId](const audioDecoder& item) {
            return item.id == decoderId.value;
        });
        if(it == gAudiodecoders_hfp.end())
            return false;
        if (secureDecode && !capabilities->supportsSecure)
            return false;
        auto result = std::find(capabilities->supportedCodecs.begin(), capabilities->supportedCodecs.end(), testConfig.codec);
        if(result == capabilities->supportedCodecs.end())
            return false;
        return true;
    }

    bool runSequentialSink(testConfiguration_t& testConfig, bool secureDecode)
    {
        bool isTestExecuted = false;
        try
        {
            // Find the first usable decoder
            IAudioDecoder::Id decoderId;
            sp<IAudioDecoder> decoderHandle;
            sp<AudioDecoderControllerListener> decoderControlListener;
            sp<AudioDecoderEventListener> decoderEventListener;
            bool foundDecoder = false;
            IAVClock::Id clockId;
            bool isClockInUse = false;


            for (size_t i = 0; i < mAudioDecoderList.size(); ++i)
            {
                const auto& decoderTuple = mAudioDecoderList[i];
                const IAudioDecoder::Id& _decoderId = std::get<0>(decoderTuple);
                if (isDecoderUsable(_decoderId, std::get<4>(decoderTuple), testConfig, secureDecode))
                {
                    decoderId = _decoderId;
                    decoderHandle = std::get<1>(decoderTuple);
                    decoderControlListener = std::get<2>(decoderTuple);
                    decoderEventListener = std::get<3>(decoderTuple);
                    foundDecoder = true;
                    break;
                }
            }

            if (!foundDecoder)
            {
                return false;
            }

            if (gAudiosinksHFP.availableClockIds.size() == 0)
            {
                UT_ASSERT_TRUE(false);
                UT_LOG_WARNING("No available clock found for the given test configuration.");
                return false;
            }

            clockId.value = gAudiosinksHFP.availableClockIds[0];

            for (size_t i = 0; i < mAudioSinkList.size(); ++i)
            {
                const auto& sinkTuple = mAudioSinkList[i];
                const IAudioSink::Id& _sinkId = std::get<0>(sinkTuple);
                HeapMetrics beforeHeapMetrics, afterHeapMetrics;
                bool isSinkAttached = true;

                if (!isSinkUsable(_sinkId, std::get<4>(sinkTuple), secureDecode))
                {
                    UT_LOG_INFO("[SinkID: %d] is not usable for this test configuration, skipping.", _sinkId.value);
                    continue;
                }

                resetDecodeSession(decoderEventListener, decoderControlListener);
                AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, beforeHeapMetrics);

                {
                    AudioDecoderTest decodeTest(decoderId, decoderHandle, decoderControlListener,
                                                decoderEventListener, mAVBufferManager, mAVbufferHelper,
                                                secureDecode, testConfig, isSinkAttached);

                    std::thread decodeThread(&AudioDecoderTest::run, &decodeTest, false);

                    //TODO: remove the isTunnel parameter from AudioSinkTest constructor
                    AudioSinkTest audiosinkTest(_sinkId, std::get<1>(sinkTuple), std::get<2>(sinkTuple), std::get<3>(sinkTuple),
                                                decoderControlListener, mAVBufferManager, decoderId, clockId,
                                                secureDecode, false, testConfig, isClockInUse);
                    audiosinkTest.run();

                    isTestExecuted = true;

                    if (decodeThread.joinable())
                        decodeThread.join();
                }

                AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, afterHeapMetrics);

                // Check if the decoder and sink freed all the buffer pools
                UT_ASSERT_EQUAL(beforeHeapMetrics.secure, afterHeapMetrics.secure);
                UT_ASSERT_EQUAL(beforeHeapMetrics.bytesUsed, afterHeapMetrics.bytesUsed);
                UT_ASSERT_EQUAL(beforeHeapMetrics.bytesTotal, afterHeapMetrics.bytesTotal);
            }
        }
        catch (const std::exception& ex)
        {
            UT_LOG_ERROR("Exception in runSequentialSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
        }
        catch (...)
        {
            UT_LOG_ERROR("Unknown exception in runSequentialSink");
            UT_ASSERT_TRUE(false);
        }
        return isTestExecuted;
    }

    bool runConcurrentSink(testConfiguration_t& testConfig, bool secureDecode)
    {
        bool isTestExecuted = false;
        try
        {
            HeapMetrics beforeHeapMetrics, afterHeapMetrics;
            std::vector<std::tuple<IAudioDecoder::Id, sp<IAudioDecoder>,
                                  sp<AudioDecoderControllerListener>,
                                  sp<AudioDecoderEventListener>,
                                  audiodecoder::Capabilities *>> usableDecoders;
            std::vector<std::tuple<IAudioSink::Id, sp<IAudioSink>, sp<AudioSinkControllerListener>, sp<AudioSinkEventListener>, audiosink::Capabilities *>> usableSinks;
            std::vector<std::thread> decoderThreads;
            std::vector<std::thread> sinkThreads;
            std::vector<std::unique_ptr<AudioDecoderTest>> decoderTests;
            std::vector<std::unique_ptr<AudioSinkTest>> sinkTests;
            bool isSinkAttached = true;

            // Get heap metrics
            AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, beforeHeapMetrics);

            // Collect usable decoders
            for (const auto& entry : mAudioDecoderList)
            {
                const auto& [_decoderId, _dh, _cl, _el, _caps] = entry;
                if (isDecoderUsable(_decoderId, _caps, testConfig, secureDecode))
                    usableDecoders.push_back({_decoderId, _dh, _cl, _el, _caps});
            }

            // Collect usable sinks
            for (const auto& entry : mAudioSinkList)
            {
                const auto& [_sinkId, _, __, ___, _caps] = entry;
                if (isSinkUsable(_sinkId, _caps, secureDecode))
                    usableSinks.push_back({_sinkId, _, __, ___, _caps});
            }

            int32_t availableDecoderCount = usableDecoders.size();
            int32_t availableSinkCount = usableSinks.size();
            int32_t availableClockCount = gAudiosinksHFP.availableClockIds.size();
            int32_t numInstances = std::min({availableDecoderCount, availableSinkCount, availableClockCount});

            if (availableDecoderCount == 0)
            {
                UT_LOG_WARNING("No usable decoder found for the given test configuration, skipping concurrent test.");
                return isTestExecuted;
            }
            if (availableSinkCount == 0)
            {
                UT_LOG_WARNING("No usable sink found for the given test configuration, skipping concurrent test.");
                return isTestExecuted;
            }
            if (availableClockCount == 0)
            {
                UT_LOG_WARNING("No available clock found for the given test configuration, skipping concurrent test.");
                return isTestExecuted;
            }

            // Launch sink threads, pairing with decoder control listeners
            for (int i = 0; i < numInstances; ++i)
            {
                IAVClock::Id clockId;
                bool isClockInUse = false;

                const auto& [sinkId, sinkHandle, controlListener, eventListener, sinkCaps] = usableSinks[i];
                const auto& [_decoderId, _, decoderControlListener, __, ___] = usableDecoders[i];

                clockId.value = gAudiosinksHFP.availableClockIds[i];
                resetDecodeSession(std::get<3>(usableDecoders[i]), decoderControlListener);

                auto sinkTest = std::make_unique<AudioSinkTest>(sinkId, sinkHandle,
                                                                controlListener, eventListener,
                                                                decoderControlListener,
                                                                mAVBufferManager, _decoderId, clockId,
                                                                secureDecode, false, /* TODO: remove false */
                                                                testConfig, isClockInUse);
                sinkThreads.emplace_back(&AudioSinkTest::run, sinkTest.get());
                sinkTests.push_back(std::move(sinkTest));
            }

            // Launch decoder threads
            for (int i = 0; i < numInstances; ++i)
            {
                const auto& [_decoderId, decoderHandle, controlListener, eventListener, capabilities] = usableDecoders[i];
                auto decoderTest = std::make_unique<AudioDecoderTest>(_decoderId, decoderHandle,
                                                                     controlListener, eventListener,
                                                                     mAVBufferManager, mAVbufferHelper,
                                                                     secureDecode, testConfig, isSinkAttached);
                decoderThreads.emplace_back(&AudioDecoderTest::run, decoderTest.get(), false);
                decoderTests.push_back(std::move(decoderTest));
            }

            // Join all threads
            for (auto& t : decoderThreads)
                if (t.joinable()) t.join();
            for (auto& t : sinkThreads)
                if (t.joinable()) t.join();

            isTestExecuted = true;
            // Destroy decoder tests now so their pool destructors fire before the metrics check
            decoderTests.clear();

            // Get heap metrics
            AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, afterHeapMetrics);

            // Check if the decoder freed all the buffer pools
            UT_ASSERT_EQUAL(beforeHeapMetrics.secure, afterHeapMetrics.secure);
            UT_ASSERT_EQUAL(beforeHeapMetrics.bytesUsed, afterHeapMetrics.bytesUsed);
            UT_ASSERT_EQUAL(beforeHeapMetrics.bytesTotal, afterHeapMetrics.bytesTotal);
        }
        catch (const std::exception& ex)
        {
            UT_LOG_ERROR("Exception in runConcurrentSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
        }
        catch (...)
        {
            UT_LOG_ERROR("Unknown exception in runConcurrentSink");
            UT_ASSERT_TRUE(false);
        }
        return isTestExecuted;
    }

    void runSingleProcessTest(const char *testName, bool isSecureMode, bool isConcurrentMode)
    {
        vector<testConfiguration_t> testConfiguration;
        bool isTestExecuted = false;

        vts_audiodecoder_parse_testconfig(gAudioSinkTestConfiguration, gAudioModuleName, gTestGroup, testName, testConfiguration);

        if(isConcurrentMode)
        {
            for(auto& test: testConfiguration)
            {
                if (!runConcurrentSink(test, isSecureMode))
                {
                    continue;
                }
                isTestExecuted = true;
            }
        }
        else
        {
            for(auto& test: testConfiguration)
            {
                if (!runSequentialSink(test, isSecureMode))
                {
                    continue;
                }
                isTestExecuted = true;
            }
        }

        if(isTestExecuted == false)
        {
            UT_LOG_INFO("Skipping test '%s': No valid test configuration executed", testName);
            GTEST_SKIP() << "No valid test configuration executed";
        }
    }

    void TearDown() override
    {
        for (const auto& entry : mAudioDecoderList)
        {
            AUDIODECODER_UNREGISTER_EVENT_SUCCESS(std::get<1>(entry), std::get<3>(entry));
        }
        mAudioDecoderList.clear();

        for (const auto& [_sinkId, sinkHandle, controlListener, eventListener, sinkCaps] : mAudioSinkList)
        {
            /* Un-register the events */
            AUDIOSINK_UNREGISTER_EVENT_SUCCESS(sinkHandle, eventListener);
        }
        mAudioSinkList.clear();

        releaseAVBufferHelperInstance(mAVbufferHelper);

        //TODO: check right place to call joinThreadPool
        //android::IPCThreadState::self()->joinThreadPool();
    }
};

/**
* @brief Test the audio sink functionality (sequential, non-secure)
*
* This test verifies the basic decode functionality.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 001@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Sequential_NonSecure)
{
    const char *testName = "Decode";
    bool isSecureMode = false;
    bool isConcurrentMode = false;

    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (sequential, non-secure)
*
* This test verifies the flush scenario.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 002@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Sequential_NonSecure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = false;
    bool isConcurrentMode = false;

    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (sequential, non-secure)
*
* This test verifies the EOS scenario.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 003@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Sequential_NonSecure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = false;
    bool isConcurrentMode = false;

    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (sequential, non-secure)
*
* This test verifies the underflow scenario.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 004@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Sequential_NonSecure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = false;
    bool isConcurrentMode = false;

    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}
/*Tunnel and secure cases*/
/**
* @brief Test the audio sink functionality (sequential, secure)
*
* This test verifies the basic decode functionality in secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 005@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Sequential_Secure)
{
    const char *testName = "Decode";
    bool isSecureMode = true;
    bool isConcurrentMode = false;

    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (sequential, secure)
*
* This test verifies the flush scenario in secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 006@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Sequential_Secure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = true;
    bool isConcurrentMode = false;
    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (sequential, secure)
*
* This test verifies the EOS scenario in secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 007@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Sequential_Secure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = true;
    bool isConcurrentMode = false;
    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (sequential, secure)
*
* This test verifies the underflow scenario in secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 008@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Sequential_Secure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = true;
    bool isConcurrentMode = false;
    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink functionality (concurrent, non-secure)
*
* This test verifies concurrent decode functionality.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 017@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Concurrent_NonSecure)
{
    const char *testName = "Decode";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (concurrent, non-secure)
*
* This test verifies the flush scenario in concurrent mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 018@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Concurrent_NonSecure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (concurrent, non-secure)
*
* This test verifies the EOS scenario in concurrent mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 019@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Concurrent_NonSecure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (concurrent, non-secure)
*
* This test verifies the underflow scenario in concurrent mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 020@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Concurrent_NonSecure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = false;
    bool isConcurrentMode = true;
    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/*Tunnel and secure cases*/
/**
* @brief Test the audio sink functionality (concurrent, secure)
*
* This test verifies concurrent decode functionality in secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 021@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Concurrent_Secure)
{
    const char *testName = "Decode";
    bool isSecureMode = true;
    bool isConcurrentMode = true;
    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink flush functionality (concurrent, secure)
*
* This test verifies the flush scenario in concurrent and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 022@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Concurrent_Secure_Flush)
{
    const char *testName = "Decode_Flush";
    bool isSecureMode = true;
    bool isConcurrentMode = true;

    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink EOS (End Of Stream) functionality (concurrent, secure)
*
* This test verifies the EOS scenario in concurrent and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 023@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Concurrent_Secure_EOS)
{
    const char *testName = "Decode_EOS";
    bool isSecureMode = true;
    bool isConcurrentMode = true;
    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/**
* @brief Test the audio sink underflow events (concurrent, secure)
*
* This test verifies the underflow scenario in concurrent and secure mode.
*
* **Test Group ID:** 03@n
* **Test Case ID:** 024@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L3_AudioSink, ASink_Concurrent_Secure_Underflow)
{
    const char *testName = "Decode_Underflow";
    bool isSecureMode = true;
    bool isConcurrentMode = true;
    runSingleProcessTest(testName, isSecureMode, isConcurrentMode);
}

/** @} */ // End of AUDIO_SINK_HALTEST L3 File
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK Module
/** @} */ // End of HPK
