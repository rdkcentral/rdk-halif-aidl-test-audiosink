/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

/** @brief
* AudioSink.cpp
*
* Implementation of AudioSink service
*/
#include "AudioSink.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace com {
    namespace rdk {
        namespace hal {
            namespace audiosink {

                State g_audiosink_state[NUM_AUDIO_SINK] = {State::UNKNOWN, State::UNKNOWN};
                audiodecoder::IAudioDecoder::Id g_audiodecoder_id[NUM_AUDIO_SINK];
                bool g_audiodecoder_selected[NUM_AUDIO_SINK];
                int32_t g_resource_id[NUM_AUDIO_SINK];
                int32_t g_queue_depth_ns[NUM_AUDIO_SINK];
                int32_t g_render_latency_ns[NUM_AUDIO_SINK];
                avclock::IAVClock::Id g_avclock_id[NUM_AUDIO_SINK];

                /* Read Write Properties */
                AVSource g_av_source[NUM_AUDIO_SINK];
                int32_t g_mixer_id[NUM_AUDIO_SINK];
                int32_t g_reference_level[NUM_AUDIO_SINK];
                Volume g_volume[NUM_AUDIO_SINK];
                android::sp<audiosink::IAudioSinkEventListener> g_audioSinkEventListeners[NUM_AUDIO_SINK];
                android::sp<audiosink::IAudioSinkControllerListener> g_audioSinkControllerListeners[NUM_AUDIO_SINK];

                std::mutex state_mutex;
                std::mutex ramp_mutex;
                std::condition_variable state_cv;
                std::condition_variable conditionVar;

                AudioSinkManager::AudioSinkManager() {
                    for (int32_t i = 0;i<NUM_AUDIO_SINK;i++)
                    {
                        g_reference_level[i] = -31;
                        g_av_source[i] = AVSource::UNKNOWN;
                        g_mixer_id[i] = 0;
                        g_volume[i].volume = 1.0;
                        g_volume[i].muted = false;
                        g_audiodecoder_id[i].value = audiodecoder::IAudioDecoder::Id::UNDEFINED;
                        g_audiodecoder_selected[i] = false;
                        g_avclock_id[i].value = avclock::IAVClock::Id::UNDEFINED;
                        g_resource_id[i] = i;
                        g_audioSinkEventListeners[i] = nullptr;
                        g_audioSinkControllerListeners[i] = nullptr;
                    }
                    printf("AudioSinkManager service initialized\n");
                }

                AudioSinkManager::~AudioSinkManager() {
                    printf("AudioSinkManager service destroyed\n");
                }

                static inline int32_t canonicalSinkIndex(int64_t rawSinkId)
                {
                    if (rawSinkId == g_resource_id[0]) {
                        return 0;
                    }
                    if (rawSinkId == g_resource_id[1]) {
                        return 1;
                    }
                    if (rawSinkId < 0) {
                        return -1;
                    }
                    return static_cast<int32_t>(rawSinkId & 0x1);
                }

                static inline void notifyStateChanged(int32_t sinkId, State oldState, State newState)
                {
                    const int32_t sinkIndex = canonicalSinkIndex(sinkId);
                    if (sinkIndex < 0 || sinkIndex >= NUM_AUDIO_SINK) {
                        return;
                    }

                    if (g_audioSinkEventListeners[sinkIndex] != nullptr) {
                        g_audioSinkEventListeners[sinkIndex]->onStateChanged(oldState, newState);
                    }

                    if (g_audioSinkControllerListeners[sinkIndex] != nullptr) {
                        g_audioSinkControllerListeners[sinkIndex]->onStateChanged(oldState, newState);
                    }
                }

                android::binder::Status AudioSinkManager::getAudioSinkIds(  std::vector<IAudioSink::Id>* _aidl_return  )
                {
                    for (int32_t i = 0;i<NUM_AUDIO_SINK;i++)
                    {
                        audiosink::IAudioSink::Id sinkId;
                        sinkId.value = g_resource_id[i];
                        _aidl_return->push_back(sinkId);
                    }
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkManager::getPlatformCapabilities(  PlatformCapabilities* _aidl_return  )
                {
                    if (_aidl_return == nullptr) {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
                    }

                    _aidl_return->supportsLowLatency = false;
                    _aidl_return->supportedSampleRatesHz = {48000};
                    _aidl_return->supportedPCMFormats = {com::rdk::hal::audiodecoder::PCMFormat::S16LE};
                    _aidl_return->supportsPlanarFormat = false;
                    _aidl_return->supportsSoCProprietary = false;
                    _aidl_return->maxNumChannels = 2;
                    _aidl_return->supportsDolbyAtmos = false;

                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkManager::getAudioSink(  const IAudioSink::Id& audioSinkId, android::sp<IAudioSink>* _aidl_return  )
                {
                    int32_t found = 0;
                    for(int32_t i = 0; i < NUM_AUDIO_SINK; i++)
                    {
                        if(g_resource_id[i] == audioSinkId.value)
                        {
                            found = 1;
                            break;
                        }
                    }
                    if(!found) {
                        *_aidl_return = nullptr;
                        return android::binder::Status::fromExceptionCode(
                                android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    android::sp<IAudioSink> audioSink = new AudioSink(audioSinkId);
                    g_audiosink_state[audioSinkId.value] = State::CLOSED;
                    *_aidl_return = audioSink;
                    return android::binder::Status::ok();
                }

                AudioSink::AudioSink(IAudioSink::Id id)
                {
                    this->id = id;
                    this->id.value = canonicalSinkIndex(id.value);
                    this->event_listener = false;
                    printf("AudioSink service initialized\n");
                }

                AudioSink::~AudioSink() {
                    printf("AudioSink service destroyed\n");
                }

                android::binder::Status AudioSink::getCapabilities(  audiosink::Capabilities* _aidl_return  ) {
                    audiosink::Capabilities capabilities;

                    // Normalize any non-canonical sink id encoding to one of the
                    // two supported mock sinks so capability payload is never empty.
                    const int64_t sinkKey =
                        (this->id.value == g_resource_id[0] || this->id.value == g_resource_id[1])
                            ? this->id.value
                            : (this->id.value & 0x1);

                    if (sinkKey == g_resource_id[0]) {
                        capabilities.supportsSecure = true;
                        capabilities.supportedContentTypes = {
                            audiosink::ContentType::UNKNOWN,
                            audiosink::ContentType::SPEECH,
                            audiosink::ContentType::MUSIC,
                            audiosink::ContentType::MOVIE,
                            audiosink::ContentType::SONIFICATION,
                        };
                    } else {
                        capabilities.supportsSecure = false;
                        capabilities.supportedContentTypes = {
                            audiosink::ContentType::SPEECH,
                            audiosink::ContentType::MUSIC,
                        };
                    }
                    *_aidl_return = capabilities;
                    return android::binder::Status::ok();
                }


                android::binder::Status AudioSink::getProperty(audiosink::Property property, std::optional<com::rdk::hal::PropertyValue>* _aidl_return)
                {
                    if (!_aidl_return)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
                    }
                    com::rdk::hal::PropertyValue value;
                    switch (property) {
                        case audiosink::Property::RESOURCE_ID:
                        value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(this->id.value);
                        break;
                        case audiosink::Property::REFERENCE_LEVEL:
                        value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(g_reference_level[this->id.value]);
                        break;
                        case audiosink::Property::AV_SOURCE:
                        value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(static_cast<int32_t>(g_av_source[this->id.value]));
                        break;
                        //TBD
                        case audiosink::Property::RENDER_LATENCY_NS:
                        value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(0);
                        break;
                        //TBD
                        case audiosink::Property::QUEUE_DEPTH_NS:
                        value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(0);
                        break;
                        case audiosink::Property::MIXER_ID:
                        value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(g_mixer_id[this->id.value]);
                        break;
                        //TBD
                        case audiosink::Property::METRIC_xxxx:
                        value.value = com::rdk::hal::PropertyValue::Value::make<com::rdk::hal::PropertyValue::Value::intValue>(/* metric value */ 100);
                        break;
                        default:
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    *_aidl_return = value;
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSink::getState(  State* _aidl_return  )
                {
                    if (this->id.value < 0 || this->id.value >= NUM_AUDIO_SINK) {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    *_aidl_return = g_audiosink_state[this->id.value];
                    return android::binder::Status::ok();
                }


                android::binder::Status AudioSink::open(ContentType contentType, const android::sp<audiosink::IAudioSinkControllerListener>& audioSinkControllerListener, android::sp<audiosink::IAudioSinkController>* _aidl_return  )
                {
                    if (this->id.value < 0 || this->id.value >= NUM_AUDIO_SINK)
                    {
                        *_aidl_return = nullptr;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    if(audioSinkControllerListener == nullptr)
                    {
                        *_aidl_return = nullptr;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
                    }

                    if(g_audiosink_state[this->id.value] != State::CLOSED)
                    {
                        *_aidl_return = nullptr;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }

                    const int32_t contentTypeValue = static_cast<int32_t>(contentType);
                    if (contentTypeValue < static_cast<int32_t>(ContentType::UNKNOWN) ||
                        contentTypeValue > static_cast<int32_t>(ContentType::SONIFICATION))
                    {
                        *_aidl_return = nullptr;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    g_audioSinkControllerListeners[this->id.value] = audioSinkControllerListener;

                    g_audiodecoder_id[this->id.value].value = audiodecoder::IAudioDecoder::Id::UNDEFINED;
                    g_audiodecoder_selected[this->id.value] = false;
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from CLOSED to OPENING...\n";
                            g_audiosink_state[this->id.value] = State::OPENING;
                            std::cout << "[SERVICE]::g_audiosink_state value :"<< static_cast<int>(g_audiosink_state[this->id.value])<<"\n";//added
                        }
                        //average wait time to simulate transitory state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        std::cout << "[SERVICE]::Calling onStateChanged for OPENING...\n";
                        notifyStateChanged(this->id.value, State::CLOSED, State::OPENING);
                        state_cv.notify_all();
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from OPENING to READY...\n";
                            g_audiosink_state[this->id.value] = State::READY;
                            std::cout << "[SERVICE]::g_audiosink_state value :"<< static_cast<int>(g_audiosink_state[this->id.value])<<"\n";//added
                        }
                        //average wait time to simulate target state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        std::cout << "[SERVICE]::Calling onStateChanged for READY...\n";
                        notifyStateChanged(this->id.value, State::OPENING, State::READY);
                        state_cv.notify_all();
                    }
                    *_aidl_return = new AudioSinkController(this->id);
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSink::close(  const android::sp<audiosink::IAudioSinkController>& audioSinkController, bool* _aidl_return  )
                {
                    if (this->id.value < 0 || this->id.value >= NUM_AUDIO_SINK)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    if(g_audiosink_state[this->id.value] != State::READY)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from READY to CLOSING...\n";
                            g_audiosink_state[this->id.value] = State::CLOSING;
                        }
                        //average wait time to simulate transitory state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        std::cout << "[SERVICE]::Calling onStateChanged for CLOSING...\n";
                        notifyStateChanged(this->id.value, State::READY, State::CLOSING);
                        state_cv.notify_all();
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from CLOSING to CLOSED...\n";
                            g_audiosink_state[this->id.value] = State::CLOSED;
                            g_audiodecoder_id[this->id.value].value = audiodecoder::IAudioDecoder::Id::UNDEFINED;
                            g_audiodecoder_selected[this->id.value] = false;
                        }
                        //average wait time to simulate target state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        std::cout << "[SERVICE]::Calling onStateChanged for CLOSED...\n";
                        notifyStateChanged(this->id.value, State::CLOSING, State::CLOSED);
                        state_cv.notify_all();
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            g_audioSinkControllerListeners[this->id.value] = nullptr;
                        }
                    }
                    * _aidl_return = true;
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSink::registerEventListener(  const android::sp<audiosink::IAudioSinkEventListener>& audioSinkEventListener, bool* _aidl_return  )
                {
                    if (this->id.value < 0 || this->id.value >= NUM_AUDIO_SINK)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    if(audioSinkEventListener == nullptr)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
                    }
                    if(g_audioSinkEventListeners[this->id.value] != nullptr)
                    {
                        std::cout << "[SERVICE]::eventListener for ID : " <<this->id.value<<" is already registered ...exiting...\n";//added
                        *_aidl_return = false;
                        this->event_listener = true;
                        return android::binder::Status::ok();
                    }
                    g_audioSinkEventListeners[this->id.value] = audioSinkEventListener;
                    *_aidl_return = true;
                    this->event_listener = true;
                    std::cout << "[SERVICE]::eventListener for ID : " <<this->id.value<<" is registered successfully :: eventListener value : " << this->event_listener<<"\n";//added
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSink::unregisterEventListener(  const android::sp<audiosink::IAudioSinkEventListener>& audioSinkEventListener, bool* _aidl_return  )
                {
                    if (this->id.value < 0 || this->id.value >= NUM_AUDIO_SINK)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    if(g_audioSinkEventListeners[this->id.value] == nullptr)
                    {
                        std::cout << "[SERVICE]::eventListener for ID : " <<this->id.value<<" is already unregistered ...exiting...\n";//added
                        *_aidl_return = false;
                        this->event_listener = false;
                        return android::binder::Status::ok();
                    }

                    if(audioSinkEventListener == nullptr)
                    {
                        std::cout << "[SERVICE]::audioSinkEventListener for ID : " <<this->id.value<<" is NULL ...exiting...\n";//added
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
                    }
                    *_aidl_return = true;
                    std::cout << "[SERVICE]::eventListener for ID : " <<this->id.value<<" is unregistered successfully:: eventListener value : " << this->event_listener << "\n";//added
                    this->event_listener = false;
                    g_audioSinkEventListeners[this->id.value] = nullptr;
                    return android::binder::Status::ok();
                }

                AudioSinkController::AudioSinkController(IAudioSink::Id id)
                {
                    this->id = id;
                    this->id.value = canonicalSinkIndex(id.value);
                    if (this->id.value >= 0 && this->id.value < NUM_AUDIO_SINK)
                    {
                        g_audiodecoder_id[this->id.value].value = audiodecoder::IAudioDecoder::Id::UNDEFINED;
                        g_audiodecoder_selected[this->id.value] = false;
                    }
                    printf("AudioSinkController service initialized\n");
                }

                AudioSinkController::~AudioSinkController()
                {
                    printf("AudioSinkController service destroyed\n");
                }

                android::binder::Status AudioSinkController::setAudioDecoder(  const ::com::rdk::hal::audiodecoder::IAudioDecoder::Id& audioDecoderId, bool* _aidl_return  )
                {
                    if(this->id.value < 0 || this->id.value >= NUM_AUDIO_SINK)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    if(g_audiosink_state[this->id.value] != State::READY)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }

                    if(audioDecoderId.value < 0 || audioDecoderId.value > 2)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    g_audiodecoder_id[this->id.value] = audioDecoderId;
                    g_audiodecoder_selected[this->id.value] = true;
                    *_aidl_return = true;
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::setProperty( audiosink::Property property, const PropertyValue& propertyValue, bool* _aidl_return )
                {
                    int32_t intvalue;
                    *_aidl_return = true;
                    switch (property)
                    {
                        case audiosink::Property::RESOURCE_ID:
                        case audiosink::Property::RENDER_LATENCY_NS:
                        case audiosink::Property::QUEUE_DEPTH_NS:
                            *_aidl_return = false;
                            return android::binder::Status::fromExceptionCode(android::binder::Status::EX_UNSUPPORTED_OPERATION);
                        case audiosink::Property::REFERENCE_LEVEL:
                            if(g_audiosink_state[this->id.value] != State::READY)
                            {
                                *_aidl_return = false;
                                return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                            }
                            intvalue  = propertyValue.value->get<PropertyValue::Value::Tag::intValue>();

                            //TBD
                            if(intvalue < -31 || intvalue > 31)
                            {
                                *_aidl_return = false;
                                return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                            }
                            g_reference_level[this->id.value] = intvalue;
                            break;
                        case audiosink::Property::AV_SOURCE:
                            if(g_audiosink_state[this->id.value] != State::READY)
                            {
                                *_aidl_return = false;
                                return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                            }
                            intvalue = propertyValue.value->get<PropertyValue::Value::Tag::intValue>();
                            if (intvalue < static_cast<int32_t>(AVSource::AUTO) || intvalue > static_cast<int32_t>(AVSource::COMPOSITE_5))
                            {
                                *_aidl_return = false;
                                return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                            }
                            g_av_source[this->id.value] = static_cast<AVSource>(intvalue);
                            break;
                        case audiosink::Property::MIXER_ID:
                            if(g_audiosink_state[this->id.value] != State::READY)
                            {
                                *_aidl_return = false;
                                return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                            }
                            intvalue = propertyValue.value->get<PropertyValue::Value::Tag::intValue>();

                            if(intvalue < -1)
                            {
                                *_aidl_return = false;
                                return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                            }
                            g_mixer_id[this->id.value] = intvalue;
                            break;
                        default:
                            *_aidl_return = false;
                            return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::getAudioDecoder(  ::com::rdk::hal::audiodecoder::IAudioDecoder::Id* _aidl_return  )
                {
                    if(this->id.value < 0 || this->id.value >= NUM_AUDIO_SINK)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    if(g_audiosink_state[this->id.value] != State::READY && g_audiosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    if(!g_audiodecoder_selected[this->id.value])
                    {
                        _aidl_return->value = audiodecoder::IAudioDecoder::Id::UNDEFINED;
                        return android::binder::Status::ok();
                    }
                    *_aidl_return = g_audiodecoder_id[this->id.value];
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::attachClock(const ::com::rdk::hal::avclock::IAVClock::Id& clockId)
                {
                    if(this->id.value < 0 || this->id.value >= NUM_AUDIO_SINK)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    if(g_audiosink_state[this->id.value] != State::READY && g_audiosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }

                    if (clockId.value == avclock::IAVClock::Id::UNDEFINED)
                    {
                        g_avclock_id[this->id.value].value = avclock::IAVClock::Id::UNDEFINED;
                        return android::binder::Status::ok();
                    }

                    if (clockId.value < 0 || clockId.value > 2)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    g_avclock_id[this->id.value] = clockId;
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::detachClock()
                {
                    if(g_audiosink_state[this->id.value] != State::READY && g_audiosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    g_avclock_id[this->id.value].value = avclock::IAVClock::Id::UNDEFINED;
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::getClock(::com::rdk::hal::avclock::IAVClock::Id* _aidl_return)
                {
                    if(g_audiosink_state[this->id.value] != State::READY && g_audiosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    *_aidl_return = g_avclock_id[this->id.value];
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::start()
                {
                    if(g_audiosink_state[this->id.value] != State::READY)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from READY to STARTING...\n";
                            g_audiosink_state[this->id.value] = State::STARTING;
                        }
                        //average wait time to simulate transitory state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        std::cout << "[SERVICE]::Calling onStateChanged for STARTING...\n";
                        notifyStateChanged(this->id.value, State::READY, State::STARTING);
                        state_cv.notify_all();
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from STARTING to STARTED...\n";
                            g_audiosink_state[this->id.value] = State::STARTED;
                        }
                        //average wait time to simulate target state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        std::cout << "[SERVICE]::Calling onStateChanged for STARTED...\n";
                        notifyStateChanged(this->id.value, State::STARTING, State::STARTED);
                        state_cv.notify_all();
                    }
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::stop()
                {
                    if(g_audiosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }

                    // Stop any ongoing volume ramp
                    stopRequested.store(true); // Request ramp thread to stop
                    if (rampThread.joinable()) {
                        rampThread.join();  // Ensure the ramping thread exits before proceeding
                    }

                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from STARTED to STOPPING...\n";
                            g_audiosink_state[this->id.value] = State::STOPPING;
                        }
                        //average wait time to simulate transitory state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        std::cout << "[SERVICE]::Calling onStateChanged for STOPPING...\n";
                        notifyStateChanged(this->id.value, State::STARTED, State::STOPPING);
                        state_cv.notify_all();
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from STOPPING to READY...\n";
                            g_audiosink_state[this->id.value] = State::READY;
                        }
                        //average wait time to simulate target state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        std::cout << "[SERVICE]::Calling onStateChanged for READY...\n";
                        notifyStateChanged(this->id.value, State::STOPPING, State::READY);
                        state_cv.notify_all();
                    }
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::queueAudioFrame(  int64_t nsPresentationTime, int64_t bufferHandle, const ::com::rdk::hal::audiodecoder::FrameMetadata& metadata, bool* _aidl_return  )
                {
                    if(_aidl_return == nullptr)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
                    }

                    *_aidl_return = false;

                    if(this->id.value < 0 || this->id.value >= NUM_AUDIO_SINK)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    if(g_audiosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }

                    if (nsPresentationTime < 0 || bufferHandle < 0)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    if (!metadata.metadata.has_value() || metadata.metadata->numChannels <= 0 ||
                        metadata.metadata->sampleRate <= 0)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    const auto expectedChannels = static_cast<size_t>(metadata.metadata->numChannels);
                    if (!metadata.metadata->channelTypes.empty() && metadata.metadata->channelTypes.size() != expectedChannels)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    *_aidl_return = true;
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::signalEndOfStream()
                {
                    if(g_audiosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::flush()
                {
                    if(g_audiosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from STARTED to FLUSHING...\n";
                            g_audiosink_state[this->id.value] = State::FLUSHING;
                        }
                        //average wait time to simulate transitory state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        std::cout << "[SERVICE]::Calling onStateChanged for FLUSHING...\n";
                        notifyStateChanged(this->id.value, State::STARTED, State::FLUSHING);
                        state_cv.notify_all();
                    }
                    {
                        {
                            std::lock_guard <std::mutex> lock(state_mutex);
                            std::cout << "[SERVICE]::Changing state from FLUSHING to STARTED...\n";
                            g_audiosink_state[this->id.value] = State::STARTED;
                        }
                        //average wait time to simulate target state change
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        std::cout << "[SERVICE]::Calling onStateChanged for STARTED...\n";
                        notifyStateChanged(this->id.value, State::FLUSHING, State::STARTED);
                        state_cv.notify_all();
                    }
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::getVolume(  com::rdk::hal::audiosink::Volume* _aidl_return  )
                {
                    if(g_audiosink_state[this->id.value] != State::READY && g_audiosink_state[this->id.value] != State::STARTED)
                    {
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }

                    // Ensure reads observe the finalized target value when a ramp is
                    // still winding down near the test's sampling boundary.
                    if (rampThread.joinable()) {
                        rampThread.join();
                    }

                    *_aidl_return = g_volume[this->id.value];
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::setVolume(  const com::rdk::hal::audiosink::Volume& volume, bool* _aidl_return  )
                {
                    if(g_audiosink_state[this->id.value] != State::READY && g_audiosink_state[this->id.value] != State::STARTED)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }
                    if(volume.volume < 0.0 || volume.volume > 1.0)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }
                    g_volume[this->id.value] = volume;
                    *_aidl_return = true;
                    return android::binder::Status::ok();
                }

                android::binder::Status AudioSinkController::setVolumeRamp(  double targetVolume, int32_t overMs, com::rdk::hal::audiosink::VolumeRamp volumeRamp, bool* _aidl_return  )
                {
                    //std::lock_guard<std::mutex> lock(ramp_mutex);

                    if (g_audiosink_state[this->id.value] != State::STARTED)
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
                    }

                    //Validate inputs
                    if (targetVolume < 0.0 || targetVolume > 1.0 || overMs <= 0 ||
                        (volumeRamp != VolumeRamp::LINEAR && volumeRamp != VolumeRamp::IN_CUBIC && volumeRamp != VolumeRamp::OUT_CUBIC))
                    {
                        *_aidl_return = false;
                        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
                    }

                    //Stop any existing ramp before starting a new one
                    stopRequested.store(true);
                     if (rampThread.joinable()) {
                        rampThread.join();  // Wait for the previous ramp to exit completely
                    }

                    stopRequested.store(false);  // Reset for the new ramp

                    // Start a new volume ramp
                    isRamping.store(true);
                    rampThread = std::thread([this, targetVolume, overMs, volumeRamp]() {
                        std::unique_lock<std::mutex> rampLock(ramp_mutex);

                        double startVolume = g_volume[this->id.value].volume;
                        int steps = overMs / stepIntervalMs;  // Calculate the number of steps
                        if (steps < 1) steps = 1;

                        for (int i = 0; i < steps; i++) {
                            if (stopRequested.load()) {
                                g_volume[this->id.value].volume = targetVolume;
                                break;  // Stop if another ramp starts
                            }

                            double progress = static_cast<double>(i + 1) / steps;
                            switch (volumeRamp) {
                                case VolumeRamp::LINEAR:
                                    g_volume[this->id.value].volume = startVolume + (targetVolume - startVolume) * progress;
                                    break;
                                case VolumeRamp::IN_CUBIC:
                                    g_volume[this->id.value].volume = startVolume + (targetVolume - startVolume) * (progress * progress * progress);
                                    break;
                                case VolumeRamp::OUT_CUBIC:
                                    double t = progress - 1;
                                    g_volume[this->id.value].volume = startVolume + (targetVolume - startVolume) * (t * t * t + 1);
                                    break;
                            }

                            // Do not wait after the final step so target volume is settled
                            // strictly within the requested ramp duration.
                            if (i + 1 < steps) {
                                conditionVar.wait_for(rampLock, std::chrono::milliseconds(stepIntervalMs));
                            }
                        }

                        // Ensure final volume is set
                        g_volume[this->id.value].volume = targetVolume;
                        isRamping.store(false);
                    });

                    *_aidl_return = true;
                    return android::binder::Status::ok();
                }
            }
        }
    }
}

