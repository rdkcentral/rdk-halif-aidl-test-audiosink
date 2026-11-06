/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
 * @defgroup AUDIO_SINK_HALTEST_L2 Audio Sink HAL Tests L2 File
 * @{
 * @parblock
 *
 * ### L2 Test Cases for Audio Sink HAL :
 *
 * This module includes Level 2 functional tests (success and failure scenarios).
 * This is to ensure that the API meets the operational requirements of the module across all vendors.
 *
 * **Pre-Conditions:** @n
 * **Dependencies:** None@n
 *
 * @endparblock
 *
 */

/**
 * @file vts_audiosink_l2_tests.c
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
#include "vts_audiodecoder_parse_testconfig.h"
#include "vts_audiodecoder.h"
#include "vts_audiosink_helper_functions.h"
#include "vts_audiosink_parse_hfp.h"
#include "vts_audiosink.h"

static std::multimap<audiosink::Property, int32_t> validIntPropertyReady =
{
    {audiosink::Property::REFERENCE_LEVEL, -31},
    {audiosink::Property::MIXER_ID , 0},
    {audiosink::Property::AV_SOURCE , int32_t(static_cast<int32_t>(AVSource::AUTO))},
    {audiosink::Property::AV_SOURCE, int32_t(static_cast<int32_t>(AVSource::UNKNOWN))},
    {audiosink::Property::AV_SOURCE, int32_t(static_cast<int32_t>(AVSource::IP))},
    {audiosink::Property::AV_SOURCE, int32_t(static_cast<int32_t>(AVSource::TUNER))},
    {audiosink::Property::AV_SOURCE, int32_t(static_cast<int32_t>(AVSource::SYSTEM))},
    {audiosink::Property::AV_SOURCE, int32_t(static_cast<int32_t>(AVSource::HDMI_1))},
};

/* Test class registration with UT_Core */
UT_ADD_TEST_TO_GROUP(VTS_L2_AudioSink, UT_TESTS_L2);

extern sp<IAudioSinkManager> gAudioSinkManager;

class VTS_L2_AudioSink : public UTCore
{
public:
VTS_L2_AudioSink() : UTCore() {}

    ~VTS_L2_AudioSink() override = default;

    public:
        std::vector<std::tuple<sp<IAudioSink>, sp<AudioSinkControllerListener>, sp<AudioSinkEventListener>>> mAudioSinkList;

        void SetUp() override
        {
            if(gAudioSinkManager == NULL)
            {
                UT_FAIL_FATAL("AudioSink Service not available");
            }
            std::vector<IAudioSink::Id> sinkIds;
            /* Get the Audio Sink IDs */
            AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

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

                mAudioSinkList.push_back({audioSink, controlListener, eventListener});
            }
        }

    void checkCompleteStateTransition(sp<IAudioSink> sink, sp<AudioSinkEventListener> eventListener,
                                      sp<AudioSinkControllerListener> controlListener,
                                      com::rdk::hal::audiosink::State oldState,
                                      com::rdk::hal::audiosink::State intermediateState,
                                      com::rdk::hal::audiosink::State newState)
    {
        com::rdk::hal::audiosink::State currentState;

        // Sleep for a short duration to allow state changes to be captured by listeners
        std::this_thread::sleep_for(std::chrono::milliseconds(MAX_FINAL_STATE_WAIT_MS));

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

    void TearDown() override
    {
        for (const auto& [sink, controlListener, eventListener] : mAudioSinkList)
        {
            /* Un-register the events */
            AUDIOSINK_UNREGISTER_EVENT_SUCCESS(sink, eventListener);
        }

        mAudioSinkList.clear();
        //TODO: check right place to call joinThreadPool
        //android::IPCThreadState::self()->joinThreadPool();
    }
};

/**
* @brief Test the read and write operations for audio sink properties
*
* This test verifies the functionality of setting and getting properties for audio sinks.
* It ensures that the setProperty and getProperty methods work correctly and return expected results.
*
* **Test Group ID:** 02@n
* **Test Case ID:** 001@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L2_AudioSink, TestReadWriteProperties)
{

    for (const auto& [audioSink, controlListener, eventListener] : mAudioSinkList)
    {
        std::optional<PropertyValue> resource_id;
        android::sp<IAudioSinkController> audioSinkController = nullptr;

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, ContentType::UNKNOWN, controlListener, audioSinkController, true);

        for (const auto& [property, value] : validIntPropertyReady)
        {
            PropertyValue setValue;
            setValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(value);

            /* Set the property and value*/
            AUDIOSINK_SETPROPERTY_SUCCESS(audioSinkController, property, setValue);

            /* Get the resource ID */
            AUDIOSINK_GET_PROPERTY_SUCCESS(audioSink, property, resource_id);

            int32_t _resource_id = resource_id->value->get<PropertyValue::Value::Tag::intValue>();

            /* Set and Get resouce should be equal*/
            UT_ASSERT_EQUAL(_resource_id, value);

        }
        // Close the audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the state transitions of the AudioSink component
 *
 * This test verifies the correct state transitions of the AudioSink component
 * to ensure it behaves as expected under various conditions.
 *
 * **Test Group ID:** 02@n
 * **Test Case ID:** 002@n
 *
 * **Test Procedure:**
 * Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
 */
