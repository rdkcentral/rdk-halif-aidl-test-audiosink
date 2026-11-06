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
 * @file vts_audiosink.cpp
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

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace com::rdk::hal::audiosink;

#define MAX_WAIT_TIME 1000

/**
 * Override Call-back funtion for decoder state changes
 *
 * @param oldState Old state
 * @param newState New state
 * @return Status
 */
Status AudioSinkControllerListener::onStateChanged(::com::rdk::hal::audiosink::State oldState, ::com::rdk::hal::audiosink::State newState)
{
    UT_LOG_INFO("[Audio SinkID: %d] ControllerListener State Change received: %s -> %s\n", mSinkID, com::rdk::hal::audiosink::toString(oldState).c_str(), com::rdk::hal::audiosink::toString(newState).c_str());
    {
        std::lock_guard<std::mutex> lock(mStateMutex);
        mStateChanges.emplace_back(oldState, newState);
        mCurrentState = newState;
    }
    mCV.notify_all(); // Unblock any waiting test code
    return Status::ok();
}

/**
 * Override Call-back function for first frame rendered event
 *
 * @param nsPresentationTime Presentation time of the first frame rendered
 * @return Status
 */
Status AudioSinkControllerListener::onFirstFrameRendered(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Audio SinkID: %d] ControllerListener First Frame Rendered received: %lld\n", mSinkID, nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mFFMutex);
        mFirstframerendered.push(nsPresentationTime);
    }
    mFFRenderAvailable.notify_all(); // Notify any waiting threads that a frame is available
    return Status::ok();
}

/**
 * Get the next first frame rendered PTS from the queue
 *
 * @param firstFramePTS Reference to store the first frame PTS
 * @param popFrame If true, pop the frame from the queue after retrieving it
 * @return true if a frame was successfully retrieved, false if timeout occurred
 */
bool AudioSinkControllerListener::getNextFirstFrameRenderedPTS(int64_t &firstFramePTS)
{
    std::unique_lock<std::mutex> lock(mFFMutex);

    // Wait until queue is not empty or timeout occurs
    bool gotEvent = mFFRenderAvailable.wait_for(
        lock,
        std::chrono::milliseconds(MAX_WAIT_TIME),
        [this]() { return !mFirstframerendered.empty(); }
    );

    if (!gotEvent)
    {
        UT_LOG_WARNING("[Audio SinkID: %d] ControllerListener First Frame Rendered Event not received after %d msec", mSinkID, MAX_WAIT_TIME);
        return false;
    }

    firstFramePTS = mFirstframerendered.front();
    mFirstframerendered.pop();

    return true;
}

/**
 * Override Call-back function for frame buffer available event
 *
 * @param None
 * @return Status
 */
Status AudioSinkControllerListener::onFrameBufferAvailable()
{
    UT_LOG_INFO("[Audio SinkID: %d] ControllerListener Frame Buffer Available received\n", mSinkID);
    {
        std::lock_guard<std::mutex> lock(mFrameBufferAvailableMutex);
        mFrameBufferAvailableCount++;
    }
    return Status::ok();
}

int32_t AudioSinkControllerListener::getFrameBufferAvailableCount(bool reset)
{
    std::lock_guard<std::mutex> lock(mFrameBufferAvailableMutex);
    int32_t count = mFrameBufferAvailableCount;
    if (reset)
    {
        mFrameBufferAvailableCount = 0; // Reset the count if requested
    }
    return count;
}

/**
 * Override Call-back function for end of stream event
 *
 * @param nsPresentationTime Presentation time of the end of stream
 * @return Status
 */
Status AudioSinkControllerListener::onEndOfStream(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Audio SinkID: %d] ControllerListener EOS received: %lld\n", mSinkID, nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mEOSMutex);
        mEndofstream.push(nsPresentationTime);
    }
    mEOSAvailable.notify_all(); // Notify any waiting threads that EOS is available
    return Status::ok();
}

/**
 * Get the next end of stream PTS from the queue
 *
 * @param endOfStreamPTS Reference to store the end of stream PTS
 * @param popFrame If true, pop the frame from the queue after retrieving it
 * @return true if a frame was successfully retrieved, false if timeout occurred
 */
