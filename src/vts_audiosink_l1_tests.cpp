
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
 * @defgroup AUDIO_SINK_HALTEST_L1 Audio Sink HAL Tests L1 File
 * @{
 * @parblock
 *
 * ### L1 Test Cases for Audio Sink HAL :
 *
 * This module includes Level 1 functional tests (success and failure scenarios).
 * This is to ensure that the API meets the operational requirements of the module across all vendors.
 *
 * **Pre-Conditions:** @n
 * **Dependencies:** None@n
 *
 * @endparblock
 *
 */

/**
 * @file vts_audiosink_l1_tests.c
 *
 */

#include <ut_kvp_profile.h>
#include <ut.h>
#include <ut_log.h>
#include <ut_control_plane.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/Status.h>
#include <utils/String16.h>
#include <stdio.h>
#include <vector>
#include <optional>
#include <thread>
#include <chrono>
#include <mutex>
#include <map>
#include <condition_variable>
#include <algorithm>
#include <unordered_set>


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
    {audiosink::Property::DOLBY_ATMOS_LOCK, 0},
    {audiosink::Property::DOLBY_ATMOS_LOCK, 1},
};

static std::multimap<audiosink::Property, int32_t> invalidIntPropertyReady =
{
    {audiosink::Property::REFERENCE_LEVEL, -100},
    {audiosink::Property::MIXER_ID, -2},
    {audiosink::Property::AV_SOURCE , int32_t(static_cast<int32_t>(AVSource::AUTO))-1},
    {audiosink::Property::AV_SOURCE , int32_t(static_cast<int32_t>(AVSource::COMPOSITE_5 ))+1},
    {audiosink::Property::DOLBY_ATMOS_LOCK, -1},
};

static std::multimap<audiosink::Property, int32_t> setIntPropertyReadonly =
{
    {audiosink::Property::RESOURCE_ID, 1},
    {audiosink::Property::RENDER_LATENCY_NS , 2},
    {audiosink::Property::QUEUE_DEPTH_NS , 3},
};

/* Test class registration with UT_Core */
UT_ADD_TEST_TO_GROUP(VTS_L1_AudioSink, UT_TESTS_L1);

extern sp<IAudioSinkManager> gAudioSinkManager;

class VTS_L1_AudioSink : public UTCore
{
public:
    VTS_L1_AudioSink() : UTCore() {}

    ~VTS_L1_AudioSink() override = default;

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

UT_status_t vts_audiosink_getAudioSinkService()
{
    gAudioSinkManager = android::interface_cast<IAudioSinkManager>(
            defaultServiceManager()->getService(String16(IAudioSinkManager::serviceName().c_str())));
    if (gAudioSinkManager == nullptr)
    {
        UT_LOG_ERROR("AudioSink Service not available\n");
        return UT_STATUS_FAILURE;
    }
    return UT_STATUS_OK;
}

/**
 * @brief Test the positive scenario of retrieving audio sink IDs
 *
 * This test verifies that the `getAudioSinkIds` function correctly retrieves a list of audio sink IDs and that the returned list is not empty. It ensures that the function call is successful and that the IDs match the expected set of IDs.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 001@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize audioSinkIds vector and status variable | audioSinkIds = empty, status = uninitialized | Variables initialized | Should be successful |
 * | 02 | Log the start of getAudioSinkIds call | None | Log message | Should be successful |
 * | 03 | Call getAudioSinkIds to retrieve audio sink IDs | audioSinkIds = empty | status.isOk() = true, audioSinkIds not empty | Should Pass |
 * | 04 | Log the result of getAudioSinkIds call | status, audioSinkIds | Log message | Should be successful |
 * | 05 | Assert that the status is OK | status.isOk() = true | Assertion passes | Should Pass |
 * | 06 | Assert that audioSinkIds is not empty | audioSinkIds not empty | Assertion passes | Should Pass |
 * | 07 | Initialize expectedIds set with expected audio sink IDs | expectedIds = set of expected IDs | expectedIds initialized | Should be successful |
 * | 08 | Verify each ID in audioSinkIds is in expectedIds | audioSinkIds, expectedIds | Assertion passes for each ID | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, getAudioSinkIds_pos)
{
    std::vector<IAudioSink::Id> initialaudiosinkIds;
    std::vector<IAudioSink::Id> secondaudiosinkIds;
    std::vector<int32_t> sinkId;
    std::unordered_set<int> sinkId_from_profile;
    size_t expected_count = 0;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(initialaudiosinkIds);

    for (const auto& obj : initialaudiosinkIds)
    {
        sinkId.push_back(obj.value);
    }

    /* Get the Audio sink IDs second time */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(secondaudiosinkIds);

    UT_ASSERT_EQUAL(initialaudiosinkIds.size(), secondaudiosinkIds.size());

    for (size_t i = 0; i < initialaudiosinkIds.size(); ++i)
    {
        UT_ASSERT_EQUAL(initialaudiosinkIds[i].value, secondaudiosinkIds[i].value);
    }

    const auto& obj = gAudiosinks_hfp;

    /* checking the number of audio sink ids from the api getAudioSinkIds()
       equal to the number of audio sink ids from HAL FEATURE PROFILE  */
    UT_ASSERT_EQUAL(initialaudiosinkIds.size(), obj[0].ids.size());
}

/**
 * @brief Test the retrieval of platform capabilities from the AudioSinkManager service.
 *
 * This test verifies the functionality of the getPlatformCapabilities method in the AudioSinkManager service. It checks whether the method correctly retrieves the platform capabilities and matches the expected values for various attributes such as supportsLowLatency, systemMixerSampleRateHz, systemMixerPCMFormat, and supportsPlanarFormat.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 002@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Verify that the AudioSinkManager service is not null | gAudioSinkManagerService = nullptr | Should not be null | Should be successful |
 * | 02 | Call getPlatformCapabilities method on AudioSinkManager service | platformCapabilities = uninitialized | status.isOk() = true | Should Pass |
 * | 03 | Check supportsLowLatency attribute against expected value | platformCapabilities.supportsLowLatency = value | platformCapabilities.supportsLowLatency = expectedSupportsLowLatency | Should Pass |
 * | 04 | Check systemMixerSampleRateHz attribute against expected value | platformCapabilities.systemMixerSampleRateHz = value | platformCapabilities.systemMixerSampleRateHz = expectedSystemMixerSampleRateHz | Should Pass |
 * | 05 | Check systemMixerPCMFormat attribute against expected value | platformCapabilities.systemMixerPCMFormat = value | platformCapabilities.systemMixerPCMFormat = expectedSystemMixerPCMFormat | Should Pass |
 * | 06 | Check supportsPlanarFormat attribute against expected value | platformCapabilities.supportsPlanarFormat = value | platformCapabilities.supportsPlanarFormat = expectedSupportsPlanarFormat | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, getPlatformCapabilities_pos)
{
    PlatformCapabilities platformCapabilities;

    AUDIOSINK_GETPLATFORMCAPABILITIES(platformCapabilities);

    /* Log platform capabilities for debugging */
    UT_LOG_INFO("lowlatency: %s", UT_Control_GetMapString(bool_mapTable, platformCapabilities.supportsLowLatency));
    UT_LOG_INFO("samplerate: %d", platformCapabilities.systemMixerSampleRateHz);
    UT_LOG_INFO("mixer_pcmformat: %d", platformCapabilities.systemMixerPCMFormat);
    UT_LOG_INFO("supportsPlanarFormat: %s", UT_Control_GetMapString(bool_mapTable, platformCapabilities.supportsPlanarFormat));

    /* validate platform capabilites using hfp profile*/
    vts_audiosink_helper_validateCapabilities(platformCapabilities);
}


/**
 * @brief Test the positive scenario of retrieving an audio sink using a valid audio sink ID.
 *
 * This test verifies the functionality of the getAudioSink API by providing a valid audio sink ID and checking if the audio sink is retrieved successfully. The test ensures that the API returns a successful status and a non-null audio sink object.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 003@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * |01|Retrieve the initial audio sink IDs|None|Should retrieve a non-empty list of IDs|Should Pass|
 * |02|Loop through each sink in the sink list and get sink instance|id, audiosink|Status is OK and audiosink is not NULL|Should Pass|
 * |03|Invoke getAudioSink again with same id; check status|id, audiosink|Status is OK and audiosink is not NULL|Should Pass|
 * |04|Compare RESOURCE_ID from both invocations|resource_id_1, resource_id_2|RESOURCE_IDs are equal|Should Pass|
 */
