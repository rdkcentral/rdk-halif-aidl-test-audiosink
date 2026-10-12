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

void AudioSinkTest::run()
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

void AudioSinkTest::setupClock()
{
    // Open the clock
    AVCLOCK_MANAGER_GETAVCLOCK_SUCCESS(mClockId, mClock);

    // Open the clock controller
    AVCLOCK_OPEN_SUCCESS(mClock, mClockCtrlListener, mClockController,true);

    // Set the clock mode to Audio Master
    AVCLOCK_SET_CLOCK_MODE_SUCCESS(mClock, mClockController, ClockMode::AUDIO_MASTER, true);

    // Start the clock
    AVCLOCK_START_SUCCESS(mClock, mClockController, true);
}

void AudioSinkTest::tearDownClock()
{
    // Stop the clock
    AVCLOCK_STOP_SUCCESS(mClock, mClockController, true);

    // Close the clock controller
    AVCLOCK_CLOSE_SUCCESS(mClock, mClockController, true);
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

    if(!mClockInUse)
    {
        tearDownClock();
    }
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
    /* Control listener */
    int64_t firstFrameRenderedPTS = -1;
    UT_ASSERT_TRUE(mControlListener->getNextFirstFrameRenderedPTS(firstFrameRenderedPTS));
    UT_ASSERT_EQUAL(expectedFirstFramePTS, firstFrameRenderedPTS);

    if (expectUnderflow)
    {
        int64_t resumedPTS = -1;
        UT_ASSERT_TRUE(mControlListener->getUnderFlowCount(true) > 0);
        UT_ASSERT_TRUE(mControlListener->getNextAudioResumedPTS(resumedPTS));
        UT_ASSERT_TRUE(resumedPTS >= 0);
    }

    /* Event listener */
    firstFrameRenderedPTS = -1;
    UT_ASSERT_TRUE(mEventListener->getNextFirstFrameRenderedPTS(firstFrameRenderedPTS));
    UT_ASSERT_EQUAL(expectedFirstFramePTS, firstFrameRenderedPTS);

    if (expectUnderflow)
    {
        int64_t resumedPTS = -1;
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

void AudioSinkTest::queueDecodedFrameIfNonTunnel(const DecodedAudioFrame_t& decodedFrame,
                                                  audiodecoder::FrameMetadata& metadata,
                                                  bool& flushDone)
{
    if (decodedFrame.isTunnel)
    {
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

    bool result = false;
    AUDIOSINK_QUEUEAUDIOFRAME_SUCCESS(mAudioSinkController, decodedFrame.framePTS,
                                      decodedFrame.frameBufferHandle, metadata, result);
    if (!result)
    {
        UT_LOG_WARNING("[SinkID: %d] Failed to queue frame to audio sink, PTS: %lld, Buffer Handle: 0x%016" PRIx64,
                       mSinkId.value, decodedFrame.framePTS, decodedFrame.frameBufferHandle);
        AVBUFFER_FREE_SUCCESS(mAVBufferManager, decodedFrame.frameBufferHandle);
    }
}

void AudioSinkTest::audioSinkNonTunnelTest()
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
        avSourcePropertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(int32_t(static_cast<int32_t>(AVSource::IP)));

        UT_LOG_INFO("[SinkID: %d] Start of Testing Stream: %s", mSinkId.value, mTestConfiguration.input.c_str());

        setupAudioSink();

        while(mdecoderControlListener->getNextFrame(decodedFrame, 500, true))
        {
            if (decodedFrame.isTunnel != mIsTunnel)
            {
                mFrameOutputModeMismatch = true;
                if (!decodedFrame.isTunnel)
                {
                    AVBUFFER_FREE_SUCCESS(mAVBufferManager, decodedFrame.frameBufferHandle);
                }
                continue;
            }

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

            queueDecodedFrameIfNonTunnel(decodedFrame, metadata, flushDone);

            /* Intentional delay to starve the sink queue and trigger onAudioUnderflow.
             * decoderDelay is sourced from delay_between_decode_calls in the YAML
             * (e.g. vts_l3_AudioDecode_Underflow.yaml sets this to 200 ms).
             * The delay is applied here, in the sink queueing path, not in the decoder
             * submission path, because only a gap between queueAudioFrame calls can
             * empty the sink's internal PCM queue while the clock is running. */
            if (!decodedFrame.isTunnel && mTestConfiguration.decoderDelay > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(mTestConfiguration.decoderDelay));
            }

            std::optional<PropertyValue> latencyValue;
            AUDIOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, audiosink::Property::RENDER_LATENCY_NS, latencyValue);
            UT_LOG_INFO("[SinkID: %d] Render Latency: %lld ns", mSinkId.value, latencyValue->value->get<PropertyValue::Value::Tag::longValue>());
        }

        /* Signal EOS to the sink after all frames have been queued */
        AUDIOSINK_SIGNAL_ENDOFSTREAM_SUCCESS(mAudioSinkController);

        /* Validate callbacks only for the requested decoder output mode. */
        if (!mFrameOutputModeMismatch)
        {
            validateCallbacks(firstFramePTS, mTestConfiguration.decoderDelay > 0, mTestConfiguration.decoderDelay > 0);
        }

        /* Validate the metrics */
        validateMetric();

        /* Stop the Audio sink */
        tearDownAudioSink();
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

void AudioSinkTest::audioSinkTunnelTest()
{
    try
    {
        Pool audioPool;
        PropertyValue avSourcePropertyValue;
        DecodedAudioFrame_t decodedFrame;
        int32_t frameCount = 0;
        int64_t firstFramePTS = -1;
        bool flushDone = false;
        avSourcePropertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(int32_t(static_cast<int32_t>(AVSource::IP)));

        UT_LOG_INFO("[SinkID: %d] Start of Testing Stream: %s", mSinkId.value, mTestConfiguration.input.c_str());

        setupAudioSink();

        while(mdecoderControlListener->getNextFrame(decodedFrame, 500, true))
        {
            if (decodedFrame.isTunnel != mIsTunnel)
            {
                mFrameOutputModeMismatch = true;
                if (!decodedFrame.isTunnel)
                {
                    AVBUFFER_FREE_SUCCESS(mAVBufferManager, decodedFrame.frameBufferHandle);
                }
                continue;
            }

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

            if (mTestConfiguration.sinkFlushPts > 0 &&
                decodedFrame.framePTS >= mTestConfiguration.sinkFlushPts && !flushDone)
            {
                AUDIOSINK_FLUSH_SUCCESS(mSinkHandle, mAudioSinkController, true);
                flushDone = true;
            }

            std::optional<PropertyValue> latencyValue;
            AUDIOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, audiosink::Property::RENDER_LATENCY_NS, latencyValue);
            UT_LOG_INFO("[SinkID: %d] Render Latency: %lld ns", mSinkId.value, latencyValue->value->get<PropertyValue::Value::Tag::longValue>());
        }

        /* Signal EOS to the sink (0.21.0 requirement — identical behaviour in tunnel and non-tunnel modes) */
        AUDIOSINK_SIGNAL_ENDOFSTREAM_SUCCESS(mAudioSinkController);

        /* Validate callbacks only for the requested decoder output mode. */
        if (!mFrameOutputModeMismatch)
        {
            validateCallbacks(firstFramePTS, mTestConfiguration.decoderDelay > 0, mTestConfiguration.decoderDelay > 0);
        }

        /* Validate the metrics */
        validateMetric();

        /* Stop the Audio sink */
        tearDownAudioSink();
    }
    catch (const std::exception& ex)
    {
        UT_LOG_ERROR("[SinkID: %d] Exception in audioSinkTunnelTest: %s", mSinkId.value, ex.what());
        UT_ASSERT_TRUE(false);
    }
    catch (...)
    {
        UT_LOG_ERROR("[SinkID: %d] Unknown exception in audioSinkTunnelTest", mSinkId.value);
        UT_ASSERT_TRUE(false);
    }
}

/** @} */ // End of AUDIO_SINK_HALTEST_CLASS
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK
/** @} */ // End of HPK