bool AudioSinkControllerListener::getNextEndOfStreamPTS(int64_t &endOfStreamPTS)
{
    std::unique_lock<std::mutex> lock(mEOSMutex);
    // Wait until queue is not empty or timeout occurs
    bool gotEvent = mEOSAvailable.wait_for(
        lock,
        std::chrono::milliseconds(MAX_WAIT_TIME),
        [this]() { return !mEndofstream.empty(); }
    );
    if (!gotEvent)
    {
        UT_LOG_WARNING("[Audio SinkID: %d] ControllerListener End of Stream Event not received after %d msec", mSinkID, MAX_WAIT_TIME);
        return false;
    }
    endOfStreamPTS = mEndofstream.front();
    mEndofstream.pop();
    return true;
}

/**
 * Override Call-back function for audio underflow event
 *
 * @return Status
 */
Status AudioSinkControllerListener::onAudioUnderflow()
{
    UT_LOG_INFO("[Audio SinkID: %d] ControllerListener Underflow Event received\n", mSinkID);
    {
        std::lock_guard<std::mutex> lock(mUnderFlowMutex);
        mUnderFlowCount++;
    }
    return Status::ok();
}

/**
 * Get the current underflow count
 *
 * @param reset If true, reset the count after retrieving it
 * @return The current underflow count
 */
int32_t AudioSinkControllerListener::getUnderFlowCount(bool reset)
{
    std::lock_guard<std::mutex> lock(mUnderFlowMutex);
    int32_t count = mUnderFlowCount;
    if (reset)
    {
        mUnderFlowCount = 0; // Reset the count if requested
    }
    return count;
}

/**
 * Override Call-back function for audio resumed event
 *
 * @param nsPresentationTime Presentation time of the audio resumed
 * @return Status
 */
Status AudioSinkControllerListener::onAudioResumed(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Audio SinkID: %d] ControllerListener Resumed Event received: %lld\n", mSinkID, nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mResumeMutex);
        mAudioresumed.push(nsPresentationTime);
    }
    return Status::ok();
}

/**
 * Get the next audio resumed PTS from the queue
 *
 * @param audioResumedPTS Reference to store the audio resumed PTS
 * @param popFrame If true, pop the frame from the queue after retrieving it
 * @return true if a frame was successfully retrieved, false if timeout occurred
 */
bool AudioSinkControllerListener::getNextAudioResumedPTS(int64_t &audioResumedPTS)
{
    std::unique_lock<std::mutex> lock(mResumeMutex);
    // Wait until queue is not empty or timeout occurs
    bool gotEvent = mResumeAvailable.wait_for(
        lock,
        std::chrono::milliseconds(MAX_WAIT_TIME),
        [this]() { return !mAudioresumed.empty(); }
    );
    if (!gotEvent)
    {
        UT_LOG_WARNING("[Audio SinkID: %d] ControllerListener Audio Resumed Event not received after %d msec", mSinkID, MAX_WAIT_TIME);
        return false;
    }
    audioResumedPTS = mAudioresumed.front();
    mAudioresumed.pop();
    return true;
}

/**
 * Override Call-back function for flush complete event
 *
 * @return Status
 */
Status AudioSinkControllerListener::onFlushComplete()
{
    UT_LOG_INFO("[Audio SinkID: %d] ControllerListener Flush Complete Event received\n", mSinkID);
    {
        std::lock_guard<std::mutex> lock(mFlushMutex);
        mFlushCompleteCount++;
    }
    return Status::ok();
}
/**
 * Get the current flush complete count
 *
 * @param reset If true, reset the count after retrieving it
 * @return The current flush complete count
 */
int32_t AudioSinkControllerListener::getFlushCompleteCount(bool reset)
{
    std::lock_guard<std::mutex> lock(mFlushMutex);
    int32_t count = mFlushCompleteCount;
    if (reset)
    {
        mFlushCompleteCount = 0; // Reset the count if requested
    }
    return count;
}

/**
 * Wait for the state to change to the desired state within the specified timeout
 *
 * @param desiredState The state to wait for
 * @param timeout The maximum time to wait
 * @return true if the state changed to the desired state, false if timed out
 */
bool AudioSinkControllerListener::waitForState(::com::rdk::hal::audiosink::State desiredState, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    // Wait until mCurrentState == desiredState or until timeout
    return mCV.wait_for(lock, timeout, [this, desiredState]() {
        return (mCurrentState == desiredState);
    });
}

/**
 * Reset the state changes recorded by the controller listener
 */
void AudioSinkControllerListener::resetStateChanges()
{
    std::lock_guard<std::mutex> lock(mStateMutex);
    mStateChanges.clear();
}

/**
 * Check if the state transition from expOldState to expNewState has occurred
 *
 * @param expOldState Expected old state
 * @param expNewState Expected new state
 * @return true if the transition has occurred, false otherwise
 */