UT_ADD_TEST(VTS_L1_AudioSink, getAudioSink_pos)
{
    sp<IAudioSink> audiosink_1;
    sp<IAudioSink> audiosink_2;
    IAudioSink::Id id ;
    std::vector<IAudioSink::Id> sinkIds;
    std::optional<PropertyValue> resource_id_1;
    std::optional<PropertyValue> resource_id_2;
    audiosink::Property property = audiosink::Property::RESOURCE_ID;

    /* Get the Audio decoder IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        Status status;

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audiosink_1);
        /* Get the resource ID */
        AUDIOSINK_GET_PROPERTY_SUCCESS(audiosink_1, property, resource_id_1);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audiosink_2);

        /* Get the resource ID */
        AUDIOSINK_GET_PROPERTY_SUCCESS(audiosink_2, property, resource_id_2);

        int32_t _resource_id_1 = resource_id_1->value->get<PropertyValue::Value::Tag::intValue>();
        int32_t _resource_id_2 = resource_id_2->value->get<PropertyValue::Value::Tag::intValue>();

        UT_ASSERT_EQUAL(_resource_id_1, _resource_id_2);
    }

}

/**
 * @brief Test the behavior of getAudioSink with invalid audio sink IDs.
 *
 * This test verifies that the getAudioSink function correctly handles invalid audio sink IDs by returning an error status and a null audio sink object. It ensures that the function does not accept IDs outside the valid range.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 004@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve audio sink IDs from the service and ensure they are not empty. | None | Status is OK, audioSinkIds is not empty | Should be successful |
 * | 02 | Sort the audio sink IDs to determine the least and highest values. | None | Sorted list of audioSinkIds | Should be successful |
 * | 03 | Test getAudioSink with an invalid ID (least value - 1). | input: audioSinkId = leastValue - 1, output: audioSink = nullptr | Status is not OK, audioSink is nullptr | Should Fail |
 * | 04 | Test getAudioSink with an invalid ID (highest value + 1). | input: audioSinkId = highestValue + 1, output: audioSink = nullptr | Status is not OK, audioSink is nullptr | Should Fail |
 */
UT_ADD_TEST(VTS_L1_AudioSink, getAudioSink_neg)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    std::vector<int> sinkId_from_profile;
    IAudioSink::Id invalidID;

    /* Get the Audio decoder IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    if(sinkIds.size() > 1)
        std::sort(sinkIds.begin(), sinkIds.end());

    invalidID = sinkIds.front();
    invalidID.value -= 1;
    AUDIOSINK_MANAGER_GETAUDIOSINK_FAILURE(invalidID, audioSink, Status::EX_ILLEGAL_ARGUMENT);

    invalidID = sinkIds.back();
    invalidID.value += 1;
    AUDIOSINK_MANAGER_GETAUDIOSINK_FAILURE(invalidID, audioSink, Status::EX_ILLEGAL_ARGUMENT);

    invalidID.value = IAudioSink::Id::UNDEFINED;
    AUDIOSINK_MANAGER_GETAUDIOSINK_FAILURE(invalidID, audioSink, Status::EX_ILLEGAL_ARGUMENT);
}

/**
 * @brief Test the positive scenario for getting audio sink capabilities
 *
 * This test verifies that the getCapabilities function of each audio sink in mAudioSinks
 * returns a valid capabilities object and that the function call is successful.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 005@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mAudioSinks is not empty | mAudioSinks | Should not be empty | Should be successful |
 * | 02 | Iterate over each audio sink in mAudioSinks | mAudioSinks | Should iterate successfully | Should be successful |
 * | 03 | Call getCapabilities on each audio sink | sink, capabilities | status.isOk() should be true, capabilities should not be nullptr | Should Pass |
 * | 04 | Log the result of getCapabilities call | capabilities, status | Log should display capabilities and status | Should be successful |
 * | 05 | Assert that the status is OK | status.isOk() | status.isOk() should be true | Should Pass |
 * | 06 | Assert that capabilities is not nullptr | capabilities | capabilities should not be nullptr | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, getCapabilities_pos)
{
    audiosink::Capabilities capabilities;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sinks IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        /* Get Capabilities of Audio sink */
        AUDIODSINK_GETCAPABILITIES(audioSink, capabilities);
    }
}

/**
 * @brief Test the retrieval of various properties from AudioSinkBase.
 *
 * This test verifies the ability to retrieve different properties from the AudioSinkBase
 * and checks the correctness of the returned values. It ensures that the properties are
 * correctly fetched and validated against expected results.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 006@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve RESOURCE_ID Property | property = RESOURCE_ID | status.isOk() = true, aidlReturn.has_value() = true, aidlReturn->value >= 0 | Should Pass |
 * | 02 | Retrieve REFERENCE_LEVEL Property in READY State | property = REFERENCE_LEVEL | status.isOk() = true, aidlReturn.has_value() = true, aidlReturn->value = -31 | Should Pass |
 * | 03 | Retrieve RENDER_LATENCY_NS Property | property = RENDER_LATENCY_NS | status.isOk() = true, aidlReturn.has_value() = true, aidlReturn->value > 0 | Should Pass |
 * | 04 | Retrieve QUEUE_DEPTH_NS Property | property = QUEUE_DEPTH_NS | status.isOk() = true, aidlReturn.has_value() = true, aidlReturn->value > 0 | Should Pass |
 * | 05 | Retrieve MIXER_ID Property in READY State | property = MIXER_ID | status.isOk() = true, aidlReturn.has_value() = true, aidlReturn->value = 0 | Should Pass |
 * | 06 | Retrieve AV_SOURCE Property in READY State | property = AV_SOURCE | status.isOk() = true, aidlReturn.has_value() = true, aidlReturn->value = 0 | Should Pass |
 * | 07 | Retrieve DOLBY_ATMOS_LOCK Property in READY State | property = DOLBY_ATMOS_LOCK | status.isOk() = true, aidlReturn.has_value() = true, aidlReturn->value = 0 or 1 | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, getProperty_pos)
{

    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    std::vector<audiosink::Property> testProperties = {audiosink::Property::RESOURCE_ID, audiosink::Property::REFERENCE_LEVEL, audiosink::Property::RENDER_LATENCY_NS, audiosink::Property::QUEUE_DEPTH_NS,
                                            audiosink::Property::MIXER_ID , audiosink::Property::AV_SOURCE , audiosink::Property::DOLBY_ATMOS_LOCK};
    ::android::binder::Status status;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        /* Get RESOURCE_ID */
        for (auto property : testProperties)
        {
            std::optional<PropertyValue> propertyValue;
            AUDIOSINK_GET_PROPERTY_SUCCESS(audioSink, property, propertyValue);
            UT_ASSERT_TRUE(vts_audiosink_helper_validatePropertyValue(_sinkId.value, propertyValue, property));
        }
    }
}

/**
 * @brief Test the negative scenario for the getProperty function of AudioSink
 *
 * This test verifies the behavior of the getProperty function when an invalid property is requested. It ensures that the function returns an error status and no property value.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 007@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mAudioSinks is not empty | mAudioSinks | Should not be empty | Should be successful |
 * | 02 | Iterate over each sink in mAudioSinks and call getProperty with invalid property | property = -1, aidl_return = null | status.isOk() = false, aidl_return.has_value() = false | Should Fail |
 */
UT_ADD_TEST(VTS_L1_AudioSink, getProperty_neg)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    std::optional<PropertyValue> propertyValue;
    int32_t firstValue = static_cast<int32_t>(audiosink::Property::RESOURCE_ID);
    audiosink::Property Invalidproperty = static_cast<audiosink::Property>(firstValue - 1);
    ::android::binder::Status status;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Audio decoder handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        AUDIOSINK_GET_PROPERTY_FAILURE(audioSink, Invalidproperty, propertyValue, Status::EX_ILLEGAL_ARGUMENT);
    }
}

