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

#include "com/rdk/hal/audiosink/IAudioSinkManager.h"
#include "com/rdk/hal/audiosink/BnAudioSinkControllerListener.h"
#include "com/rdk/hal/audiosink/BnAudioSinkEventListener.h"
#include "com/rdk/hal/audiosink/ContentType.h"
#include "com/rdk/hal/audiodecoder/FrameMetadata.h"

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace std;
using namespace com::rdk::hal::audiosink;

extern void *gAudioSinkTestConfiguration;
extern sp<IAudioSinkManager> gAudioSinkManager;

#define MAX_TRANSITORY_STATE_WAIT_MS  5
#define MAX_FINAL_STATE_WAIT_MS  500

#ifndef ENABLE_UT_INFO_LOGGING
#ifdef UT_LOG_DEBUG
#undef UT_LOG_DEBUG
#define UT_LOG_DEBUG(...)
#endif
#ifdef UT_LOG_INFO
#undef UT_LOG_INFO
#define UT_LOG_INFO(...)
#endif
#endif

#define AUDIOSINK_MANAGER_GETAUDIOSINKIDS(audioSinkIds) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getAudioSinkIds(OUT:sinkIds:[])"); \
        status = (gAudioSinkManager)->getAudioSinkIds(&audioSinkIds); \
        UT_LOG_DEBUG("Result getAudioSinksIds(status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_GETPLATFORMCAPABILITIES(platformCapabilities) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("getPlatformCapabilities(OUT:platformCapabilities[])"); \
        status = (gAudioSinkManager)->getPlatformCapabilities(&platformCapabilities); \
        UT_LOG_DEBUG("Result getCapabilities(OUT:capabilities), status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_MANAGER_GETAUDIOSINK(_sinkId, audioSink) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getAudioSink(IN:audioSinkId:[%d], OUT:audioSink[])", _sinkId.value); \
        status = (gAudioSinkManager)->getAudioSink(_sinkId, &audioSink); \
        UT_LOG_DEBUG("Result getAudioSink(IN:_sinkId:[%d], OUT:audioSink[0x%X]), status:[isOK:%s, exception:%s]", \
                                _sinkId.value, audioSink.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_TRUE_FATAL(audioSink.get() != nullptr); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_MANAGER_GETAUDIOSINK_FAILURE(_sinkId, audioSink, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getAudioSink(IN:_sinkId:[%d], OUT:audioSink[])", _sinkId.value); \
        status = (gAudioSinkManager)->getAudioSink(_sinkId, &audioSink); \
        UT_LOG_DEBUG("Result getAudioSink(IN:_sinkId:[%d]), status:[isOK:%s, exception:%s]", \
                                _sinkId.value, UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIODSINK_GETCAPABILITIES(audioSink, capabilities) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getCapabilities(OUT:capabilities)"); \
        status = (audioSink)->getCapabilities(&capabilities); \
        UT_LOG_DEBUG("Result getCapabilities(OUT:capabilities), status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_NOT_EQUAL(&capabilities, nullptr); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define AUDIOSINK_GET_PROPERTY_SUCCESS(audioSink, property, propertyValue) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getProperty(IN:property[%s], OUT:propertyValue[]", audiosink::toString(property).c_str()); \
        status = (audioSink)->getProperty(property, &propertyValue); \
        switch (propertyValue->value.value().getTag()) \
        { \
            case PropertyValue::Value::Tag::intValue: \
                UT_LOG_DEBUG("Result getProperty(IN:property[%s], OUT:propertyValue[%d]), status:[isOK:%s, exception:%s]", audiosink::toString(property).c_str(), \
                propertyValue->value->get<PropertyValue::Value::Tag::intValue>(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
                break; \
            case PropertyValue::Value::Tag::stringValue: \
                UT_LOG_DEBUG("Result getProperty(IN:property[%s], OUT:propertyValue[%s]), status:[isOK:%s, exception:%s]", audiosink::toString(property).c_str(), \
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
        UT_LOG_DEBUG("Calling getProperty(IN:property[%s], OUT:propertyValue[]", audiosink::toString(property).c_str()); \
        status = (audioSink)->getProperty(property, &propertyValue); \
        UT_LOG_DEBUG("Result getProperty(IN:property[%s], OUT:propertyValue[]), status:[isOK:%s, exception:%s]", audiosink::toString(property).c_str(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_GET_STATE(audioSink, state) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getState(OUT:state[]"); \
        status = (audioSink)->getState(&state); \
        UT_LOG_DEBUG("Result getState(OUT:state[%s]), status:[isOK:%s, exception:%s]", com::rdk::hal::toString(state).c_str(), \
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
        UT_LOG_DEBUG("Calling open(IN:contentType[%d], IN:controllerListener[0x%p], OUT:audioSinkController[])", static_cast<int32_t>(contentType), (void*)((controllerListener).get())); \
        status = (audioSink)->open(contentType, (controllerListener), &(audioSinkController)); \
        UT_LOG_DEBUG("Result open(IN:contentType[%d], IN:controllerListener[0x%p], OUT:audioSinkController[0x%p]), status:[isOK:%s, exception:%s]", static_cast<int32_t>(contentType), (void*)((controllerListener).get()),\
            (void*)((audioSinkController).get()),\
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_NOT_EQUAL_FATAL(audioSinkController.get(), nullptr); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
        \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            AUDIOSINK_WAIT_FOR_STATE(audioSink, ::com::rdk::hal::State::READY, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::State::READY); \
        } \
    } while (0)

#define AUDIOSINK_OPEN_FAILURE(audioSink, contentType, controllerListener, audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling open failure(IN:contentType[%d], IN:controllerListener[N/A], OUT:audioSinkController[])", \
                     static_cast<int32_t>(contentType)); \
        status = (audioSink)->open(contentType, (controllerListener), &(audioSinkController)); \
        UT_LOG_DEBUG("Result open failure (IN:contentType[%d], IN:controllerListener[N/A]), status:[isOK:%s, exception:%s]", static_cast<int32_t>(contentType),\
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_CLOSE_SUCCESS(audioSink, audioSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling close(IN:audioSinkController[0x%p], OUT:_aidl_return[])", audioSinkController.get()); \
        status = audioSink->close(audioSinkController, &_aidl_return); \
        UT_LOG_DEBUG("Result close(IN:audioSinkController[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            audioSinkController.get(), UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_TRUE_FATAL(_aidl_return); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            AUDIOSINK_WAIT_FOR_STATE(audioSink, ::com::rdk::hal::State::CLOSED, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::State::CLOSED); \
        } \
    } while (0)

#define AUDIOSINK_CLOSE_FAILURE(audioSink, audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling close(OUT:_aidl_return[])"); \
        status = (audioSink)->close(audioSinkController, &_aidl_return); \
        UT_LOG_DEBUG("Result close(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_REGISTER_EVENT_SUCCESS(audioSink, eventListener) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = (audioSink)->registerEventListener(eventListener, &_aidl_return); \
        UT_LOG_DEBUG("Result registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
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
        UT_LOG_DEBUG("Calling registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = (audioSink)->registerEventListener(eventListener, &_aidl_return); \
        UT_LOG_DEBUG("Result registerEventListener(IN:eventListener[0x%p]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        if (exception == Status::EX_NONE) \
            UT_ASSERT_TRUE(status.isOk()); \
        else \
            UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_UNREGISTER_EVENT_SUCCESS(audioSink, eventListener) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = (audioSink)->unregisterEventListener(eventListener, &_aidl_return); \
        UT_LOG_DEBUG("Result unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
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
        UT_LOG_DEBUG("Calling unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = (audioSink)->unregisterEventListener(eventListener, &_aidl_return); \
        UT_LOG_DEBUG("Result unregisterEventListener(IN:eventListener[0x%p]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define AUDIOSINK_START_SUCCESS(audioSink, audioSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling start()"); \
        status = (audioSinkController)->start(); \
        UT_LOG_DEBUG("Result start(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            AUDIOSINK_WAIT_FOR_STATE(audioSink, ::com::rdk::hal::State::STARTED, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::State::STARTED); \
        } \
    } while (0)

#define AUDIOSINK_START_FAILURE(audioSink, audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling start()"); \
        status = (audioSinkController)->start(); \
        UT_LOG_DEBUG("Result start(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_STOP_SUCCESS(audioSink, audioSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling stop()"); \
        status = (audioSinkController)->stop(); \
        UT_LOG_DEBUG("Result stop(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            AUDIOSINK_WAIT_FOR_STATE(audioSink, ::com::rdk::hal::State::READY, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::State::READY); \
        } \
    } while (0)

#define AUDIOSINK_STOP_FAILURE(audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling stop()"); \
        status = (audioSinkController)->stop(); \
        UT_LOG_DEBUG("Result stop(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_SETAUDIODECODER_SUCCESS(decoderId, audioSinkController) \
    do { \
        ::android::binder::Status status; \
        ::com::rdk::hal::audiodecoder::IAudioDecoder::Id audioDecoderId; \
        audioDecoderId.value = decoderId; \
        bool aidl_return; \
        UT_LOG_DEBUG("Calling setAudioDecoder(IN:audioDecoderId:[%d], OUT:_aidl_return[])", audioDecoderId.value); \
        status = (audioSinkController)->setAudioDecoder(audioDecoderId, &aidl_return); \
        UT_LOG_DEBUG("Result setAudioDecoder(IN:audioDecoderId:[%d], OUT:_aidl_return:[%s]), status:[isOK:%s, exception:%s]", \
                    audioDecoderId.value, aidl_return ? "true" : "false", \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(aidl_return); \
        UT_ASSERT_TRUE(status.isOk()); \
    } while (0)

#define AUDIOSINK_SETAUDIODECODER_FAILURE(decoderId, audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        ::com::rdk::hal::audiodecoder::IAudioDecoder::Id audioDecoderId; \
        audioDecoderId.value = decoderId; \
        bool aidl_return = false; \
        UT_LOG_DEBUG("Calling setAudioDecoder(IN:audioDecoderId:[%d], OUT:_aidl_return[])", audioDecoderId.value); \
        status = (audioSinkController)->setAudioDecoder(audioDecoderId, &aidl_return); \
        UT_LOG_DEBUG("Result setAudioDecoder(IN:audioDecoderId:[%d]), status:[isOK:%s, exception:%s]", \
                    audioDecoderId.value, \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_GETAUDIODECODER_SUCCESS(audioDecoderId, audioSinkController) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling setAudioDecoder(IN:audioDecoderId:[], OUT:_aidl_return[])"); \
        status = (audioSinkController)->getAudioDecoder(&audioDecoderId); \
        UT_LOG_DEBUG("Result setAudioDecoder(IN:audioDecoderId:[%d], status:[isOK:%s, exception:%s]", \
                    audioDecoderId.value, \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
    } while (0)

#define AUDIOSINK_GETAUDIODECODER_FAILURE(audioDecoderId, audioSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling setAudioDecoder(OUT:audioDecoderId:[]"); \
        status = (audioSinkController)->getAudioDecoder(&audioDecoderId);\
        UT_LOG_DEBUG("Result setAudioDecoder(), status:[isOK:%s, exception:%s]", \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_QUEUEAUDIOFRAME_SUCCESS(sindkId, audioSinkcontroller, nsPresentationTime, bufferHandle, metadata) \
    do { \
        ::android::binder::Status status; \
        bool aidl_return; \
        UT_LOG_DEBUG("[SinkID: %d] Calling queueAudioFrame(IN:nsPresentationTime:[%lld], IN:bufferHandle:[0x%016" PRIx64 "], IN:metadata:[sourceCodec:%d, isDolbyAtmos:%d, numChannels:%d, sampleRate:%d, format:%d, source:%d])", \
                    sindkId, nsPresentationTime, bufferHandle, static_cast<int>(metadata.sourceCodec), metadata.isDolbyAtmos, metadata.numChannels, metadata.sampleRate, static_cast<int>(metadata.format), static_cast<int>(metadata.source)); \
        status = (audioSinkController)->queueAudioFrame(nsPresentationTime, bufferHandle, metadata, &aidl_return); \
        UT_LOG_DEBUG("[SinkID: %d] Result queueAudioFrame(IN:nsPresentationTime:[%lld], IN:bufferHandle:[0x%016" PRIx64 "], IN:metadata:[sourceCodec:%d, isDolbyAtmos:%d, numChannels:%d, sampleRate:%d, format:%d, source:%d]), status:[isOK:%s, exception:%s]", \
                    sindkId, nsPresentationTime, bufferHandle, static_cast<int>(metadata.sourceCodec), metadata.isDolbyAtmos, metadata.numChannels, metadata.sampleRate, static_cast<int>(metadata.format), static_cast<int>(metadata.source), \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(aidl_return); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(status.isOk()); \
        } while(0)

#define AUDIOSINK_QUEUEAUDIOFRAME_FAILURE(audioSinkcontroller, nsPresentationTime, bufferHandle, metadata, exception) \
    do { \
        ::android::binder::Status status; \
        bool aidl_return; \
        UT_LOG_DEBUG("Calling queueAudioFrame(IN:nsPresentationTime:[%lld], IN:bufferHandle:[0x%016" PRIx64 "], IN:metadata:[sourceCodec:%d, isDolbyAtmos:%d, numChannels:%d, sampleRate:%d, format:%d, source:%d])", \
                    nsPresentationTime, bufferHandle, static_cast<int>(metadata.sourceCodec), metadata.isDolbyAtmos, metadata.numChannels, metadata.sampleRate, static_cast<int>(metadata.format), static_cast<int>(metadata.source)); \
        status = (audioSinkController)->queueAudioFrame(nsPresentationTime, bufferHandle, metadata, &aidl_return); \
        UT_LOG_DEBUG("Result queueAudioFrame(IN:nsPresentationTime:[%lld], IN:bufferHandle:[0x%016" PRIx64 "], IN:metadata:[sourceCodec:%d, isDolbyAtmos:%d, numChannels:%d, sampleRate:%d, format:%d, source:%d]), status:[isOK:%s, exception:%s]", \
                    nsPresentationTime, bufferHandle, static_cast<int>(metadata.sourceCodec), metadata.isDolbyAtmos, metadata.numChannels, metadata.sampleRate, static_cast<int>(metadata.format), static_cast<int>(metadata.source), \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
        } while(0)

#define AUDIOSINK_FLUSH_SUCCESS(audioSink, audioSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling flush"); \
        status = (audioSinkController)->flush(); \
        UT_LOG_DEBUG("Result flush(status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            std::this_thread::sleep_for(std::chrono::milliseconds(MAX_FINAL_STATE_WAIT_MS)); \
            ::com::rdk::hal::State lastState; \
            AUDIOSINK_WAIT_FOR_STATE(audioSink, ::com::rdk::hal::State::STARTED, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::State::STARTED); \
        } \
    } while (0)

#define AUDIOSINK_FLUSH_FAILURE(audioDecoderController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling flush"); \
        status = (audioSinkController)->flush(); \
        UT_LOG_DEBUG("Result flush(status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_SETPROPERTY_SUCCESS(audioSink, property, propertyValue) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[])", com::rdk::hal::audiosink::toString(property).c_str(), \
                      propertyValue.value); \
        status = (audioSink)->setProperty(property, propertyValue, &_aidl_return); \
        UT_LOG_DEBUG("Result setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", com::rdk::hal::audiosink::toString(property).c_str(), propertyValue.value, \
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
        UT_LOG_DEBUG("Calling setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[])", com::rdk::hal::audiosink::toString(property).c_str(), propertyValue.value); \
        status = (audioSink)->setProperty(property, propertyValue, &_aidl_return); \
        UT_LOG_DEBUG("Result setProperty(IN:property[%s], IN:propertyValue[0x%X]), status:[isOK:%s, exception:%s]", com::rdk::hal::audiosink::toString(property).c_str(), propertyValue.value, \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_SETVOLUME_SUCCESS(audioSinkController, volumeSet) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling setVolume(IN:volume:[%f], IN:muted:[%s])", volumeSet.volume, UT_Control_GetMapString(bool_mapTable, volumeSet.muted)); \
        status = (audioSinkController)->setVolume(volumeSet, &(_aidl_return)); \
        UT_LOG_DEBUG("Result setVolume(IN:volume:[%f], IN:muted:[%s], OUT:volumeSetResult:[%s]), status:[isOK:%s, exception:%s]", volumeSet.volume, \
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
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling setVolume(IN:volume:[%f], IN:muted:[%s])", volumeSet.volume, UT_Control_GetMapString(bool_mapTable, volumeSet.muted)); \
        status = (audioSinkController)->setVolume(volumeSet, &(_aidl_return)); \
        UT_LOG_DEBUG("Result setVolume(IN:volume:[%f], IN:muted:[%s]), status:[isOK:%s, exception:%s]", volumeSet.volume, \
            UT_Control_GetMapString(bool_mapTable, volumeSet.muted), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_GETVOLUME_SUCCESS(audioSinkController, volumeGet) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getVolume(OUT:volumeGet[])"); \
        status = (audioSinkController)->getVolume(&volumeGet); \
        UT_LOG_DEBUG("Result getVolume(OUT:volumeGet[volume:%f, muted:%s]), status:[isOK:%s, exception:%s]", \
            volumeGet.volume,UT_Control_GetMapString(bool_mapTable, volumeGet.muted), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(status.isOk()); \
    } while (0)

#define AUDIOSINK_GETVOLUME_FAILURE(audioSinkController, volumeGet, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getVolume(OUT:volumeGet[])"); \
        status = (audioSinkController)->getVolume(&volumeGet); \
        UT_LOG_DEBUG("Result getVolume(OUT:status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define AUDIOSINK_SETVOLUMERAMP_SUCCESS(audioSinkController, targetVolume, overMs, volumeRamp) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling setVolumeRamp(IN:targetVolume:[%f], IN:overMs:[%d], IN:volumeRamp:[%d], OUT:aidlReturn)", targetVolume, \
            overMs, static_cast<int32_t>(volumeRamp)); \
        status = (audioSinkController)->setVolumeRamp(targetVolume, overMs, volumeRamp, &_aidl_return); \
        UT_LOG_DEBUG("Result setVolumeRamp(IN:targetVolume:[%f], IN:overMs:[%d], IN:volumeRamp:[%d], OUT:aidlReturn:[%s]), status:[isOK:%s, exception:%s]", targetVolume, \
            overMs, static_cast<int32_t>(volumeRamp), _aidl_return ? "true" : "false", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(_aidl_return); \
    }while (0)

#define AUDIOSINK_SETVOLUMERAMP_FAILURE(audioSinkController, targetVolume, overMs, volumeRamp, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling setVolumeRamp(IN:targetVolume:[%f], IN:overMs:[%d], IN:volumeRamp:[%d], OUT:aidlReturn)", targetVolume, \
            overMs, static_cast<int32_t>(volumeRamp)); \
        status = (audioSinkController)->setVolumeRamp(targetVolume, overMs, volumeRamp, &_aidl_return); \
        UT_LOG_DEBUG("Result setVolumeRamp(IN:targetVolume:[%f], IN:overMs:[%d], IN:volumeRamp:[%d]), status:[isOK:%s, exception:%s]", targetVolume, \
            overMs, static_cast<int32_t>(volumeRamp), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
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
         mSinkID(sinkID)
    {
    }
    virtual ~AudioSinkControllerListener() = default;

    Status onStateChanged(::com::rdk::hal::State oldState, ::com::rdk::hal::State newState) override;
    Status onFirstFrameRendered(int64_t nsPresentationTime) override;
    Status onEndOfStream(int64_t nsPresentationTime) override;
    Status onAudioUnderflow() override;
    Status onAudioResumed(int64_t nsPresentationTime) override;
    Status onFlushComplete() override;

    bool waitForState(::com::rdk::hal::State desiredState, std::chrono::milliseconds timeout);
    void resetStateChanges();
    bool checkStateTransistion(::com::rdk::hal::State expOldState, ::com::rdk::hal::State expNewState);
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
    condition_variable mUnderFlowAvailable;
    condition_variable mResumeAvailable;
    condition_variable mFlushCompleteAvailable;

    ::com::rdk::hal::State mCurrentState;
    vector<std::pair<::com::rdk::hal::State, ::com::rdk::hal::State>> mStateChanges;
    queue<int64_t> mFirstframerendered;
    queue<int64_t> mEndofstream;
    queue<int64_t> mAudioresumed;

    int32_t mUnderFlowCount;
    int32_t mFlushCompleteCount;
    int32_t mSinkID;
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

    Status onStateChanged(::com::rdk::hal::State oldState, ::com::rdk::hal::State newState) override;
    Status onFirstFrameRendered(int64_t nsPresentationTime) override;
    Status onEndOfStream(int64_t nsPresentationTime) override;
    Status onAudioUnderflow() override;
    Status onAudioResumed(int64_t nsPresentationTime) override;
    Status onFlushComplete() override;

    bool waitForState(::com::rdk::hal::State desiredState, std::chrono::milliseconds timeout);
    void resetStateChanges();
    bool checkStateTransistion(::com::rdk::hal::State expOldState, ::com::rdk::hal::State expNewState);
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
    condition_variable mUnderFlowAvailable;
    condition_variable mResumeAvailable;
    condition_variable mFlushCompleteAvailable;

    ::com::rdk::hal::State mCurrentState;
    vector<std::pair<::com::rdk::hal::State, ::com::rdk::hal::State>> mStateChanges;
    queue<int64_t> mFirstframerendered;
    queue<int64_t> mEndofstream;
    queue<int64_t> mAudioresumed;
    int32_t mUnderFlowCount;
    int32_t mFlushCompleteCount;
    int32_t mSinkID;
};
#endif //__VTS_AUDIOSINK_H__

/** @} */ // End of AUDIO_SINK_HALTEST_HEADER
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK
/** @} */ // End of HPK