bool AudioSinkControllerListener::checkStateTransistion(::com::rdk::hal::audiosink::State expOldState, ::com::rdk::hal::audiosink::State expNewState)
{
    bool foundClosedToOpening = false;
    for (const auto &[oldState, newState] : mStateChanges)
    {
        if (oldState == expOldState && newState == expNewState)
        {
            foundClosedToOpening = true;
            break;
        }
    }
    return foundClosedToOpening;
}

/* Event listener Class Functions */

/**
 * Override Call-back funtion for decoder state changes
 *
 * @param oldState Old state
 * @param newState New state
 * @return Status
 */
Status AudioSinkEventListener::onStateChanged(::com::rdk::hal::audiosink::State oldState, ::com::rdk::hal::audiosink::State newState)
{
    UT_LOG_INFO("[Audio SinkID: %d] EventListener State Change received: %s -> %s\n", mSinkID, com::rdk::hal::audiosink::toString(oldState).c_str(), com::rdk::hal::audiosink::toString(newState).c_str());
    {
        std::lock_guard<std::mutex> lock(mStateMutex);
        mStateChanges.emplace_back(oldState, newState);
        mCurrentState = newState;
    }
    mCV.notify_all(); // Unblock any waiting test code
    return Status::ok();
}

/**
 * Override Call-back function for first frame rendered event
 *
 * @param nsPresentationTime Presentation time of the first frame rendered
 * @return Status
 */
Status AudioSinkEventListener::onFirstFrameRendered(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Audio SinkID: %d] EventListener First Frame Rendered received: %lld\n", mSinkID, nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mFFMutex);
        mFirstframerendered.push(nsPresentationTime);
    }
    mFFRenderAvailable.notify_all(); // Notify any waiting threads that a frame is available
    return Status::ok();
}

/**
 * Get the next first frame rendered PTS from the queue
 *
 * @param firstFramePTS Reference to store the first frame PTS
 * @param popFrame If true, pop the frame from the queue after retrieving it
 * @return true if a frame was successfully retrieved, false if timeout occurred
 */
bool AudioSinkEventListener::getNextFirstFrameRenderedPTS(int64_t &firstFramePTS)
{
    std::unique_lock<std::mutex> lock(mFFMutex);

    // Wait until queue is not empty or timeout occurs
    bool gotEvent = mFFRenderAvailable.wait_for(
        lock,
        std::chrono::milliseconds(MAX_WAIT_TIME),
        [this]() { return !mFirstframerendered.empty(); }
    );

    if (!gotEvent)
    {
        UT_LOG_WARNING("[Audio SinkID: %d] EventListener Frist Frame Rendered Event not received after %d msec", mSinkID, MAX_WAIT_TIME);
        return false;
    }

    firstFramePTS = mFirstframerendered.front();
    mFirstframerendered.pop();

    return true;
}


/**
 * Override Call-back function for end of stream event
 *
 * @param nsPresentationTime Presentation time of the end of stream
 * @return Status
 */
Status AudioSinkEventListener::onEndOfStream(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Audio SinkID: %d] EventListener EOS received: %lld\n", mSinkID, nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mEOSMutex);
        mEndofstream.push(nsPresentationTime);
    }
    mEOSAvailable.notify_all(); // Notify any waiting threads that EOS is available
    return Status::ok();
}

/**
 * Get the next end of stream PTS from the queue
 *
 * @param endOfStreamPTS Reference to store the end of stream PTS
 * @param popFrame If true, pop the frame from the queue after retrieving it
 * @return true if a frame was successfully retrieved, false if timeout occurred
 */
bool AudioSinkEventListener::getNextEndOfStreamPTS(int64_t &endOfStreamPTS)
{
    std::unique_lock<std::mutex> lock(mEOSMutex);
    // Wait until queue is not empty or timeout occurs
    bool gotEvent = mEOSAvailable.wait_for(
        lock,
        std::chrono::milliseconds(MAX_WAIT_TIME),
        [this]() { return !mEndofstream.empty(); }
    );
    if (!gotEvent)
    {
        UT_LOG_WARNING("[Audio SinkID: %d] EventListener End of Stream Event not received after %d msec", mSinkID, MAX_WAIT_TIME);
        return false;
    }
    endOfStreamPTS = mEndofstream.front();
    mEndofstream.pop();
    return true;
}

/**
 * Override Call-back function for audio underflow event
 *
 * @return Status
 */