/**
 * @brief Test the positive setting of properties on an audio sink in the READY state.
 *
 * This test verifies that various properties can be set on an audio sink when it is in the READY state.
 * It ensures that the properties REFERENCE_LEVEL, MIXER_ID, AV_SOURCE, and DOLBY_ATMOS_LOCK can be set
 * successfully and that the audio sink responds with the expected status and return values.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 008@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the audio sink is in the READY state and retrieve the first audio sink from the list. | mAudioSinks = non-empty, audioSink = mAudioSinks.front() | audioSink != nullptr | Should be successful |
 * | 02 | Set REFERENCE_LEVEL to -31 in the READY state using setProperty. | property = REFERENCE_LEVEL, propertyValue = -31, aidl_return = false | status.isOk() = true, aidl_return = true | Should Pass |
 * | 03 | Set MIXER_ID to 0 in the READY state using setProperty. | property = MIXER_ID, propertyValue = 0, aidl_return = false | status.isOk() = true, aidl_return = true | Should Pass |
 * | 04 | Set AV_SOURCE to 0 in the READY state using setProperty. | property = AV_SOURCE, propertyValue = 0, aidl_return = false | status.isOk() = true, aidl_return = true | Should Pass |
 * | 05 | Enable DOLBY_ATMOS_LOCK by setting it to 1 in the READY state using setProperty. | property = DOLBY_ATMOS_LOCK, propertyValue = 1, aidl_return = false | status.isOk() = true, aidl_return = true | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, setProperty_pos)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    ::android::binder::Status status;
    bool aidl_return = false;
    bool closed = false;

   /* Get the Audio sink IDs */
   AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds )
    {
        android::sp<IAudioSinkController> audioSinkController = nullptr;
        android::sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        /* set the properties which can be set after Ready */
        AUDIOSINK_SETPROPERTY_LIST_SUCCESS(audioSink, validIntPropertyReady, intValue);

        /* Start the sink */
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        /* Stop the sink */
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        /* set the properties which can be set after Ready */
        AUDIOSINK_SETPROPERTY_LIST_SUCCESS(audioSink, validIntPropertyReady, intValue);

        /* Close the sink */
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the negative scenarios for setting properties on an audio sink.
 *
 * This test verifies the behavior of the audio sink when attempting to set properties with invalid values or in incorrect states. It ensures that the audio sink does not accept invalid property settings and returns the expected failure status.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 009@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the audio sink is in the READY state by opening all controllers. | None | Controllers should be in READY state | Should be successful |
 * | 02 | Attempt to set REFERENCE_LEVEL to an invalid value (-100) in READY state. | property = REFERENCE_LEVEL, propertyValue = -100, aidl_return = false | aidl_return should be false | Should Fail |
 * | 03 | Ensure the audio sink is not in the READY state by starting all controllers. | None | Controllers should not be in READY state | Should be successful |
 * | 04 | Attempt to set MIXER_ID in a non-READY state. | property = MIXER_ID, propertyValue = 0, aidl_return = false | aidl_return should be false | Should Fail |
 * | 05 | Ensure the audio sink is in the READY state by stopping all controllers. | None | Controllers should be in READY state | Should be successful |
 * | 06 | Attempt to set AV_SOURCE to an invalid value (-1) in READY state. | property = AV_SOURCE, propertyValue = -1, aidl_return = false | aidl_return should be false | Should Fail |
 * | 07 | Ensure the audio sink is not in the READY state by starting all controllers. | None | Controllers should not be in READY state | Should be successful |
 * | 08 | Attempt to disable DOLBY_ATMOS_LOCK in a non-READY state. | property = DOLBY_ATMOS_LOCK, propertyValue = 0, aidl_return = false | aidl_return should be false | Should Fail |
 * | 09 | Attempt to set READONLY property RENDER_LATENCY_NS. | property = RENDER_LATENCY_NS, propertyValue = 0, aidl_return = false | aidl_return should be false | Should Fail |
 */
UT_ADD_TEST(VTS_L1_AudioSink, setProperty_neg)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    ::android::binder::Status status;
    bool aidl_return = false;
    bool closed = false;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds )
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        /*set valid properties in invalid state*/
        AUDIOSINK_SETPROPERTY_LIST_FAILURE(audioSink, validIntPropertyReady, intValue, Status::EX_ILLEGAL_STATE);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        /* set the properties which can be set after Ready */
        AUDIOSINK_SETPROPERTY_LIST_FAILURE(audioSink, invalidIntPropertyReady, intValue, Status::EX_ILLEGAL_ARGUMENT);

        /* Start the sink */
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        /* set the properties in invalid state */
        AUDIOSINK_SETPROPERTY_LIST_FAILURE(audioSink, invalidIntPropertyReady, intValue, Status::EX_ILLEGAL_STATE);

        /* set the read only properties*/
        AUDIOSINK_SETPROPERTY_LIST_FAILURE(audioSink, setIntPropertyReadonly, intValue, Status::EX_ILLEGAL_ARGUMENT);

        /* Stop the sink */
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        /* set the properties which can be set after Ready */
        AUDIOSINK_SETPROPERTY_LIST_FAILURE(audioSink, invalidIntPropertyReady, intValue, Status::EX_ILLEGAL_ARGUMENT);

        /* Close the sink */
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);

        /* set the properties in invalid state */
        AUDIOSINK_SETPROPERTY_LIST_FAILURE(audioSink, invalidIntPropertyReady, intValue, Status::EX_ILLEGAL_STATE);
    }
}

/**
 * @brief Test the positive scenario of getting the state of AudioSink
 *
 * This test verifies that the getState function of AudioSink returns a valid state and the operation is successful.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 010@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Iterate over each AudioSink in mAudioSinks | mAudioSinks | Should be successful | |
 * | 02 | Call getState on the current AudioSink and store the result in currentState | currentState = OUT | Should Pass | Should Pass |
 * | 03 | Log the result of getState and the status | currentState, status | Should be successful | |
 * | 04 | Assert that the status is OK | status.isOk() | Should Pass | Should Pass |
 * | 05 | Define a list of valid states | validStates | Should be successful | |
 * | 06 | Assert that the currentState is within the valid states | currentState in validStates | Should Pass | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, getState_pos)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    ::android::binder::Status status;
    State state = State::UNKNOWN;
    bool aidl_return = false;
    bool closed = false;

   /* Get the Audio sink IDs */
   AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds )
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        /* get the state of an audio sink */
        AUDIOSINK_GET_STATE(audioSink, state);

        /* Start the sink */
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        /* get the state of an audio sink */
        AUDIOSINK_GET_STATE(audioSink, state);

        /* Stop the sink */
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        /* get the state of an audio sink */
        AUDIOSINK_GET_STATE(audioSink, state);

        /* Close the sink */
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);

        /* get the state of an audio sink */
        AUDIOSINK_GET_STATE(audioSink, state);
    }
}

/**
 * @brief Test the positive scenario of opening an audio sink.
 *
 * This test verifies the successful opening of an audio sink using a controller listener. It checks if the audio sink controller is correctly initialized and the status returned is OK.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 011@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize the controller listener object | controllerListener = new AudioSinkControllerListenerImpl | Object should be created successfully | Should be successful |
 * | 02 | Call the open function with the controller listener and audio sink controller | IN: controllerListener, OUT: audioSinkController | Status should be OK, audioSinkController should not be nullptr | Should Pass |
 * | 03 | Log the result of the open function call | audioSinkController.get(), status.isOk(), status.exceptionCode() | Log should show correct status and controller address | Should be successful |
 * | 04 | Assert that the status is OK | status.isOk() | Assertion should pass | Should Pass |
 * | 05 | Assert that the audio sink controller is not nullptr | audioSinkController != nullptr | Assertion should pass | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, open_pos)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    ::android::binder::Status status;

   /* Get the Audio sink IDs */
   AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds )
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        /* Close the sink */
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);

    }
}

/**
 * @brief Test the negative scenarios for the AudioSink open function
 *
 * This test evaluates the behavior of the AudioSink open function when provided with invalid inputs or called in an invalid state. It ensures that the function handles these scenarios gracefully by returning appropriate error statuses and not altering the state inappropriately.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 012@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize the test environment and prepare the AudioSink for testing | None | None | Should be successful |
 * | 02 | Call open with a null listener to test negative scenario | audioSinkControllerListener = nullptr, controller = nullptr | status.isOk() = false, controller = nullptr | Should Fail |
 * | 03 | Create a valid AudioSinkControllerListenerImpl instance | mControllerListener = new AudioSinkControllerListenerImpl | None | Should be successful |
 * | 04 | Call open with a valid listener to establish a valid state | audioSinkControllerListener = valid, controller = valid | status.isOk() = true, controller != nullptr | Should Pass |
 * | 05 | Call open again with the same valid listener to test negative scenario | audioSinkControllerListener = valid, secondController = nullptr | status.isOk() = false, secondController = nullptr | Should Fail |
 * | 06 | Close the controller to clean up the test environment | controller = valid, closed = false | status.isOk() = true, closed = true | Done by Pre-requisite TearDown function |
 */
UT_ADD_TEST(VTS_L1_AudioSink, open_neg)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    ::android::binder::Status status;
    State state = State::UNKNOWN;
    bool aidl_return = false;
    bool closed = false;

   /* Get the Audio sink IDs */
   AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds )
    {
        sp<IAudioSinkController> audioSinkController = nullptr;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);
        android::sp<IAudioSinkController> audioSinkController_nullptr = nullptr;

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //send Null listener
        AUDIOSINK_OPEN_FAILURE(audioSink, nullptr, audioSinkController_nullptr, Status::EX_NULL_POINTER);

        //open audio sink
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        //open audio sink again
        AUDIOSINK_OPEN_FAILURE(audioSink, controllerListener, audioSinkController_nullptr, Status::EX_ILLEGAL_STATE);

        /* Close the sink */
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);

    }
}