UT_ADD_TEST(VTS_L2_AudioSink, TestStateTransition)
{
    for (const auto& [audioSink, controlListener, eventListener] : mAudioSinkList)
    {
        android::sp<IAudioSinkController> audioSinkController = nullptr;

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, ContentType::UNKNOWN, controlListener, audioSinkController, false);
        checkCompleteStateTransition(audioSink, eventListener, controlListener,
                                     com::rdk::hal::audiosink::State::CLOSED,
                                     com::rdk::hal::audiosink::State::OPENING,
                                     com::rdk::hal::audiosink::State::READY);

        // Start the audio sink
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, false);
        checkCompleteStateTransition(audioSink, eventListener, controlListener,
                                     com::rdk::hal::audiosink::State::READY,
                                     com::rdk::hal::audiosink::State::STARTING,
                                     com::rdk::hal::audiosink::State::STARTED);

        // Flush the audio sink
        AUDIOSINK_FLUSH_SUCCESS(audioSink, audioSinkController, false);
        checkCompleteStateTransition(audioSink, eventListener, controlListener,
                                     com::rdk::hal::audiosink::State::STARTED,
                                     com::rdk::hal::audiosink::State::FLUSHING,
                                     com::rdk::hal::audiosink::State::STARTED);

        // Stop the audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, false);
        checkCompleteStateTransition(audioSink, eventListener, controlListener,
                                     com::rdk::hal::audiosink::State::STARTED,
                                     com::rdk::hal::audiosink::State::STOPPING,
                                     com::rdk::hal::audiosink::State::READY);

        // Close the audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, false);
        checkCompleteStateTransition(audioSink, eventListener, controlListener,
                                     com::rdk::hal::audiosink::State::READY,
                                     com::rdk::hal::audiosink::State::CLOSING,
                                     com::rdk::hal::audiosink::State::CLOSED);
    }
}