Status AudioSinkEventListener::onAudioUnderflow()
{
    UT_LOG_INFO("[Audio SinkID: %d] EventListener Underflow Event received\n", mSinkID);
    {
        std::lock_guard<std::mutex> lock(mUnderFlowMutex);
        mUnderFlowCount++;
    }
    return Status::ok();
}

/**
 * Get the current underflow count
 *
 * @param reset If true, reset the count after retrieving it
 * @return The current underflow count
 */
int32_t AudioSinkEventListener::getUnderFlowCount(bool reset)
{
    std::lock_guard<std::mutex> lock(mUnderFlowMutex);
    int32_t count = mUnderFlowCount;
    if (reset)
    {
        mUnderFlowCount = 0; // Reset the count if requested
    }
    return count;
}

/**
 * Override Call-back function for audio resumed event
 *
 * @param nsPresentationTime Presentation time of the audio resumed
 * @return Status
 */
Status AudioSinkEventListener::onAudioResumed(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Audio SinkID: %d] EventListener Resumed Event received: %lld\n", mSinkID, nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mResumeMutex);
        mAudioresumed.push(nsPresentationTime);
    }
    return Status::ok();
}

/**
 * Get the next audio resumed PTS from the queue
 *
 * @param audioResumedPTS Reference to store the audio resumed PTS
 * @param popFrame If true, pop the frame from the queue after retrieving it
 * @return true if a frame was successfully retrieved, false if timeout occurred
 */
bool AudioSinkEventListener::getNextAudioResumedPTS(int64_t &audioResumedPTS)
{
    std::unique_lock<std::mutex> lock(mResumeMutex);
    // Wait until queue is not empty or timeout occurs
    bool gotEvent = mResumeAvailable.wait_for(
        lock,
        std::chrono::milliseconds(MAX_WAIT_TIME),
        [this]() { return !mAudioresumed.empty(); }
    );
    if (!gotEvent)
    {
        UT_LOG_WARNING("[Audio SinkID: %d] EventListener Audio Resumed Event not received after %d msec", mSinkID, MAX_WAIT_TIME);
        return false;
    }
    audioResumedPTS = mAudioresumed.front();
    mAudioresumed.pop();
    return true;
}

/**
 * Override Call-back function for flush complete event
 *
 * @return Status
 */
Status AudioSinkEventListener::onFlushComplete()
{
    UT_LOG_INFO("[Audio SinkID: %d] EventListener Flush Complete Event received\n", mSinkID);
    {
        std::lock_guard<std::mutex> lock(mFlushMutex);
        mFlushCompleteCount++;
    }
    return Status::ok();
}
/**
 * Get the current flush complete count
 *
 * @param reset If true, reset the count after retrieving it
 * @return The current flush complete count
 */
int32_t AudioSinkEventListener::getFlushCompleteCount(bool reset)
{
    std::lock_guard<std::mutex> lock(mFlushMutex);
    int32_t count = mFlushCompleteCount;
    if (reset)
    {
        mFlushCompleteCount = 0; // Reset the count if requested
    }
    return count;
}

/**
 * Wait for the state to change to the desired state within the specified timeout
 *
 * @param desiredState The state to wait for
 * @param timeout The maximum time to wait
 * @return true if the state changed to the desired state, false if timed out
 */
bool AudioSinkEventListener::waitForState(::com::rdk::hal::audiosink::State desiredState, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    // Wait until mCurrentState == desiredState or until timeout
    return mCV.wait_for(lock, timeout, [this, desiredState]() {
        return (mCurrentState == desiredState);
    });
}

/**
 * Reset the state changes recorded by the controller listener
 */
void AudioSinkEventListener::resetStateChanges()
{
    std::lock_guard<std::mutex> lock(mStateMutex);
    mStateChanges.clear();
}

/**
 * Check if the state transition from expOldState to expNewState has occurred
 *
 * @param expOldState Expected old state
 * @param expNewState Expected new state
 * @return true if the transition has occurred, false otherwise
 */
bool AudioSinkEventListener::checkStateTransistion(::com::rdk::hal::audiosink::State expOldState, ::com::rdk::hal::audiosink::State expNewState)
{
    bool foundClosedToOpening = false;
    for (const auto &[oldState, newState] : mStateChanges)
    {
        if (oldState == expOldState && newState == expNewState)
        {
            foundClosedToOpening = true;
            break;
        }
    }
    return foundClosedToOpening;
}

/** @} */ // End of AUDIO_SINK_HALTEST_CLASS
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK
/** @} */ // End of HPK