/**
 * @brief Test the positive scenario of closing an AudioSink.
 *
 * This test verifies the successful closure of an AudioSink when the controller is in the READY state. It ensures that the close operation returns a successful status and the closed flag is set to true.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 013@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the AudioSink is in READY state by starting the controller | controller = mControllers[i], status = controller->start() | status.isOk() should be true | Should be successful |
 * | 02 | Log before calling the close API | controller = mControllers[i] | Log should be successful | Should be successful |
 * | 03 | Call the close API to close the AudioSink | controller = mControllers[i], closed = false | status.isOk() should be true, closed should be true | Should Pass |
 * | 04 | Log the result of the close API | controller = mControllers[i], closed = true, status = status | Log should be successful | Should be successful |
 * | 05 | Verify the close operation was successful | status.isOk() = true, closed = true | status.isOk() should be true, closed should be true | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, close_pos)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    ::android::binder::Status status;
    State state = State::UNKNOWN;
    bool aidl_return = false;
    bool closed = false;

   /* Get the Audio sink IDs */
   AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds )
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        /* Close the sink */
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);

    }
}

/**
 * @brief Test the behavior of the AudioSink close function with various invalid inputs and states.
 *
 * This test evaluates the `close` method of the AudioSink class under different invalid scenarios to ensure that it handles errors gracefully and returns the expected results. The test covers cases such as null controller instances, invalid controller instances, and closing from an invalid state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 014@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Attempt to close with a null AudioSinkController instance | audioSinkController = nullptr, _aidl_return = false | _aidl_return = false, status.isOk() = false | Should Fail |
 * | 02 | Attempt to close with an invalid AudioSinkController instance | audioSinkController = invalid, _aidl_return = false | _aidl_return = false, status.isOk() = false | Should Fail |
 * | 03 | Attempt to close with a valid AudioSinkController instance from a valid state | audioSinkController = valid, _aidl_return = false | _aidl_return = true, status.isOk() = true | Should Pass |
 * | 04 | Attempt to close again with a valid AudioSinkController instance from a CLOSED state | audioSinkController = valid, _aidl_return = false | _aidl_return = false, status.isOk() = false | Should Fail |
 */
UT_ADD_TEST(VTS_L1_AudioSink, close_neg)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    ::android::binder::Status status;
    State state = State::UNKNOWN;
    bool aidl_return = false;
    bool closed = false;

   /* Get the Audio sink IDs */
   AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds )
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);
        android::sp<IAudioSinkController> audioSinkController_invalid;

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //open audio sink
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        //send NULL controller
        AUDIOSINK_CLOSE_FAILURE(audioSink, nullptr, Status::EX_NULL_POINTER);

        //send invalid controller
        AUDIOSINK_CLOSE_FAILURE(audioSink, audioSinkController_invalid, Status::EX_NULL_POINTER);

        /* Close the sink */
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);

        /* Close the sink again */
        AUDIOSINK_CLOSE_FAILURE(audioSink, audioSinkController, Status::EX_ILLEGAL_STATE);

    }
}

/**
 * @brief Test the positive scenario of registering an event listener with AudioSink.
 *
 * This test verifies the successful registration of a valid IAudioSinkEventListener instance
 * with the AudioSink. It ensures that the API call returns a successful status and the output
 * parameter indicates success.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 015@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a valid instance of IAudioSinkEventListener | mSinkEventListener = new AudioSinkEventListenerImpl | Instance should be created successfully | Should be successful |
 * | 02 | Log the API call for registerEventListener | None | Log should be generated | Should be successful |
 * | 03 | Call registerEventListener with a valid IAudioSinkEventListener instance | audioSinkEventListener = mSinkEventListener, _aidl_return = false | Status should be OK, _aidl_return should be true | Should Pass |
 * | 04 | Log the result of registerEventListener | None | Log should be generated with status and _aidl_return | Should be successful |
 * | 05 | Verify that the return value indicates success | status.isOk() = true | Assertion should pass | Should Pass |
 * | 06 | Check that the output parameter *_aidl_return is not NULL or empty | aidlReturn = true | Assertion should pass | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, registerEventListener_pos)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    ::android::binder::Status status;

    /* Get the Audio decoder IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        sp<IAudioSinkEventListener> eventListener = new AudioSinkEventListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        /* Register events */
        AUDIOSINK_REGISTER_EVENT_SUCCESS(audioSink, eventListener);

        /* Un-register the events */
        AUDIOSINK_UNREGISTER_EVENT_SUCCESS(audioSink, eventListener);
    }
}

/**
 * @brief Test the registration of an audio sink event listener with various scenarios.
 *
 * This test evaluates the behavior of the registerEventListener function when registering valid and invalid IAudioSinkEventListener instances. It checks the function's response to multiple registrations of the same listener and the registration of a null listener.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 016@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a valid AudioSinkEventListenerImpl instance | mSinkEventListener = new AudioSinkEventListenerImpl | Instance should be created successfully | Should be successful |
 * | 02 | Register the valid listener for the first time | audioSinkEventListener = mSinkEventListener, aidlReturn = false | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 03 | Register the valid listener for the second time | audioSinkEventListener = mSinkEventListener, aidlReturn = false | status.isOk() = true, aidlReturn = false | Should Fail |
 * | 04 | Attempt to register a null listener | audioSinkEventListener = nullptr, aidlReturn = false | status.isOk() = false, aidlReturn = false | Should Fail |
 */
UT_ADD_TEST(VTS_L1_AudioSink, registerEventListener_neg)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    ::android::binder::Status status;

    /* Get the Audio decoder IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        sp<IAudioSinkEventListener> eventListener = new AudioSinkEventListener(_sinkId.value);
        sp<IAudioSinkEventListener> eventListener_nullptr = nullptr;

        /* Get Audio decoder handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        /* Register events with invalid listener */
        AUDIOSINK_REGISTER_EVENT_FAILURE(audioSink, eventListener_nullptr, Status::EX_NULL_POINTER);

        /* Register events */
        AUDIOSINK_REGISTER_EVENT_SUCCESS(audioSink, eventListener);

        /* Register events again */
        AUDIOSINK_REGISTER_EVENT_FAILURE(audioSink, eventListener, Status::EX_ILLEGAL_STATE);

        /* Un-register the events */
        AUDIOSINK_UNREGISTER_EVENT_SUCCESS(audioSink, eventListener);
    }
}

/**
 * @brief Test the positive scenario of unregistering an event listener from AudioSink.
 *
 * This test verifies the successful unregistration of a valid AudioSinkEventListener instance from the AudioSinkManagerService. It ensures that the listener is initially registered and then checks the status and return value after attempting to unregister it.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 017@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a new AudioSinkEventListenerImpl instance and ensure it is valid | mSinkEventListener = new AudioSinkEventListenerImpl | mSinkEventListener should not be nullptr | Should be successful |
 * | 02 | Register the event listener with each AudioSink instance | sink->registerEventListener(mSinkEventListener) | status.isOk() = true | Should be successful |
 * | 03 | Log the call to unregisterEventListener | UT_LOG_INFO("Calling unregisterEventListener...") | mSinkEventListener.get() | Log should be successful | Should be successful |
 * | 04 | Call unregisterEventListener with the valid IAudioSinkEventListener instance | gAudioSinkManagerService->unregisterEventListener(mSinkEventListener, &aidl_return) | aidl_return = false | status.isOk() = true, aidl_return = true | Should Pass |
 * | 05 | Verify the return value is a successful ::android::binder::Status | UT_ASSERT_TRUE(status.isOk()) | status.isOk() = true | Assertion should pass | Should Pass |
 * | 06 | Check the output parameter *_aidl_return to confirm it indicates success | UT_ASSERT_TRUE(aidl_return) | aidl_return = true | Assertion should pass | Should Pass |
 * | 07 | Log the result of unregisterEventListener | UT_LOG_INFO("Result unregisterEventListener...") | mSinkEventListener.get(), aidl_return | Log should be successful | Should be successful |
 */
UT_ADD_TEST(VTS_L1_AudioSink, unregisterEventListener_pos)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    ::android::binder::Status status;
    bool _aidl_return;

    /* Get the Audio decoder IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        sp<IAudioSinkEventListener> eventListener = new AudioSinkEventListener(_sinkId.value);

        /* Get Audio decoder handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        /* Register events */
        AUDIOSINK_REGISTER_EVENT_SUCCESS(audioSink, eventListener);

        /* Un-register the events */
        AUDIOSINK_UNREGISTER_EVENT_SUCCESS(audioSink, eventListener);

    }
}