/**
* @brief Test the audio sink volume control functionality
*
* This test verifies the ability to set and get the volume on audio sinks.
* It ensures that the volume can be adjusted from 0.0 to 1.0 in increments
* of 0.1 and checks that the set volume matches the retrieved volume.
*
* **Test Group ID:** 02@n
* **Test Case ID:** 003@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L2_AudioSink, TestAudioSinkVolumeControl)
{
    for (const auto& [audioSink, controlListener, eventListener] : mAudioSinkList)
    {
        android::sp<IAudioSinkController> audioSinkController = nullptr;

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, ContentType::UNKNOWN, controlListener, audioSinkController, true);

        // Set and verify volume
        for (double volume = 0.0; volume <= 1.0; volume += 0.1)
        {
            Volume setVol;
            setVol.volume = volume;
            setVol.muted = false;
            AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, setVol);

            Volume currentVol;
            AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, currentVol);

            UT_ASSERT_EQUAL(setVol.volume, currentVol.volume);
            UT_ASSERT_EQUAL(setVol.muted, currentVol.muted);
        }

        // Set and verify volume in started state
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);
        for (double volume = 0.0; volume <= 1.0; volume += 0.1)
        {
            Volume setVol;
            setVol.volume = volume;
            setVol.muted = false;
            AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, setVol);

            Volume currentVol;
            AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, currentVol);

            UT_ASSERT_EQUAL(setVol.volume, currentVol.volume);
            UT_ASSERT_EQUAL(setVol.muted, currentVol.muted);
        }
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        // Close the audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
* @brief Test the mute and unmute operations of the AudioSink component
*
* This test verifies the basic operations of the AudioSink component, including opening,
* setting volume, muting, unmuting, and closing the audio sink.
* It ensures that the state transitions and volume settings are correctly handled.
*
* **Test Group ID:** 02@n
* **Test Case ID:** 004@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L2_AudioSink, TestAudioSinkMuteUnmute)
{
    for (const auto& [audioSink, controlListener, eventListener] : mAudioSinkList)
    {
        android::sp<IAudioSinkController> audioSinkController = nullptr;

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, ContentType::UNKNOWN, controlListener, audioSinkController, true);

        // Set and verify volume
        for (double volume = 0.1; volume <= 1.0; volume += 0.1)
        {
            Volume setVol;
            Volume currentVol;
            setVol.volume = volume;
            setVol.muted = true;

            /*Set the volume and mute status */
            AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, setVol);

            /*Get the volume and mute status*/
            AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, currentVol);

            /*Verify set and get volume should be equal*/
            UT_ASSERT_EQUAL(setVol.volume, currentVol.volume);

            /*Verify set and get mute status should be equal*/
            UT_ASSERT_EQUAL(setVol.muted, currentVol.muted);

            /*set the mute state to unmute*/
            setVol.muted = false;

            /*Set the volume and mute status */
            AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, setVol);

            /*Get the volume and mute status*/
            AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, currentVol);

            /*Verify set and get volume should be equal*/
            UT_ASSERT_EQUAL(setVol.volume, currentVol.volume);

            /*Verify set and get mute status should be equal*/
            UT_ASSERT_EQUAL(setVol.muted, currentVol.muted);
        }

        // set and verify volume in started state
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);
        for (double volume = 0.1; volume <= 1.0; volume += 0.1)
        {
            Volume setVol;
            Volume currentVol;
            setVol.volume = volume;
            setVol.muted = true;

            /*Set the volume and mute status */
            AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, setVol);

            /*Get the volume and mute status*/
            AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, currentVol);

            /*Verify set and get volume should be equal*/
            UT_ASSERT_EQUAL(setVol.volume, currentVol.volume);

            /*Verify set and get mute status should be equal*/
            UT_ASSERT_EQUAL(setVol.muted, currentVol.muted);

            /*set the mute state to unmute*/
            setVol.muted = false;

            /*Set the volume and mute status */
            AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, setVol);

            /*Get the volume and mute status*/
            AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, currentVol);

            /*Verify set and get volume should be equal*/
            UT_ASSERT_EQUAL(setVol.volume, currentVol.volume);

            /*Verify set and get mute status should be equal*/
            UT_ASSERT_EQUAL(setVol.muted, currentVol.muted);
        }
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        // Close the audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the Set VolumeRamp functionality of the AudioSink component
 *
 * This test verifies the setting the volume ramp is successful.
 *
 * **Test Group ID:** 02@n
 * **Test Case ID:** 005@n
 *
 * **Test Procedure:**
 * Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
 */
UT_ADD_TEST(VTS_L2_AudioSink, TestAudioSinkVolumeRamp)
{
    for (const auto& [audioSink, controlListener, eventListener] : mAudioSinkList)
    {
        Volume currentVol;
        android::sp<IAudioSinkController> audioSinkController = nullptr;

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, ContentType::UNKNOWN, controlListener, audioSinkController, true);

        // Start the audio sink
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        // Iterate over targetVolume, overMs, and volumeRamp
        std::vector<double> targetVolumes = {0.0, 0.5, 1.0};
        std::vector<double> initVolumes = {0.0, 0.5, 1.0};
        int32_t overMs = 1000;
        std::vector<VolumeRamp> volumeRamps = {VolumeRamp::LINEAR, VolumeRamp::IN_CUBIC, VolumeRamp::OUT_CUBIC};

        for(auto& initVolume : initVolumes)
        {
            for (auto& targetVolume : targetVolumes)
            {
                for (auto& volumeRamp : volumeRamps)
                {
                    currentVol.muted = false;
                    currentVol.volume = initVolume; //reset volume to initVolume
                    AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, currentVol);

                    AUDIOSINK_SETVOLUMERAMP_SUCCESS(audioSinkController, targetVolume, overMs,volumeRamp);
                    // Add a delay to approximate the ramp duration
                    std::this_thread::sleep_for(std::chrono::milliseconds(overMs));

                    AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, currentVol);
                    UT_ASSERT_EQUAL(targetVolume, currentVol.volume);
                }
            }
        }

        // Stop the audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        // Close the audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
