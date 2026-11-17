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
* AudioSink.h
*
* Header file for the AudioSink service implementation
*/

#ifndef AUDIOSINK_H
#define AUDIOSINK_H

#include <com/rdk/hal/audiosink/BnAudioSinkManager.h>
#include <com/rdk/hal/audiosink/IAudioSink.h>
#include <com/rdk/hal/audiosink/BnAudioSink.h>
#include <com/rdk/hal/audiosink/BnAudioSinkController.h>
#include <com/rdk/hal/audiosink/BnAudioSinkEventListener.h>
#include <com/rdk/hal/avclock/IAVClock.h>
#include <binder/BinderService.h>
#include <optional>
#include <string>
#include <utils/StrongPointer.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <future>
#include <chrono>
#include <thread>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <atomic>

namespace com {
    namespace rdk {
        namespace hal {
            namespace audiosink {

#define NUM_AUDIO_SINK 2

              class AudioSinkManager :public android::BinderService<AudioSinkManager>,
                                      public BnAudioSinkManager {
              public:
                 AudioSinkManager();
                 virtual ~AudioSinkManager();

                 static char const* getServiceName() {
                     return IAudioSinkManager::serviceName().c_str(); }

                 android::binder::Status getAudioSinkIds( std::vector<IAudioSink::Id>* _aidl_return ) override;
				 android::binder::Status getPlatformCapabilities( PlatformCapabilities* _aidl_return ) override;
				 android::binder::Status getAudioSink( const IAudioSink::Id& audioSinkId, android::sp<IAudioSink>* _aidl_return ) override;
                 };

              class AudioSink :public android::BinderService<AudioSink>,
                          public BnAudioSink {

              public:
                 IAudioSink::Id id;
                 bool event_listener;
                 AudioSink(IAudioSink::Id id);
                 virtual ~AudioSink();

                 android::binder::Status getCapabilities( audiosink::Capabilities* _aidl_return ) override;
                 android::binder::Status getProperty( audiosink::Property property, std::optional<PropertyValue>* _aidl_return ) override;
                 android::binder::Status getState( State* _aidl_return ) override;
                 android::binder::Status open(ContentType contentType, const android::sp<audiosink::IAudioSinkControllerListener>& audioSinkControllerListener, android::sp<audiosink::IAudioSinkController>* _aidl_return ) override;
                 android::binder::Status close( const android::sp<audiosink::IAudioSinkController>& audioSinkController, bool* _aidl_return ) override;
                 android::binder::Status registerEventListener( const android::sp<audiosink::IAudioSinkEventListener>& audioSinkEventListener, bool* _aidl_return ) override;
                 android::binder::Status unregisterEventListener( const android::sp<audiosink::IAudioSinkEventListener>& audioSinkEventListener, bool* _aidl_return ) override;
                 };

              class AudioSinkController :public android::BinderService<AudioSinkController>,
                          public BnAudioSinkController {
              private:
                  IAudioSink::Id id;
                  std::atomic<bool> stopRequested{false};
                  std::atomic<bool> isRamping{false};
                  //TBD
                  static constexpr int stepIntervalMs = 50;
                  std::thread rampThread;

              public:
                 bool event_listener = false;
                 AudioSinkController(IAudioSink::Id id);
                 virtual ~AudioSinkController();

                 android::binder::Status setAudioDecoder( const ::com::rdk::hal::audiodecoder::IAudioDecoder::Id& audioDecoderId, bool* _aidl_return ) override;
                 android::binder::Status getAudioDecoder( ::com::rdk::hal::audiodecoder::IAudioDecoder::Id* _aidl_return ) override;
                 android::binder::Status attachClock(const ::com::rdk::hal::avclock::IAVClock::Id& clockId) override;
                 android::binder::Status detachClock() override;
                 android::binder::Status getClock(::com::rdk::hal::avclock::IAVClock::Id* _aidl_return) override;
                 android::binder::Status setProperty( audiosink::Property property, const PropertyValue& propertyValue, bool* _aidl_return ) override;
                 android::binder::Status start() override;
                 android::binder::Status stop() override;
                 android::binder::Status queueAudioFrame( int64_t nsPresentationTime, int64_t bufferHandle, const ::com::rdk::hal::audiodecoder::FrameMetadata& metadata, bool* _aidl_return ) override;
                 android::binder::Status signalEndOfStream() override;
                 android::binder::Status flush() override;
                 android::binder::Status getVolume( com::rdk::hal::audiosink::Volume* _aidl_return ) override;
                 android::binder::Status setVolume( const com::rdk::hal::audiosink::Volume& volume, bool* _aidl_return ) override;
                 android::binder::Status setVolumeRamp( double targetVolume, int32_t overMs, com::rdk::hal::audiosink::VolumeRamp volumeRamp, bool* _aidl_return ) override;
                 };
            }
        }
    }
}
#endif AUDIOSINK_H