/**
 * @brief Test the behavior of unregistering an event listener in various scenarios.
 *
 * This test evaluates the behavior of the `unregisterEventListener` function when attempting to unregister an event listener that is not registered, and when unregistering an event listener that has already been unregistered.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 018@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a valid instance of IAudioSinkEventListener | listener = new AudioSinkEventListenerImpl() | Instance should be created successfully | Should be successful |
 * | 02 | Attempt to unregister an event listener that is not registered | listener = 0x[listener address], aidl_return = false | aidl_return = false, status.isOk() = false | Should Fail |
 * | 03 | Register the event listener | listener = 0x[listener address] | status.isOk() = true | Should be successful |
 * | 04 | Unregister the event listener for the first time | listener = 0x[listener address], aidl_return = false | aidl_return = true, status.isOk() = true | Should Pass |
 * | 05 | Attempt to unregister the event listener again | listener = 0x[listener address], aidl_return = false | aidl_return = false, status.isOk() = false | Should Fail |
 */
UT_ADD_TEST(VTS_L1_AudioSink, unregisterEventListener_neg)
{
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    ::android::binder::Status status;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        sp<IAudioSinkEventListener> eventListener = new AudioSinkEventListener(_sinkId.value);
        sp<IAudioSinkEventListener> eventListener_nullptr = nullptr;

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        /* Un-register the events */
        AUDIOSINK_UNREGISTER_EVENT_FAILURE(audioSink, eventListener, Status::EX_ILLEGAL_STATE);

        /* Register events */
        AUDIOSINK_REGISTER_EVENT_SUCCESS(audioSink, eventListener);

        /* Un-register the events with nullptr*/
        AUDIOSINK_UNREGISTER_EVENT_FAILURE(audioSink, eventListener_nullptr, Status::EX_NULL_POINTER);

        /* Un-register the events */
        AUDIOSINK_UNREGISTER_EVENT_SUCCESS(audioSink, eventListener);

        /* Un-register the events again */
        AUDIOSINK_UNREGISTER_EVENT_FAILURE(audioSink, eventListener, Status::EX_ILLEGAL_STATE);
    }
}

/**
 * @brief Test the positive scenario of setting an audio decoder in AudioSinkController
 *
 * This test verifies that the AudioSinkController can successfully set a valid audio decoder ID.
 * It ensures that the controller is in the READY state and retrieves valid audio decoder IDs
 * using the configuration YAML APIs. The test then calls setAudioDecoder with each valid ID
 * and checks that the operation is successful.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 019@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the resource is in the State::READY state | mControllerListener->mNewState = READY | State should be READY | Should be successful |
 * | 02 | Retrieve valid audio decoder IDs using the configuration YAML APIs | decoderIds | decoderIds should not be empty | Should be successful |
 * | 03 | Call setAudioDecoder with each valid ID | audioDecoderId = decoderId.value, _aidl_return = false | _aidl_return should be true, status should be OK | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, setAudioDecoder_pos)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    std::vector<int> decoderIds;

    std::string audiodecoder_count_path = "IAudioDecoderManager.IAudioDecoder";
    size_t expectedCount = UT_KVP_PROFILE_GET_LIST_COUNT(audiodecoder_count_path.c_str());

    for (size_t i = 0; i < expectedCount; ++i){
        std::string audiodecoderidpath = "IAudioDecoderManager.IAudioDecoder."+std::to_string(i) + ".id";
        int32_t expectedIdValue = UT_KVP_PROFILE_GET_UINT32(audiodecoderidpath.c_str());
        decoderIds.push_back(expectedIdValue);
    }

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    // Ensure the resource is in the State::READY state
      for(const auto& _sinkId : sinkIds)
      {
        sp<IAudioSinkController> audioSinkController = nullptr;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        for (const auto& _decoderId : decoderIds) {
                AUDIOSINK_SETAUDIODECODER_SUCCESS(_decoderId, audioSinkController);
        }
        /* Close the sink */
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the behavior of setAudioDecoder with invalid and valid IDs in different states
 *
 * This test verifies the behavior of the setAudioDecoder function when provided with invalid audio decoder IDs and when the resource is not in the READY state. It ensures that the function handles these scenarios correctly by returning appropriate status and results.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 020@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve all available audio decoder IDs and ensure the list is not empty | None | List should not be empty | Should be successful |
 * | 02 | Sort the audio decoder IDs and calculate invalid low and high IDs | None | Invalid IDs calculated | Should be successful |
 * | 03 | Ensure the resource is in the READY state | None | State should be READY | Should be successful |
 * | 04 | Test setAudioDecoder with invalid low ID | audioDecoderId = invalidLowId, aidlReturn = false | aidlReturn should be false, status should be OK | Should Fail |
 * | 05 | Test setAudioDecoder with invalid high ID | audioDecoderId = invalidHighId, aidlReturn = false | aidlReturn should be false, status should be OK | Should Fail |
 * | 06 | Transition resource to STARTED state | None | State should be STARTED | Should be successful |
 * | 07 | Retrieve a valid audio decoder ID | None | Valid ID should be retrieved | Should be successful |
 * | 08 | Test setAudioDecoder with valid ID in STARTED state | audioDecoderId = validId, aidlReturn = false | status.exceptionCode should be EX_ILLEGAL_STATE, aidlReturn should be false | Should Fail |
 * | 09 | Transition resource back to READY state | None | State should be READY | Should be successful |
 */
UT_ADD_TEST(VTS_L1_AudioSink, setAudioDecoder_neg)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;
    std::vector<int> decoderIds;

    std::string audiodecoder_count_path = "IAudioDecoderManager.IAudioDecoder";
    size_t expectedCount = UT_KVP_PROFILE_GET_LIST_COUNT(audiodecoder_count_path.c_str());

    // Test Case 3: Invalid Audio Decoder ID
    std::vector<uint32_t> audioDecoderIds;
    for (size_t i = 0; i<expectedCount ; ++i)
    {
        std::string audiodecoderid_path = "IAudioDecoderManager.IAudioDecoder."+std::to_string(i)+".id";
        uint32_t id = UT_KVP_PROFILE_GET_UINT32(audiodecoderid_path.c_str());
        audioDecoderIds.push_back(id);
    }
    UT_ASSERT_FALSE(audioDecoderIds.empty());

    std::sort(audioDecoderIds.begin(), audioDecoderIds.end());
    int32_t invalidLowId = audioDecoderIds.front() - 1;
    int32_t invalidHighId = audioDecoderIds.back() + 1;
    int32_t validid = audioDecoderIds.front();

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

       // Test with invalid ID (least value -1)
       AUDIOSINK_SETAUDIODECODER_FAILURE(invalidLowId, audioSinkController, Status::EX_ILLEGAL_ARGUMENT);

        // Test with invalid ID (highest value +1)
        AUDIOSINK_SETAUDIODECODER_FAILURE(invalidHighId, audioSinkController, Status::EX_ILLEGAL_ARGUMENT);

        // Test Case 4: Resource Not in READY State
        // Transition to a non-READY state (e.g., STARTED)
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        // Test with valid ID
        AUDIOSINK_SETAUDIODECODER_FAILURE(validid, audioSinkController, Status::EX_ILLEGAL_STATE);

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }

}