* @brief Test the Set VolumeRamp functionality of the AudioSink component in Mute condition
*
* This test verifies the setting the volume ramp with mute is successful.
*
* **Test Group ID:** 02@n
* **Test Case ID:** 006@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L2_AudioSink, TestAudioSinkVolumeRampMute)
{
    for (const auto& [audioSink, controlListener, eventListener] : mAudioSinkList)
    {
        Volume currentVol;
        Volume volume;
        android::sp<IAudioSinkController> audioSinkController = nullptr;

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, ContentType::UNKNOWN, controlListener, audioSinkController, true);

        // Start the audio sink
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        // Set volume ramp
        // Iterate over targetVolume, overMs, and volumeRamp
        std::vector<double> targetVolumes = {0.0, 0.5, 1.0};
        int32_t overMs = 1000;
        std::vector<VolumeRamp> volumeRamps = {VolumeRamp::LINEAR, VolumeRamp::IN_CUBIC, VolumeRamp::OUT_CUBIC};

        for (auto& targetVolume : targetVolumes)
        {
            for (auto& volumeRamp : volumeRamps)
            {
                volume.volume = 0.0;
                volume.muted = true;

                /*Set volume mute state to true*/
                AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, volume);

                /*Set Ramp over target volume and volume ramps type */
                AUDIOSINK_SETVOLUMERAMP_SUCCESS(audioSinkController, targetVolume, overMs,volumeRamp);

                // Add a delay to approximate the ramp duration
                std::this_thread::sleep_for(std::chrono::milliseconds(overMs));

                /*Get the volume set and mute status should false*/
                AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, currentVol);
                UT_ASSERT_EQUAL(targetVolume, currentVol.volume);
                UT_ASSERT_EQUAL(currentVol.muted , true);
            }
        }

        currentVol.volume = 0.0; //reset volume to zero
        currentVol.muted = false;
        AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, currentVol);

        // Stop the audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        // Close the audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the Set VolumeRamp functionality of the AudioSink component by invoking it twice
 *
 * This test verifies the setting the volume ramp back to back is successful.
 *
 * **Test Group ID:** 02@n
 * **Test Case ID:** 007@n
 *
 * **Test Procedure:**
 * Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
 */
UT_ADD_TEST(VTS_L2_AudioSink, TestAudioSinkVolumeRampTwice)
{
    for (const auto& [audioSink, controlListener, eventListener] : mAudioSinkList)
    {
        Volume currentVol;
        android::sp<IAudioSinkController> audioSinkController = nullptr;

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, ContentType::UNKNOWN, controlListener, audioSinkController, true);

        // Start the audio sink
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        // Set volume ramp
        // Iterate over targetVolume, overMs, and volumeRamp
        std::vector<double> targetVolumes = {0.0, 0.5, 1.0};
        int32_t overMs = 1000;
        std::vector<VolumeRamp> volumeRamps = {VolumeRamp::LINEAR, VolumeRamp::IN_CUBIC, VolumeRamp::OUT_CUBIC};

        for (auto& targetVolume : targetVolumes)
        {
            for (auto& volumeRamp : volumeRamps)
            {
                currentVol.volume = 0.0; //reset volume to zero
                currentVol.muted = false;
                AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, currentVol);

                /*Set Ramp over target volume and volume ramps type */
                AUDIOSINK_SETVOLUMERAMP_SUCCESS(audioSinkController, targetVolume, overMs,volumeRamp);

                //Immediately set another volume ramp.
                targetVolume = 0.5;
                AUDIOSINK_SETVOLUMERAMP_SUCCESS(audioSinkController, targetVolume, overMs,volumeRamp);

                // Add a delay to approximate the ramp duration
                std::this_thread::sleep_for(std::chrono::milliseconds(overMs));

                /*Get the volume set and mute status should false*/
                AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, currentVol);
                UT_ASSERT_EQUAL(targetVolume, currentVol.volume);
            }
        }

        currentVol.volume = 0.0; //reset volume to zero
        currentVol.muted = false;
        AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, currentVol);

        // Stop the audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        // Close the audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
