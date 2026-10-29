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
 * @file vts_audiosink.h
 *
 */

#ifndef __VTS_AUDIOSINK_H__
#define __VTS_AUDIOSINK_H__

#include <thread>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <inttypes.h>

#include "vts_audiodecoder.h"
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

extern void *gAudioSinkTestConfiguration;
extern sp<IAudioSinkManager> gAudioSinkManager;

#ifdef MAX_TRANSITORY_STATE_WAIT_MS
#undef MAX_TRANSITORY_STATE_WAIT_MS
#endif
#define MAX_TRANSITORY_STATE_WAIT_MS 5

#define MAX_FINAL_STATE_WAIT_MS  500

#define AUDIOSINK_MANAGER_GETAUDIOSINKIDS(audioSinkIds) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getAudioSinkIds(OUT:sinkIds:[])"); \
        status = (gAudioSinkManager)->getAudioSinkIds(&audioSinkIds); \
        UT_LOG_INFO("Result getAudioSinksIds(status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_GETPLATFORMCAPABILITIES(platformCapabilities) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("getPlatformCapabilities(OUT:platformCapabilities[])"); \
        status = (gAudioSinkManager)->getPlatformCapabilities(&platformCapabilities); \
        UT_LOG_INFO("Result getCapabilities(OUT:capabilities), status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getAudioSink(IN:audioSinkId:[%d], OUT:audioSink[])", _sinkId.value); \
        status = (gAudioSinkManager)->getAudioSink(_sinkId, &audioSink); \
        UT_LOG_INFO("Result getAudioSink(IN:_sinkId:[%d], OUT:audioSink[0x%X]), status:[isOK:%s, exception:%s]", \
                                _sinkId.value, audioSink.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_TRUE_FATAL(audioSink.get() != nullptr); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_MANAGER_GETAUDIOSINK_FAILURE(_sinkId, audioSink, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getAudioSink(IN:_sinkId:[%d], OUT:audioSink[])", _sinkId.value); \
        status = (gAudioSinkManager)->getAudioSink(_sinkId, &audioSink); \
        UT_LOG_INFO("Result getAudioSink(IN:_sinkId:[%d], OUT:audioSink[0x%X]), status:[isOK:%s, exception:%s]", \
                                _sinkId.value, audioSink.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        if ((exception) == Status::EX_NONE) { \
            /* Invalid ID is reported via a null return, not a binder exception. */ \
            UT_ASSERT_TRUE(status.isOk()); \
            UT_ASSERT_TRUE(audioSink.get() == nullptr); \
        } else { \
            UT_ASSERT_FALSE(status.isOk()); \
        } \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIODSINK_GETCAPABILITIES(audioSink, capabilities) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getCapabilities(OUT:capabilities)"); \
        status = (audioSink)->getCapabilities(&capabilities); \
        UT_LOG_INFO("Result getCapabilities(OUT:capabilities), status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_NOT_EQUAL(&capabilities, nullptr); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_GET_PROPERTY_SUCCESS(audioSink, property, propertyValue) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getProperty(IN:property[%s], OUT:propertyValue[]", audiosink::toString(property).c_str()); \
        status = (audioSink)->getProperty(property, &propertyValue); \
        switch (propertyValue->value.value().getTag()) \
        { \
            case PropertyValue::Value::Tag::intValue: \
                UT_LOG_INFO("Result getProperty(IN:property[%s], OUT:propertyValue[%d]), status:[isOK:%s, exception:%s]", audiosink::toString(property).c_str(), \
                propertyValue->value->get<PropertyValue::Value::Tag::intValue>(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
                break; \
            case PropertyValue::Value::Tag::stringValue: \
                UT_LOG_INFO("Result getProperty(IN:property[%s], OUT:propertyValue[%s]), status:[isOK:%s, exception:%s]", audiosink::toString(property).c_str(), \
                propertyValue->value->get<PropertyValue::Value::Tag::stringValue>().string(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
                break; \
            default: \
                break; \
        } \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_GET_PROPERTY_FAILURE(audioSink, property, propertyValue, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getProperty(IN:property[%s], OUT:propertyValue[]", audiosink::toString(property).c_str()); \
        status = (audioSink)->getProperty(property, &propertyValue); \
        UT_LOG_INFO("Result getProperty(IN:property[%s], OUT:propertyValue[]), status:[isOK:%s, exception:%s]", audiosink::toString(property).c_str(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_GET_STATE(audioSink, state) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getState(OUT:state[]"); \
        status = (audioSink)->getState(&state); \
    UT_LOG_INFO("Result getState(OUT:state[%s]), status:[isOK:%s, exception:%s]", com::rdk::hal::audiosink::toString(state).c_str(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_WAIT_FOR_STATE(audioSink, desiredState, lastState) \
    do { \
        int32_t sleep_time = 0; \
        do \
        { \
            std::this_thread::sleep_for(std::chrono::milliseconds(MAX_TRANSITORY_STATE_WAIT_MS)); \
            sleep_time += MAX_TRANSITORY_STATE_WAIT_MS; \
            /* Check the state of the sink */ \
            AUDIOSINK_GET_STATE((audioSink), lastState); \
        } while (sleep_time < MAX_FINAL_STATE_WAIT_MS && lastState != desiredState); \
    } while (0)

#define AUDIOSINK_OPEN_SUCCESS(audioSink, contentType, controllerListener, audioSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling open(IN:contentType[%d], IN:controllerListener[0x%p], OUT:audioSinkController[])", static_cast<int32_t>(contentType), (void*)((controllerListener).get())); \
        status = (audioSink)->open(contentType, (controllerListener), &(audioSinkController)); \
        UT_LOG_INFO("Result open(IN:contentType[%d], IN:controllerListener[0x%p], OUT:audioSinkController[0x%p]), status:[isOK:%s, exception:%s]", static_cast<int32_t>(contentType), (void*)((controllerListener).get()),\
            (void*)((audioSinkController).get()),\
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_NOT_EQUAL_FATAL(audioSinkController.get(), nullptr); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
        \
        if (stateCheck) \
        { \
            ::com::rdk::hal::audiosink::State lastState; \
            AUDIOSINK_WAIT_FOR_STATE(audioSink, ::com::rdk::hal::audiosink::State::READY, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::audiosink::State::READY); \
        } \
    } while (0)

#define AUDIOSINK_OPEN_FAILURE(audioSink, contentType, controllerListener, audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling open failure(IN:contentType[%d], IN:controllerListener[N/A], OUT:audioSinkController[])", \
                     static_cast<int32_t>(contentType)); \
        status = (audioSink)->open(contentType, (controllerListener), &(audioSinkController)); \
        UT_LOG_INFO("Result open failure (IN:contentType[%d], IN:controllerListener[N/A]), status:[isOK:%s, exception:%s]", static_cast<int32_t>(contentType),\
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling close(IN:audioSinkController[0x%p], OUT:_aidl_return[])", audioSinkController.get()); \
        status = audioSink->close(audioSinkController, &_aidl_return); \
        UT_LOG_INFO("Result close(IN:audioSinkController[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            audioSinkController.get(), UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_TRUE_FATAL(_aidl_return); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::audiosink::State lastState; \
            AUDIOSINK_WAIT_FOR_STATE(audioSink, ::com::rdk::hal::audiosink::State::CLOSED, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::audiosink::State::CLOSED); \
        } \
    } while (0)

#define AUDIOSINK_CLOSE_FAILURE(audioSink, audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling close(OUT:_aidl_return[])"); \
        status = (audioSink)->close(audioSinkController, &_aidl_return); \
        UT_LOG_INFO("Result close(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_REGISTER_EVENT_SUCCESS(audioSink, eventListener) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = (audioSink)->registerEventListener(eventListener, &_aidl_return); \
        UT_LOG_INFO("Result registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(_aidl_return); \
    } while (0)

#define AUDIOSINK_REGISTER_EVENT_FAILURE(audioSink, eventListener, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_INFO("Calling registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = (audioSink)->registerEventListener(eventListener, &_aidl_return); \
        UT_LOG_INFO("Result registerEventListener(IN:eventListener[0x%p]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        if (exception == Status::EX_NONE) {\
            UT_ASSERT_TRUE(status.isOk()); \
            UT_ASSERT_FALSE(_aidl_return); \
        } \
        else {\
            UT_ASSERT_FALSE(status.isOk()); \
        }\
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_UNREGISTER_EVENT_SUCCESS(audioSink, eventListener) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = (audioSink)->unregisterEventListener(eventListener, &_aidl_return); \
        UT_LOG_INFO("Result unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_UNREGISTER_EVENT_FAILURE(audioSink, eventListener, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_INFO("Calling unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = (audioSink)->unregisterEventListener(eventListener, &_aidl_return); \
        UT_LOG_INFO("Result unregisterEventListener(IN:eventListener[0x%p]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        if (exception == Status::EX_NONE) {\
            UT_ASSERT_TRUE(status.isOk()); \
            UT_ASSERT_FALSE(_aidl_return); \
        } \
        else {\
            UT_ASSERT_FALSE(status.isOk()); \
            UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        }\
    } while (0)

#define AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling start()"); \
        status = (audioSinkController)->start(); \
        UT_LOG_INFO("Result start(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::audiosink::State lastState; \
            AUDIOSINK_WAIT_FOR_STATE(audioSink, ::com::rdk::hal::audiosink::State::STARTED, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::audiosink::State::STARTED); \
        } \
    } while (0)

#define AUDIOSINK_START_FAILURE(audioSink, audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling start()"); \
        status = (audioSinkController)->start(); \
        UT_LOG_INFO("Result start(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling stop()"); \
        status = (audioSinkController)->stop(); \
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::audiosink::State lastState; \
            AUDIOSINK_WAIT_FOR_STATE(audioSink, ::com::rdk::hal::audiosink::State::READY, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::audiosink::State::READY); \
        } \
    } while (0)

#define AUDIOSINK_STOP_FAILURE(audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling stop()"); \
        status = (audioSinkController)->stop(); \
        UT_LOG_INFO("Result stop(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_SETAUDIODECODER_SUCCESS(audioSinkController, decoderId) \
    do { \
        ::android::binder::Status status; \
        ::com::rdk::hal::audiodecoder::IAudioDecoder::Id audioDecoderId; \
        audioDecoderId.value = decoderId; \
        bool aidl_return; \
        UT_LOG_INFO("Calling setAudioDecoder(IN:audioDecoderId:[%d], OUT:_aidl_return[])", audioDecoderId.value); \
        status = (audioSinkController)->setAudioDecoder(audioDecoderId, &aidl_return); \
        UT_LOG_INFO("Result setAudioDecoder(IN:audioDecoderId:[%d], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
                    audioDecoderId.value, aidl_return ? "true" : "false", \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(aidl_return); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_SETAUDIODECODER_FAILURE(audioSinkController, decoderId, exception) \
    do { \
        ::android::binder::Status status; \
        ::com::rdk::hal::audiodecoder::IAudioDecoder::Id audioDecoderId; \
        audioDecoderId.value = decoderId; \
        bool aidl_return = false; \
        UT_LOG_INFO("Calling setAudioDecoder(IN:audioDecoderId:[%d], OUT:_aidl_return[])", audioDecoderId.value); \
        status = (audioSinkController)->setAudioDecoder(audioDecoderId, &aidl_return); \
        UT_LOG_INFO("Result setAudioDecoder(IN:audioDecoderId:[%d], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
                    audioDecoderId.value, aidl_return ? "true" : "false", \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        if ((exception) == Status::EX_NONE) { \
            /* Invalid ID is reported via a false return, not a binder exception. */ \
            UT_ASSERT_TRUE(status.isOk()); \
            UT_ASSERT_FALSE(aidl_return); \
        } else { \
            UT_ASSERT_FALSE(status.isOk()); \
        } \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_GETAUDIODECODER_SUCCESS(audioSinkController, audioDecoderId) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getAudioDecoder(OUT:audioDecoderId:[])"); \
        status = (audioSinkController)->getAudioDecoder(&audioDecoderId); \
        UT_LOG_INFO("Result getAudioDecoder(OUT:audioDecoderId:[%d]), status:[isOK:%s, exception:%s]", \
                    audioDecoderId.value, \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_GETAUDIODECODER_FAILURE(audioSinkController, audioDecoderId, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getAudioDecoder(OUT:audioDecoderId:[])"); \
        status = (audioSinkController)->getAudioDecoder(&audioDecoderId);\
        UT_LOG_INFO("Result getAudioDecoder(), status:[isOK:%s, exception:%s]", \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_ATTACHCLOCK_SUCCESS(audioSinkController, id) \
    do { \
        ::android::binder::Status status; \
        IAVClock::Id _clockId; \
        _clockId.value = static_cast<int32_t>(id); \
        UT_LOG_INFO("Calling attachClock(IN:clockId:[%d])", _clockId.value); \
        status = (audioSinkController)->attachClock(_clockId); \
        UT_LOG_INFO("Result attachClock(IN:clockId:[%d]), status:[isOK:%s, exception:%s]", \
                    _clockId.value, \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_ATTACHCLOCK_FAILURE(audioSinkController, id, exception) \
    do { \
        ::android::binder::Status status; \
        IAVClock::Id _clockId; \
        _clockId.value = static_cast<int32_t>(id); \
        UT_LOG_INFO("Calling attachClock(IN:clockId:[%d])", _clockId.value); \
        status = (audioSinkController)->attachClock(_clockId); \
        UT_LOG_INFO("Result attachClock(IN:clockId:[%d]), status:[isOK:%s, exception:%s]", \
                    _clockId.value, \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_DETACHCLOCK_SUCCESS(audioSinkController) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling detachClock()"); \
        status = (audioSinkController)->detachClock(); \
        UT_LOG_INFO("Result detachClock(), status:[isOK:%s, exception:%s]", \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_GETCLOCK_SUCCESS(audioSinkController, clockID) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getClock(OUT:clockId:[])"); \
        status = (audioSinkController)->getClock(&clockID); \
        UT_LOG_INFO("Result getClock(OUT:clockId:[%d]), status:[isOK:%s, exception:%s]", \
                    clockID.value, \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_QUEUEAUDIOFRAME_SUCCESS(audioSinkcontroller, nsPresentationTime, bufferHandle, metadata, result) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling queueAudioFrame(IN:nsPresentationTime:[%lld], IN:bufferHandle:[0x%016" PRIx64 "], IN:metadata:[])", \
                    nsPresentationTime, bufferHandle); \
        status = (audioSinkcontroller)->queueAudioFrame(nsPresentationTime, bufferHandle, metadata, &result); \
        UT_LOG_INFO("Result queueAudioFrame(IN:nsPresentationTime:[%lld], IN:bufferHandle:[0x%016" PRIx64 "], IN:metadata:[], OUT:_aidl_return[%s])", \
                    nsPresentationTime, bufferHandle, UT_Control_GetMapString(bool_mapTable, result)); \
        UT_ASSERT_TRUE(result); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(status.isOk()); \
    } while(0)

#define AUDIOSINK_QUEUEAUDIOFRAME_FAILURE(audioSinkcontroller, nsPresentationTime, bufferHandle, metadata, exception) \
    do { \
        ::android::binder::Status status; \
        bool aidl_return; \
        int32_t _numChannels = ((metadata).metadata.has_value()) ? (metadata).metadata->numChannels : -1; \
        int32_t _sampleRate  = ((metadata).metadata.has_value()) ? (metadata).metadata->sampleRate : -1; \
        int32_t _format      = ((metadata).metadata.has_value()) ? static_cast<int32_t>((metadata).metadata->format) : -1; \
        UT_LOG_INFO("Calling queueAudioFrame(IN:nsPresentationTime:[%lld], IN:bufferHandle:[0x%016" PRIx64 "], IN:metadata:[sourceCodec:%d, isDolbyAtmos:%d, numChannels:%d, sampleRate:%d, format:%d, source:%d])", \
                    nsPresentationTime, bufferHandle, static_cast<int>((metadata).sourceCodec), (metadata).isDolbyAtmos, _numChannels, _sampleRate, _format, static_cast<int>((metadata).source)); \
        status = (audioSinkcontroller)->queueAudioFrame(nsPresentationTime, bufferHandle, metadata, &aidl_return); \
        UT_LOG_INFO("Result queueAudioFrame(IN:nsPresentationTime:[%lld], IN:bufferHandle:[0x%016" PRIx64 "], IN:metadata:[sourceCodec:%d, isDolbyAtmos:%d, numChannels:%d, sampleRate:%d, format:%d, source:%d]), status:[isOK:%s, exception:%s]", \
                    nsPresentationTime, bufferHandle, static_cast<int>((metadata).sourceCodec), (metadata).isDolbyAtmos, _numChannels, _sampleRate, _format, static_cast<int>((metadata).source), \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while(0)

#define AUDIOSINK_FLUSH_SUCCESS(audioSink, audioSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling flush"); \
        status = (audioSinkController)->flush(); \
        UT_LOG_INFO("Result flush(status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            std::this_thread::sleep_for(std::chrono::milliseconds(MAX_FINAL_STATE_WAIT_MS)); \
            ::com::rdk::hal::audiosink::State lastState; \
            AUDIOSINK_WAIT_FOR_STATE(audioSink, ::com::rdk::hal::audiosink::State::STARTED, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::audiosink::State::STARTED); \
        } \
    } while (0)

#define AUDIOSINK_FLUSH_FAILURE(audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling flush"); \
        status = (audioSinkController)->flush(); \
        UT_LOG_INFO("Result flush(status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_SIGNAL_ENDOFSTREAM_SUCCESS(audioSinkController) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling signalEndOfStream()"); \
        status = (audioSinkController)->signalEndOfStream(); \
        UT_LOG_INFO("Result signalEndOfStream(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_SIGNAL_ENDOFSTREAM_FAILURE(audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling signalEndOfStream()"); \
        status = (audioSinkController)->signalEndOfStream(); \
        UT_LOG_INFO("Result signalEndOfStream(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_SETPROPERTY_SUCCESS(audioSink, property, propertyValue) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[])", com::rdk::hal::audiosink::toString(property).c_str(), \
                      propertyValue.value); \
        status = (audioSink)->setProperty(property, propertyValue, &_aidl_return); \
        UT_LOG_INFO("Result setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", com::rdk::hal::audiosink::toString(property).c_str(), propertyValue.value, \
            UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_SETPROPERTY_FAILURE(audioSink, property, propertyValue, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_INFO("Calling setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[])", com::rdk::hal::audiosink::toString(property).c_str(), propertyValue.value); \
        status = (audioSink)->setProperty(property, propertyValue, &_aidl_return); \
        UT_LOG_INFO("Result setProperty(IN:property[%s], IN:propertyValue[0x%X]), status:[isOK:%s, exception:%s]", com::rdk::hal::audiosink::toString(property).c_str(), propertyValue.value, \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, volumeSet) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling setVolume(IN:volume:[%f], IN:muted:[%s])", volumeSet.volume, UT_Control_GetMapString(bool_mapTable, volumeSet.muted)); \
        status = (audioSinkController)->setVolume(volumeSet, &(_aidl_return)); \
        UT_LOG_INFO("Result setVolume(IN:volume:[%f], IN:muted:[%s], OUT:volumeSetResult:[%s]), status:[isOK:%s, exception:%s]", volumeSet.volume, \
            UT_Control_GetMapString(bool_mapTable, volumeSet.muted), \
            UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_TRUE(status.isOk()); \
    } while (0)

#define AUDIOSINK_SETVOLUME_FAILURE(audioSinkController, volumeSet, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = true; \
        UT_LOG_INFO("Calling setVolume(IN:volume:[%f], IN:muted:[%s])", volumeSet.volume, UT_Control_GetMapString(bool_mapTable, volumeSet.muted)); \
        status = (audioSinkController)->setVolume(volumeSet, &(_aidl_return)); \
        UT_LOG_INFO("Result setVolume(IN:volume:[%f], IN:muted:[%s], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", volumeSet.volume, \
            UT_Control_GetMapString(bool_mapTable, volumeSet.muted), \
            UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        if ((exception) == Status::EX_NONE) { \
            /* Invalid volume is reported via a false return, not a binder exception. */ \
            UT_ASSERT_TRUE(status.isOk()); \
            UT_ASSERT_FALSE(_aidl_return); \
        } else { \
            UT_ASSERT_FALSE(status.isOk()); \
        } \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, volumeGet) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getVolume(OUT:volumeGet[])"); \
        status = (audioSinkController)->getVolume(&volumeGet); \
        UT_LOG_INFO("Result getVolume(OUT:volumeGet[volume:%f, muted:%s]), status:[isOK:%s, exception:%s]", \
            volumeGet.volume,UT_Control_GetMapString(bool_mapTable, volumeGet.muted), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(status.isOk()); \
    } while (0)

#define AUDIOSINK_GETVOLUME_FAILURE(audioSinkController, volumeGet, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_INFO("Calling getVolume(OUT:volumeGet[])"); \
        status = (audioSinkController)->getVolume(&volumeGet); \
        UT_LOG_INFO("Result getVolume(OUT:status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_SETVOLUMERAMP_SUCCESS(audioSinkController, targetVolume, overMs, volumeRamp) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_INFO("Calling setVolumeRamp(IN:targetVolume:[%f], IN:overMs:[%d], IN:volumeRamp:[%d], OUT:aidlReturn)", targetVolume, \
            overMs, static_cast<int32_t>(volumeRamp)); \
        status = (audioSinkController)->setVolumeRamp(targetVolume, overMs, volumeRamp, &_aidl_return); \
        UT_LOG_INFO("Result setVolumeRamp(IN:targetVolume:[%f], IN:overMs:[%d], IN:volumeRamp:[%d], OUT:aidlReturn:[%s]), status:[isOK:%s, exception:%s]", targetVolume, \
            overMs, static_cast<int32_t>(volumeRamp), _aidl_return ? "true" : "false", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(_aidl_return); \
    }while (0)

#define AUDIOSINK_SETVOLUMERAMP_FAILURE(audioSinkController, targetVolume, overMs, volumeRamp, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = true; \
        UT_LOG_INFO("Calling setVolumeRamp(IN:targetVolume:[%f], IN:overMs:[%d], IN:volumeRamp:[%d], OUT:aidlReturn)", targetVolume, \
            overMs, static_cast<int32_t>(volumeRamp)); \
        status = (audioSinkController)->setVolumeRamp(targetVolume, overMs, volumeRamp, &_aidl_return); \
        UT_LOG_INFO("Result setVolumeRamp(IN:targetVolume:[%f], IN:overMs:[%d], IN:volumeRamp:[%d], OUT:aidlReturn:[%s]), status:[isOK:%s, exception:%s]", targetVolume, \
            overMs, static_cast<int32_t>(volumeRamp), _aidl_return ? "true" : "false", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        if ((exception) == Status::EX_NONE) { \
            /* Invalid args are reported via a false return, not a binder exception. */ \
            UT_ASSERT_TRUE(status.isOk()); \
            UT_ASSERT_FALSE(_aidl_return); \
        } else { \
            UT_ASSERT_FALSE(status.isOk()); \
        } \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    }while (0)

#define AUDIOSINK_SETPROPERTY_LIST_SUCCESS(audioSink, properyList, type) \
    do { \
        for (const auto& [property, value] : properyList) \
        { \
            PropertyValue propertyValue; \
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::type>(value); \
            AUDIOSINK_SETPROPERTY_SUCCESS(audioSink, property, propertyValue); \
        } \
    } while (0)

#define AUDIOSINK_SETPROPERTY_LIST_FAILURE(audioSink, properyList, type, exception) \
    do { \
        for (const auto& [property, value] : properyList) \
        { \
            PropertyValue propertyValue; \
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::type>(value); \
            AUDIOSINK_SETPROPERTY_FAILURE(audioSink, property, propertyValue, exception); \
        } \
    } while (0)

class AudioSinkControllerListener: public BnAudioSinkControllerListener
{
public:
    AudioSinkControllerListener(int32_t sinkID)
       : BnAudioSinkControllerListener(),
         mUnderFlowCount(0),
         mFlushCompleteCount(0),
         mFrameBufferAvailableCount(0),
         mSinkID(sinkID)
    {
    }
    virtual ~AudioSinkControllerListener() = default;

    Status onStateChanged(::com::rdk::hal::audiosink::State oldState, ::com::rdk::hal::audiosink::State newState) override;
    Status onFirstFrameRendered(int64_t nsPresentationTime) override;
    Status onEndOfStream(int64_t nsPresentationTime) override;
    Status onAudioUnderflow() override;
    Status onAudioResumed(int64_t nsPresentationTime) override;
    Status onFlushComplete() override;
    Status onFrameBufferAvailable() override;

    bool waitForState(::com::rdk::hal::audiosink::State desiredState, std::chrono::milliseconds timeout);
    void resetStateChanges();
    bool checkStateTransistion(::com::rdk::hal::audiosink::State expOldState, ::com::rdk::hal::audiosink::State expNewState);
    bool getNextFirstFrameRenderedPTS(int64_t &firstFramePTS);
    bool getNextEndOfStreamPTS(int64_t &endOfStreamPTS);
    bool getNextAudioResumedPTS(int64_t &audioResumedPTS);
    int32_t getUnderFlowCount(bool reset);
    int32_t getFlushCompleteCount(bool reset);
    int32_t getFrameBufferAvailableCount(bool reset);
private:
    mutable mutex mStateMutex;
    mutable mutex mFFMutex;
    mutable mutex mEOSMutex;
    mutable mutex mUnderFlowMutex;
    mutable mutex mResumeMutex;
    mutable mutex mFlushMutex;
    mutable mutex mFrameBufferAvailableMutex;
    condition_variable mCV;
    condition_variable mFFRenderAvailable;
    condition_variable mEOSAvailable;
    condition_variable mResumeAvailable;
    ::com::rdk::hal::audiosink::State mCurrentState = ::com::rdk::hal::audiosink::State::CLOSED;
    vector<std::pair<::com::rdk::hal::audiosink::State, ::com::rdk::hal::audiosink::State>> mStateChanges;
    queue<int64_t> mFirstframerendered;
    queue<int64_t> mEndofstream;
    queue<int64_t> mAudioresumed;
    int32_t mUnderFlowCount;
    int32_t mFlushCompleteCount;
    int32_t mSinkID;
    int32_t mFrameBufferAvailableCount;
};

class AudioSinkEventListener: public BnAudioSinkEventListener
{
public:
    AudioSinkEventListener(int32_t sinkID)
       : BnAudioSinkEventListener(),
         mUnderFlowCount(0),
         mFlushCompleteCount(0),
         mSinkID(sinkID)
    {
    }
    virtual ~AudioSinkEventListener() = default;

    Status onStateChanged(::com::rdk::hal::audiosink::State oldState, ::com::rdk::hal::audiosink::State newState) override;
    Status onFirstFrameRendered(int64_t nsPresentationTime) override;
    Status onEndOfStream(int64_t nsPresentationTime) override;
    Status onAudioUnderflow() override;
    Status onAudioResumed(int64_t nsPresentationTime) override;
    Status onFlushComplete() override;
    bool waitForState(::com::rdk::hal::audiosink::State desiredState, std::chrono::milliseconds timeout);
    void resetStateChanges();
    bool checkStateTransistion(::com::rdk::hal::audiosink::State expOldState, ::com::rdk::hal::audiosink::State expNewState);
    bool getNextFirstFrameRenderedPTS(int64_t &firstFramePTS);
    bool getNextEndOfStreamPTS(int64_t &endOfStreamPTS);
    bool getNextAudioResumedPTS(int64_t &audioResumedPTS);
    int32_t getUnderFlowCount(bool reset);
    int32_t getFlushCompleteCount(bool reset);
private:
    mutable mutex mStateMutex;
    mutable mutex mFFMutex;
    mutable mutex mEOSMutex;
    mutable mutex mUnderFlowMutex;
    mutable mutex mResumeMutex;
    mutable mutex mFlushMutex;
    condition_variable mCV;
    condition_variable mFFRenderAvailable;
    condition_variable mEOSAvailable;
    condition_variable mResumeAvailable;
    ::com::rdk::hal::audiosink::State mCurrentState = ::com::rdk::hal::audiosink::State::CLOSED;
    vector<std::pair<::com::rdk::hal::audiosink::State, ::com::rdk::hal::audiosink::State>> mStateChanges;
    queue<int64_t> mFirstframerendered;
    queue<int64_t> mEndofstream;
    queue<int64_t> mAudioresumed;
    int32_t mUnderFlowCount;
    int32_t mFlushCompleteCount;
    int32_t mSinkID;
};

class AudioSinkAVClockControllerListener : public BnAVClockControllerListener
{
public:
    AudioSinkAVClockControllerListener() = default;
    virtual ~AudioSinkAVClockControllerListener() = default;

    ::android::binder::Status onStateChanged(::com::rdk::hal::avclock::State oldState,
                                             ::com::rdk::hal::avclock::State newState) override
    {
        UT_LOG_DEBUG("AVClock state changed from %s to %s",
                     ::com::rdk::hal::avclock::toString(oldState).c_str(),
                     ::com::rdk::hal::avclock::toString(newState).c_str());
        return ::android::binder::Status::ok();
    }

    ::android::binder::Status onPrimed(const ::com::rdk::hal::avclock::ClockTime& currentClockTime) override
    {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mPrimed)
            {
                mFirstPrimedClockTime = currentClockTime;
                mPrimed = true;
            }
        }
        mPrimedCondition.notify_all();
        UT_LOG_DEBUG("AVClock primed: clockTimeNs=%lld sampleTimestampNs=%lld",
                     static_cast<long long>(currentClockTime.clockTimeNs),
                     static_cast<long long>(currentClockTime.sampleTimestampNs));
        return ::android::binder::Status::ok();
    }

    bool waitForPrimed(std::chrono::milliseconds timeout,
                       ::com::rdk::hal::avclock::ClockTime* outClockTime = nullptr)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        if (!mPrimedCondition.wait_for(lock, timeout, [this]() { return mPrimed; }))
        {
            return false;
        }
        if (outClockTime != nullptr)
        {
            *outClockTime = mFirstPrimedClockTime;
        }
        return true;
    }

    void resetPrimed()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mPrimed = false;
        mFirstPrimedClockTime = {};
    }

private:
    std::mutex mMutex;
    std::condition_variable mPrimedCondition;
    bool mPrimed = false;
    ::com::rdk::hal::avclock::ClockTime mFirstPrimedClockTime{};
};

class AudioSinkTest
{
public:
    AudioSinkTest(IAudioSink::Id sinkId,
                  sp<IAudioSink> sinkHandle,
                  sp<AudioSinkControllerListener> controlListener,
                  sp<AudioSinkEventListener> eventListener,
                  sp<AudioDecoderControllerListener> decoderControlListener,
                  sp<IAVBuffer> AVBufferManager,
                  IAudioDecoder::Id decoderId,
                  IAVClock::Id clockId,
                  bool secureDecode, bool isTunnel, const testConfiguration_t& config, bool isClockInUse)
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
                                      audiodecoder::FrameMetadata& metadata,
                                      bool& flushDone);
    void validateMetric();
};

#endif //__VTS_AUDIOSINK_H__

/** @} */ // End of AUDIO_SINK_HALTEST_HEADER
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK
/** @} */ // End of HPK