/**
 * @brief Test the functionality of getting and setting audio decoders in AudioSinkController.
 *
 * This test verifies the ability to set and retrieve audio decoder IDs using the AudioSinkController.
 * It ensures that the audio sink can be opened and started, and that valid audio decoder IDs can be
 * set and retrieved successfully.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 021@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the audio sink is in the READY state by opening it. | mControllerListener, opened = false | status.isOk() = true, opened = true | Should be successful |
 * | 02 | Retrieve valid audio decoder IDs using the configuration YAML APIs. | decoderIds | status.isOk() = true, decoderIds not empty | Should be successful |
 * | 03 | Set the audio decoder using a valid decoder ID. | decoderId, setResult = false | status.isOk() = true, setResult = true | Should Pass |
 * | 04 | Get the audio decoder and verify it matches the set ID. | audioDecoder | status.isOk() = true, audioDecoder.value = decoderId.value | Should Pass |
 * | 05 | Ensure the audio sink is in the STARTED state by starting it. | started = false | status.isOk() = true, started = true | Should be successful |
 * | 06 | Set the audio decoder again using a valid decoder ID. | decoderId, setResult = false | status.isOk() = true, setResult = true | Should Pass |
 * | 07 | Get the audio decoder again and verify it matches the set ID. | audioDecoder | status.isOk() = true, audioDecoder.value = decoderId.value | Should Pass |
 */
 UT_ADD_TEST(VTS_L1_AudioSink, getAudioDecoder_pos)
 {
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    ::com::rdk::hal::audiodecoder::IAudioDecoder::Id decoderId;
    ::com::rdk::hal::audiodecoder::IAudioDecoder::Id audiodecoderId;
    sp<IAudioSink> audioSink;
    std::vector<int> decoderIds;

    std::string audiodecoder_count_path = "IAudioDecoderManager.IAudioDecoder";
    size_t expectedCount = UT_KVP_PROFILE_GET_LIST_COUNT(audiodecoder_count_path.c_str());

    for (size_t i = 0; i < expectedCount; ++i){
        std::string audiodecoderidpath = "IAudioDecoderManager.IAudioDecoder."+std::to_string(i) + ".id";
        int32_t expectedIdValue = UT_KVP_PROFILE_GET_UINT32(audiodecoderidpath.c_str());
        decoderIds.push_back(expectedIdValue);
    }

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    // Ensure the resource is in the State::READY state
      for(const auto& _sinkId : sinkIds)
      {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        for (const auto& _decoderId : decoderIds) {
                decoderId.value = _decoderId;
                AUDIOSINK_SETAUDIODECODER_SUCCESS(_decoderId, audioSinkController);
                AUDIOSINK_GETAUDIODECODER_SUCCESS(audiodecoderId, audioSinkController);
                UT_ASSERT_EQUAL(decoderId.value , audiodecoderId.value);
        }
        /* Close the sink */
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the functionality of getting and setting audio decoders in AudioSinkController.
 *
 * This test verifies the ability to set and retrieve audio decoder IDs using the AudioSinkController.
 * It ensures that the audio sink can be opened and started, and that valid audio decoder IDs can be
 * set and retrieved successfully.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 021@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the audio sink is in the READY state by opening it. | mControllerListener, opened = false | status.isOk() = true, opened = true | Should be successful |
 * | 02 | Retrieve valid audio decoder IDs using the configuration YAML APIs. | decoderIds | status.isOk() = true, decoderIds not empty | Should be successful |
 * | 03 | Set the audio decoder using a valid decoder ID. | decoderId, setResult = false | status.isOk() = true, setResult = true | Should Pass |
 * | 04 | Get the audio decoder and verify it matches the set ID. | audioDecoder | status.isOk() = true, audioDecoder.value = decoderId.value | Should Pass |
 * | 05 | Ensure the audio sink is in the STARTED state by starting it. | started = false | status.isOk() = true, started = true | Should be successful |
 * | 06 | Set the audio decoder again using a valid decoder ID. | decoderId, setResult = false | status.isOk() = true, setResult = true | Should Pass |
 * | 07 | Get the audio decoder again and verify it matches the set ID. | audioDecoder | status.isOk() = true, audioDecoder.value = decoderId.value | Should Pass |
 */
// Cannot test the scenario as per the aidl spec
//UT_ADD_TEST(VTS_L1_AudioSink, audiosinkcontroller_getAudioDecoder_neg)
//{
 //  android::binder::Status status;
  // bool aidl_return = false;
   //std::vector<IAudioSink::Id> sinkIds;
   //::com::rdk::hal::audiodecoder::IAudioDecoder::Id audiodecoderId;
   //sp<IAudioSink> audioSink;

      /* Get the Audio sink IDs */
   //AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

   // Ensure the resource is in the State::READY state
     //for(const auto& _sinkId : sinkIds)
     //{
       // sp<IAudioSinkController> audioSinkController;
        //sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

       /* Get Audio sink handle */
       //AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

       //call get audio decoder in non ready state
       //AUDIOSINK_GETAUDIODECODER_FAILURE(audiodecoderId, audioSinkController, Status::EX_ILLEGAL_ARGUMENT);

       //set state to READY by invoking `open` (set-up)
       //AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

       //audiodecoderId.value = -1;
       //call get audio decoder with invalid id
       //AUDIOSINK_GETAUDIODECODER_FAILURE(audiodecoderId, audioSinkController, Status::EX_ILLEGAL_ARGUMENT);

       /* Close the sink */
       //AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
   //}
//}

/**
 * @brief Test the positive scenario of starting the AudioSinkController.
 *
 * This test verifies the successful transition of the audio sink controller from the READY state to the STARTED state by invoking the start API. It ensures that the controller is in the correct initial state and checks the expected state transitions and return values.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 022@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the audio sink is in READY state | mNewState = READY | State should be READY | Should be successful |
 * | 02 | Log before calling the start API | None | Log should be successful | Should be successful |
 * | 03 | Call the start API on the controller | None | Status should be OK | Should Pass |
 * | 04 | Log the result of the start API call | status = [isOK:true, exception:none] | Log should be successful | Should be successful |
 * | 05 | Verify the return value is binder::Status::OK | status.isOk() = true | Assertion should pass | Should Pass |
 * | 06 | Verify the state transition to STARTING and then STARTED | mOldState = READY, mNewState = STARTED | State should transition correctly | Should Pass |
 * | 07 | Teardown: Call stop to revert the state | None | Status should be OK, State should be READY | Done by Pre-requisite TearDown function |
 */
UT_ADD_TEST(VTS_L1_AudioSink, start_pos)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        // Transition to a non-READY state (e.g., STARTED)
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the negative scenario of starting the AudioSinkController when already started.
 *
 * This test verifies the behavior of the AudioSinkController when the start() method is called twice consecutively.
 * The first call to start() should transition the controller to the STARTED state, and the second call should fail
 * with an EX_ILLEGAL_STATE exception, indicating that the controller is already in the STARTED state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 023@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Verify mControllers and mAudioSinks are not empty | mControllers.size(), mAudioSinks.size() | Should be successful | Should be successful |
 * | 02 | Call start() on each controller to change to STARTED state | controller[i]->start() | Status is OK | Should be successful |
 * | 03 | Call start() again on each controller | controller[i]->start() | Status is EX_ILLEGAL_STATE | Should Fail |
 * | 04 | Verify the exception code is EX_ILLEGAL_STATE | status.exceptionCode() | EX_ILLEGAL_STATE | Should Fail |
 * | 05 | Call stop() on each controller as teardown | controller[i]->stop() | Status is OK | Done by Pre-requisite TearDown function |
 */
UT_ADD_TEST(VTS_L1_AudioSink, start_neg)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        // Transition to a non-READY state (e.g., STARTED)
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        //start Audio sink again
        AUDIOSINK_START_FAILURE(audioSink, audioSinkController, Status::EX_ILLEGAL_STATE);

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the positive scenario of stopping the AudioSinkController.
 *
 * This test verifies that the AudioSinkController can successfully transition from the STARTED state to the READY state by invoking the stop() API. It ensures that the controller is initially in the STARTED state by calling the start() API and then checks the state transition upon calling stop().
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 024@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers and mAudioSinks are not empty | mControllers.size() > 0, mAudioSinks.size() > 0 | Should be successful | Should be successful |
 * | 02 | Loop through each controller and ensure the audio sink is in the STARTED state by invoking the start() API | controller[i]->start() | status.isOk() = true, mControllerListener->mNewState = STARTED | Should Pass |
 * | 03 | Log the result of the start() API call | status.isOk(), status.exceptionCode() | Log output | Should be successful |
 * | 04 | Call the stop() API to transition to the READY state | controller[i]->stop() | status.isOk() = true, mControllerListener->mNewState = READY | Should Pass |
 * | 05 | Log the result of the stop() API call | status.isOk(), status.exceptionCode() | Log output | Should be successful |
 */
UT_ADD_TEST(VTS_L1_AudioSink, stop_pos)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        // Transition to a non-READY state (e.g., STARTED)
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the behavior of the AudioSinkController when attempting to stop in various states.
 *
 * This test verifies the behavior of the AudioSinkController's stop() method when the audio sink is in different states.
 * It ensures that the stop() method returns the correct exception code when called in an invalid state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 025@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Verify mControllers is not empty | mControllers | Should be successful | Should be successful |
 * | 02 | Ensure the audio sink is in the READY state | mNewState = READY | Should be successful | Should be successful |
 * | 03 | Call stop() in READY state and verify it returns EX_ILLEGAL_STATE | status = controller->stop() | status.isOk() = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 * | 04 | Start the audio sink to move it to STARTED state | status = controller->start() | status.isOk() = true, mNewState = STARTED | Should be successful |
 * | 05 | Call stop() to move to STOPPING and then READY | status = controller->stop() | status.isOk() = true, mNewState = READY | Should be successful |
 * | 06 | Call stop() again in READY state and verify it returns EX_ILLEGAL_STATE | status = controller->stop() | status.isOk() = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 */
UT_ADD_TEST(VTS_L1_AudioSink, stop_neg)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        //Stop the Audio sink in non STARTED state
        AUDIOSINK_STOP_FAILURE(audioSinkController, Status::EX_ILLEGAL_STATE);

        // Transition to a non-READY state (e.g., STARTED)
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        //Stop the Audio sink again
        AUDIOSINK_STOP_FAILURE(audioSinkController, Status::EX_ILLEGAL_STATE);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the negative scenario of queueing an audio frame with an invalid buffer handle.
 *
 * This test verifies the behavior of the AudioSinkController when attempting to queue an audio frame with an invalid buffer handle. It ensures that the system correctly handles invalid input by returning an appropriate error code.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 026@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the audio sink is in the STARTED state by starting each controller. | mControllers.size() = mAudioSinks.size() | Controllers should start successfully | Should be successful |
 * | 02 | Prepare test inputs with an invalid buffer handle. | nsPresentationTime = 1000000, bufferHandle = -9999, metadata = {sourceCodec = PCM, isDolbyAtmos = false, numChannels = 2, sampleRate = 44100, format = S16LE, source = SYSTEM} | Inputs prepared successfully | Should be successful |
 * | 03 | Log the API call with the prepared inputs. | nsPresentationTime = 1000000, bufferHandle = -9999, metadata = {sourceCodec = PCM, isDolbyAtmos = false, numChannels = 2, sampleRate = 44100, format = S16LE, source = SYSTEM} | Log should be successful | Should be successful |
 * | 04 | Call the queueAudioFrame API with the prepared inputs. | nsPresentationTime = 1000000, bufferHandle = -9999, metadata = {sourceCodec = PCM, isDolbyAtmos = false, numChannels = 2, sampleRate = 44100, format = S16LE, source = SYSTEM} | API should return EX_ILLEGAL_ARGUMENT | Should Fail |
 * | 05 | Log the result of the API call. | status = {isOk = false, exception = EX_ILLEGAL_ARGUMENT} | Log should be successful | Should be successful |
 * | 06 | Verify the expected output by checking the exception code. | status.exceptionCode() = EX_ILLEGAL_ARGUMENT | Exception code should be EX_ILLEGAL_ARGUMENT | Should be successful |
 */
UT_ADD_TEST(VTS_L1_AudioSink, queueAudioFrame_neg)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        // Prepare test inputs
        int64_t nsPresentationTime = -10000; // 1 millisecond in nanoseconds
        int64_t bufferHandle = -9999; // Invalid buffer handle
        com::rdk::hal::audiodecoder::FrameMetadata metadata;
        metadata.sourceCodec = com::rdk::hal::audiodecoder::Codec::PCM;
        metadata.isDolbyAtmos = false;
        metadata.numChannels = 2;
        metadata.channelTypes = {com::rdk::hal::audiodecoder::ChannelType::FRONT_LEFT, com::rdk::hal::audiodecoder::ChannelType::FRONT_RIGHT};
        metadata.sampleRate = 44100;
        metadata.format = com::rdk::hal::audiodecoder::PCMFormat::S16LE;
        metadata.planarFormat = false;
        metadata.trimStartNs = 0;
        metadata.trimEndNs = 0;
        metadata.lowLatency = false;
        metadata.endOfStream = false;
        metadata.discontinuity = false;
        metadata.source = AVSource::SYSTEM;

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        // Log the API call in Ready state
        AUDIOSINK_QUEUEAUDIOFRAME_FAILURE(audioSinkController, nsPresentationTime, bufferHandle, metadata, Status::EX_ILLEGAL_STATE);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the positive scenario of flushing the audio sink controller.
 *
 * This test verifies the functionality of the flush operation in the audio sink controller.
 * It ensures that the flush operation can be successfully executed after starting the audio sink
 * and before stopping it, confirming the expected behavior of the audio sink controller.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 027@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the controllers list is not empty | mControllers.empty() = false | Assertion should pass | Should be successful |
 * | 02 | Start the audio sink for each controller | controller->start() | status.isOk() = true | Should Pass |
 * | 03 | Flush the audio sink for each controller | controller->flush() | status.isOk() = true | Should Pass |
 * | 04 | Stop the audio sink for each controller | controller->stop() | status.isOk() = true | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, flush_pos)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        // Transition to a non-READY state (e.g., STARTED)
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

         // Transition to a non-READY state (e.g., STARTED)
         AUDIOSINK_FLUSH_SUCCESS(audioSink, audioSinkController, true);

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the behavior of AudioSinkController when attempting to flush in an invalid state.
 *
 * This test verifies that the flush operation on an AudioSinkController fails with an illegal state exception
 * when the audio sink is not in the STARTED state. The test ensures that the audio sink transitions through
 * the necessary states and checks the expected failure when flush is called in an inappropriate state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 028@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Verify that mControllers and mAudioSinks are not empty | mControllers.size() > 0, mAudioSinks.size() > 0 | Should be successful | Should be successful |
 * | 02 | Start the audio sink to transition to STARTED state | controller = mControllers[i], started = false | status.isOk() = true, started = true | Should be successful |
 * | 03 | Stop the audio sink to transition to READY state | controller = mControllers[i], stopped = false | status.isOk() = true, stopped = true | Should be successful |
 * | 04 | Attempt to flush the audio sink when not in STARTED state | None | status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 */
UT_ADD_TEST(VTS_L1_AudioSink, flush_neg)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        // Transition to a non-READY state (e.g., STARTED)
        AUDIOSINK_FLUSH_FAILURE(audioSinkController, Status::EX_ILLEGAL_STATE);

        // Transition to a non-READY state (e.g., STARTED)
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        // Call flush after stop
        AUDIOSINK_FLUSH_FAILURE(audioSinkController, Status::EX_ILLEGAL_STATE);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the functionality of setting and getting volume in AudioSinkController.
 *
 * This test verifies the ability of the AudioSinkController to correctly set and retrieve the volume settings. It ensures that the volume can be set and retrieved accurately in different states of the audio sink.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 029@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the audio sink is in State::READY | mNewState = READY | State should be READY | Should be successful |
 * | 02 | Set volume using setVolume function | volume = 1.0, muted = false | status.isOk() = true, volumeSetResult = true | Should Pass |
 * | 03 | Get volume using getVolume function | volumeGet | status.isOk() = true, volumeGet.volume = 1.0, volumeGet.muted = false | Should Pass |
 * | 04 | Start the audio sink to change state to State::STARTED | mNewState = STARTED | State should be STARTED | Should be successful |
 * | 05 | Set volume again using setVolume function | volume = 1.0, muted = false | status.isOk() = true, volumeSetResult = true | Should Pass |
 * | 06 | Get volume again using getVolume function | volumeGet | status.isOk() = true, volumeGet.volume = 1.0, volumeGet.muted = false | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, getVolume_pos)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        //Set volume
        Volume volumeSet;
        volumeSet.volume = 1.0; // Using the specified input
        volumeSet.muted = false; // Using the specified input
        AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, volumeSet);

        // Get volume
        Volume volumeGet;
        AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, volumeGet);

        //comparing set and get volume
        UT_ASSERT_EQUAL(volumeSet.volume, volumeGet.volume);

        // Ensure the audio sink is in State::STARTED
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        volumeSet.volume = 0.5; // Using the specified input
        volumeSet.muted = true; // Using the specified input
        // Set volume again
        AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, volumeSet);

        // Get volume again
        AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, volumeGet);

        //comparing set and get volume
        UT_ASSERT_EQUAL(volumeSet.volume, volumeGet.volume);

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the functionality of setting and getting volume in AudioSinkController.
 *
 * This test verifies the ability of the AudioSinkController to correctly set and retrieve the volume settings. It ensures that the volume can be set and retrieved accurately in different states of the audio sink.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 029@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the audio sink is in State::READY | mNewState = READY | State should be READY | Should be successful |
 * | 02 | Set volume using setVolume function | volume = 1.0, muted = false | status.isOk() = true, volumeSetResult = true | Should Pass |
 * | 03 | Get volume using getVolume function | volumeGet | status.isOk() = true, volumeGet.volume = 1.0, volumeGet.muted = false | Should Pass |
 * | 04 | Start the audio sink to change state to State::STARTED | mNewState = STARTED | State should be STARTED | Should be successful |
 * | 05 | Set volume again using setVolume function | volume = 1.0, muted = false | status.isOk() = true, volumeSetResult = true | Should Pass |
 * | 06 | Get volume again using getVolume function | volumeGet | status.isOk() = true, volumeGet.volume = 1.0, volumeGet.muted = false | Should Pass |
 */