* @brief Tests the VolumeRamp functionality of AudioSink by verifying state transitions and volume adjustments.
*
* This test verifies the setting the volume ramp IS successful.
* It checks that the volume ramp progresses correctly when the audio sink is stopped during the ramp.
*
* **Test Group ID:** 02@n
* **Test Case ID:** 008@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L2_AudioSink, TestStopAudioSinkVolRampProgress)
{
    for (const auto& [audioSink, controlListener, eventListener] : mAudioSinkList)
    {
        Volume currentVol;
        android::sp<IAudioSinkController> audioSinkController = nullptr;

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, ContentType::UNKNOWN, controlListener, audioSinkController, true);

        // Set volume ramp
        double targetVolume = 0.5;
        int32_t overMs = 10000;
        std::vector<VolumeRamp> volumeRamps = {VolumeRamp::LINEAR, VolumeRamp::IN_CUBIC, VolumeRamp::OUT_CUBIC};

        for (auto& volumeRamp : volumeRamps)
        {
            currentVol.volume = 0.0; //reset volume to zero
            currentVol.muted = false;
            AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, currentVol);

            // Start the audio sink
            AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

            /*Set Ramp over target volume and volume ramps type */
            AUDIOSINK_SETVOLUMERAMP_SUCCESS(audioSinkController, targetVolume, overMs,volumeRamp);

            // Stop the audio sink
            AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

            /*Get the volume and should be equal to the target volume*/
            AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, currentVol);
            UT_ASSERT_EQUAL(targetVolume, currentVol.volume);
        }

        currentVol.volume = 0.0; //reset volume to zero
        currentVol.muted = false;
        AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, currentVol);

        // Close the audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
* @brief Tests the Attach and detach of clock functionality of AudioSink.
*
* This test verifies the attaching and detaching of the clock.
*
* **Test Group ID:** 02@n
* **Test Case ID:** 009@n
*
* **Test Procedure:**
* Refer to UT specification documentation [audioSink_High-Level_TestSpecification.md](../../docs/pages/audioSink_High-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L2_AudioSink, TestClockAttachDetach)
{
    IAVClock::Id testClockId;
    android::sp<IAudioSinkController> audioSinkController = nullptr;

    for (const auto& [audioSink, controlListener, eventListener] : mAudioSinkList)
    {
        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, ContentType::UNKNOWN, controlListener, audioSinkController, true);

        // Attach and detach all available clocks
        for (const auto& id : gAudiosinksHFP.availableClockIds)
        {
            // Attach the clock
            AUDIOSINK_ATTACHCLOCK_SUCCESS(audioSinkController, id);

            // Get the attached clock and verify the ID
            AUDIOSINK_GETCLOCK_SUCCESS(audioSinkController, testClockId);
            UT_ASSERT_EQUAL(testClockId.value, id);

            // Detach the clock
            AUDIOSINK_DETACHCLOCK_SUCCESS(audioSinkController);
        }

        // Transition to a non-READY state (e.g., STARTED)
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        // Attach and detach all available clocks
        for (const auto& id : gAudiosinksHFP.availableClockIds)
        {
            // Attach the clock
            AUDIOSINK_ATTACHCLOCK_SUCCESS(audioSinkController, id);

            // Get the attached clock and verify the ID
            AUDIOSINK_GETCLOCK_SUCCESS(audioSinkController, testClockId);
            UT_ASSERT_EQUAL(testClockId.value, id);

            // Detach the clock
            AUDIOSINK_DETACHCLOCK_SUCCESS(audioSinkController);
        }

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        // Close the audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/** @} */ // End of AUDIO_SINK_HALTEST L2 File
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK Module
/** @} */ // End of HPK