// Cannot test the scenario as per the aidl spec
//UT_ADD_TEST(VTS_L1_AudioSink, audiosinkcontroller_getVolume_neg)
//{
    //android::binder::Status status;
    //bool aidl_return = false;
    //std::vector<IAudioSink::Id> sinkIds;
    //sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    //AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    //for (const auto& _sinkId : sinkIds)
    //{
        //sp<IAudioSinkController> audioSinkController;
        //sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        //AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //Get the volume in non ready state
        //Get volume
        //Volume volumeGet;
        //AUDIOSINK_GETVOLUME_FAILURE(audioSinkController, volumeGet, Status::EX_ILLEGAL_STATE);

        //set state to READY by invoking `open` (set-up)
        //AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        //close the Audio sink
        //AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);

        //Again Get the volume in non ready state
        //Get volume
        //AUDIOSINK_GETVOLUME_FAILURE(audioSinkController, volumeGet, Status::EX_ILLEGAL_STATE);
    //}
//}

/**
 * @brief Test the functionality of setting volume in different states of AudioSinkController
 *
 * This test verifies the ability to set the volume to maximum and minimum levels in different states of the AudioSinkController. It ensures that the controller behaves correctly when the volume is set in the READY and STARTED states.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 030@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Open the controller and ensure it is in the READY state | mControllerListener = nullptr | status.isOk() = true, mControllerListener->mNewState = READY | Should be successful |
 * | 02 | Set volume to maximum level in READY state | volume = 1.0, muted = false | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 03 | Start the controller and ensure it is in the STARTED state |  | status.isOk() = true, mControllerListener->mNewState = STARTED | Should be successful |
 * | 04 | Set volume to minimum level in STARTED state | volume = 0.0, muted = false | status.isOk() = true, aidlReturn = true | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, setVolume_pos)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        //Set volume
        Volume volumeSet;
        volumeSet.volume = 1.0; // Using the specified input
        volumeSet.muted = false; // Using the specified input
        AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, volumeSet);

        // Get volume
        Volume volumeGet;
        AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, volumeGet);

        //comparing set and get volume
        UT_ASSERT_EQUAL(volumeSet.volume, volumeGet.volume);

        // Ensure the audio sink is in State::STARTED
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        volumeSet.volume = 0.0; // Using the specified input
        volumeSet.muted = false; // Using the specified input
        // Set volume again
        AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, volumeSet);

        // Get volume again
        AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, volumeGet);

        //comparing set and get volume
        UT_ASSERT_EQUAL(volumeSet.volume, volumeGet.volume);

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the behavior of the AudioSinkController when setting invalid volume levels.
 *
 * This test verifies that the AudioSinkController correctly handles invalid volume levels by returning an error status and ensuring the operation does not succeed. It tests both above maximum and below minimum volume levels.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 031@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers is not empty | mControllers | Should be successful |  |
 * | 02 | Set Volume with Invalid Volume Level (Above Maximum) | volume = 1.1, muted = false, _aidl_return = false | status.isOk() = false, _aidl_return = false | Should Fail |
 * | 03 | Set Volume with Invalid Volume Level (Below Minimum) | volume = -0.1, muted = false, _aidl_return = false | status.isOk() = false, _aidl_return = false | Should Fail |
 */
UT_ADD_TEST(VTS_L1_AudioSink, setVolume_neg)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        //setting invalid volume
        Volume volumeSet;
        volumeSet.volume = 1.1; // Using the specified input
        volumeSet.muted = false; // Using the specified input
        AUDIOSINK_SETVOLUME_FAILURE(audioSinkController, volumeSet, Status::EX_ILLEGAL_ARGUMENT);

        // Ensure the audio sink is in State::STARTED
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        volumeSet.volume = -0.1; // Using the specified input
        volumeSet.muted = false; // Using the specified input
        // Set invalid volume below minimum volume again
        AUDIOSINK_SETVOLUME_FAILURE(audioSinkController, volumeSet, Status::EX_ILLEGAL_ARGUMENT);

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the positive scenario of setting volume ramp in AudioSinkController.
 *
 * This test verifies the functionality of setting a volume ramp in the AudioSinkController. It ensures that the controller can successfully start and set the volume ramp with various parameters without errors.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 032@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Start all controllers to ensure they are in State::STARTED | None | Controllers should start successfully | Should be successful |
 * | 02 | Iterate over targetVolumes and volumeRamps, and call setVolumeRamp on each controller | targetVolume = 0.0, overMs = 5000, volumeRamp = LINEAR | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 03 | Iterate over targetVolumes and volumeRamps, and call setVolumeRamp on each controller | targetVolume = 0.5, overMs = 5000, volumeRamp = LINEAR | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 04 | Iterate over targetVolumes and volumeRamps, and call setVolumeRamp on each controller | targetVolume = 1.0, overMs = 5000, volumeRamp = LINEAR | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 05 | Iterate over targetVolumes and volumeRamps, and call setVolumeRamp on each controller | targetVolume = 0.0, overMs = 5000, volumeRamp = IN_CUBIC | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 06 | Iterate over targetVolumes and volumeRamps, and call setVolumeRamp on each controller | targetVolume = 0.5, overMs = 5000, volumeRamp = IN_CUBIC | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 07 | Iterate over targetVolumes and volumeRamps, and call setVolumeRamp on each controller | targetVolume = 1.0, overMs = 5000, volumeRamp = IN_CUBIC | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 08 | Iterate over targetVolumes and volumeRamps, and call setVolumeRamp on each controller | targetVolume = 0.0, overMs = 5000, volumeRamp = OUT_CUBIC | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 09 | Iterate over targetVolumes and volumeRamps, and call setVolumeRamp on each controller | targetVolume = 0.5, overMs = 5000, volumeRamp = OUT_CUBIC | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 10 | Iterate over targetVolumes and volumeRamps, and call setVolumeRamp on each controller | targetVolume = 1.0, overMs = 5000, volumeRamp = OUT_CUBIC | status.isOk() = true, aidlReturn = true | Should Pass |
 */
UT_ADD_TEST(VTS_L1_AudioSink, setVolumeRamp_pos)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        // Ensure the audio sink is in State::STARTED
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        // Iterate over targetVolume, overMs, and volumeRamp
        std::vector<double> targetVolumes = {0.0, 0.5, 1.0};
        int32_t overMs = 5000;
        std::vector<VolumeRamp> volumeRamps = {VolumeRamp::LINEAR, VolumeRamp::IN_CUBIC, VolumeRamp::OUT_CUBIC};

        for (auto& targetVolume : targetVolumes) {
            for (auto& volumeRamp : volumeRamps) {
                AUDIOSINK_SETVOLUMERAMP_SUCCESS(audioSinkController, targetVolume, overMs,volumeRamp);
                // Add a delay to approximate the ramp duration
                std::this_thread::sleep_for(std::chrono::milliseconds(overMs+500));
            }
        }

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);
    }
}

/**
 * @brief Test the negative scenarios for setting volume ramp in AudioSinkController.
 *
 * This test evaluates the behavior of the setVolumeRamp function when provided with invalid inputs such as negative target volume, zero duration, and incorrect state. It ensures that the function handles these cases gracefully by returning appropriate error statuses.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 033@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure all controllers are started | None | Controllers should be in State::STARTED | Should be successful |
 * | 02 | Call setVolumeRamp with negative target volume | targetVolume = -0.5, overMs = 5000, volumeRamp = LINEAR | aidlReturn = false, status.isOk() = false | Should Fail |
 * | 03 | Call setVolumeRamp with zero duration | targetVolume = 0.7, overMs = 0, volumeRamp = LINEAR | aidlReturn = false, status.isOk() = false | Should Fail |
 * | 04 | Ensure all controllers are stopped | None | Controllers should NOT be in State::STARTED | Should be successful |
 * | 05 | Call setVolumeRamp with invalid state | targetVolume = 0.6, overMs = 4000, volumeRamp = LINEAR | aidlReturn = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 */
UT_ADD_TEST(VTS_L1_AudioSink, setVolumeRamp_neg)
{
    android::binder::Status status;
    bool aidl_return = false;
    std::vector<IAudioSink::Id> sinkIds;
    sp<IAudioSink> audioSink;

    /* Get the Audio sink IDs */
    AUDIOSINK_MANAGER_GETAUDIOSINKIDS(sinkIds);

    for (const auto& _sinkId : sinkIds)
    {
        sp<IAudioSinkController> audioSinkController;
        sp<IAudioSinkControllerListener> controllerListener = new AudioSinkControllerListener(_sinkId.value);

        /* Get Audio sink handle */
        AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink);

        //set state to READY by invoking `open` (set-up)
        AUDIOSINK_OPEN_SUCCESS(audioSink, controllerListener, audioSinkController, true);

        // Ensure the audio sink is in State::STARTED
        AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, true);

        // invalid target Volume Negative
        double targetVolume = -0.5;
        int32_t overMs = 5000;
        VolumeRamp volumeRampLinear = VolumeRamp::LINEAR;
        AUDIOSINK_SETVOLUMERAMP_FAILURE(audioSinkController, targetVolume, overMs, volumeRampLinear, Status::EX_ILLEGAL_ARGUMENT);

        //  Invalid Duration (Zero Milliseconds)
        targetVolume = 0.7;
        overMs = 0;
        AUDIOSINK_SETVOLUMERAMP_FAILURE(audioSinkController, targetVolume, overMs, volumeRampLinear, Status::EX_ILLEGAL_ARGUMENT);

        //Stop the Audio sink
        AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, true);

        //Invalid State (Not Started)
        overMs = 4000;
        AUDIOSINK_SETVOLUMERAMP_FAILURE(audioSinkController, targetVolume, overMs, volumeRampLinear, Status::EX_ILLEGAL_STATE);

        //close the Audio sink
        AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, true);

    }
}

/** @} */ // End of AUDIO_SINK_HALTEST L1 File
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK Module
/** @} */ // End of HPK
