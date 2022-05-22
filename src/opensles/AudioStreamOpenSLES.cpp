/* Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <sys/types.h>
#include <cassert>
#include "../common/log.h"


#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <oboe/AudioStream.h>
#include <common/AudioClock.h>

#include "common/OboeDebug.h"
#include "oboe/AudioStreamBuilder.h"
#include "AudioStreamOpenSLES.h"
#include "OpenSLESUtilities.h"

using namespace oboe;

AudioStreamOpenSLES::AudioStreamOpenSLES(const AudioStreamBuilder &builder)
    : AudioStreamBuffered(builder) {
    // OpenSL ES does not support device IDs. So overwrite value from builder.
    mDeviceId = kUnspecified;
    // OpenSL ES does not support session IDs. So overwrite value from builder.
    mSessionId = SessionId::None;
}

static constexpr int32_t   kHighLatencyBufferSizeMillis = 20; // typical Android period
static constexpr SLuint32  kAudioChannelCountMax = 30; // TODO Why 30?
static constexpr SLuint32  SL_ANDROID_UNKNOWN_CHANNELMASK  = 0; // Matches name used internally.

SLuint32 AudioStreamOpenSLES::channelCountToChannelMaskDefault(int channelCount) const {
    if (channelCount > kAudioChannelCountMax) {
        return SL_ANDROID_UNKNOWN_CHANNELMASK;
    }

    SLuint32 bitfield = (1 << channelCount) - 1;

    // Indexed channels masks were added in N.
    // For before N, the best we can do is use a positional channel mask.
    return bitfield;
}

static bool s_isLittleEndian() {
    static uint32_t value = 1;
    return (*reinterpret_cast<uint8_t *>(&value) == 1);  // Does address point to LSB?
}

SLuint32 AudioStreamOpenSLES::getDefaultByteOrder() {
    return s_isLittleEndian() ? SL_BYTEORDER_LITTLEENDIAN : SL_BYTEORDER_BIGENDIAN;
}

Result AudioStreamOpenSLES::open() {

    LOGI("AudioStreamOpenSLES::open() chans=%d, rate=%d", mChannelCount, mSampleRate);
    // mChannelCount = 2
    // mSampleRate = 44100

    SLresult result = EngineOpenSLES::getInstance().open();
    if (SL_RESULT_SUCCESS != result) {
        return Result::ErrorInternal;
    }

    Result oboeResult = AudioStreamBuffered::open();
    if (oboeResult != Result::OK) {
        return oboeResult;
    }
    // Convert to defaults if UNSPECIFIED
    if (mSampleRate == kUnspecified) {
        mSampleRate = DefaultStreamValues::SampleRate;
    }
    if (mChannelCount == kUnspecified) {
        mChannelCount = DefaultStreamValues::ChannelCount;
    }

    mSharingMode = SharingMode::Shared;

    return Result::OK;
}

Result AudioStreamOpenSLES::configureBufferSizes(int32_t sampleRate) {
    LOGD("AudioStreamOpenSLES:%s(%d) initial mFramesPerBurst = %d, mFramesPerCallback = %d",
            __func__, sampleRate, mFramesPerBurst, mFramesPerCallback);
    // Decide frames per burst based on hints from caller.
    if (mFramesPerCallback != kUnspecified) {
        // Requested framesPerCallback must be honored.
        mFramesPerBurst = mFramesPerCallback;
    } else {
        mFramesPerBurst = DefaultStreamValues::FramesPerBurst;

        // Calculate the size of a fixed duration high latency buffer based on sample rate.
        int32_t framesPerHighLatencyBuffer =
                (kHighLatencyBufferSizeMillis * sampleRate) / kMillisPerSecond;

        mFramesPerCallback = mFramesPerBurst;
    }
    LOGD("AudioStreamOpenSLES:%s(%d) final mFramesPerBurst = %d, mFramesPerCallback = %d",
         __func__, sampleRate, mFramesPerBurst, mFramesPerCallback);

    mBytesPerCallback = mFramesPerCallback * getBytesPerFrame();
    if (mBytesPerCallback <= 0) {
        LOGE("AudioStreamOpenSLES::open() bytesPerCallback < 0 = %d, bad format?",
             mBytesPerCallback);
        return Result::ErrorInvalidFormat; // causing bytesPerFrame == 0
    }

    mCallbackBuffer = std::make_unique<uint8_t[]>(mBytesPerCallback);

    if (!usingFIFO()) {
        mBufferCapacityInFrames = mFramesPerBurst * kBufferQueueLength;
        // Check for overflow.
        if (mBufferCapacityInFrames <= 0) {
            mBufferCapacityInFrames = 0;
            LOGE("AudioStreamOpenSLES::open() numeric overflow because mFramesPerBurst = %d",
                 mFramesPerBurst);
            return Result::ErrorOutOfRange;
        }
        mBufferSizeInFrames = mBufferCapacityInFrames;
    }

    return Result::OK;
}

void AudioStreamOpenSLES::logUnsupportedAttributes() {
    // Log unsupported attributes
    // only report if changed from the default

    // Device ID
    if (mDeviceId != kUnspecified) {
        LOGW("Device ID [AudioStreamBuilder::setDeviceId()] "
             "is not supported on OpenSLES streams.");
    }
    // Sharing Mode
    if (mSharingMode != SharingMode::Shared) {
        LOGW("SharingMode [AudioStreamBuilder::setSharingMode()] "
             "is not supported on OpenSLES streams.");
    }
    // Performance Mode
    int sdkVersion = getSdkVersion();
    if (mPerformanceMode != PerformanceMode::None && sdkVersion < __ANDROID_API_N_MR1__) {
        LOGW("PerformanceMode [AudioStreamBuilder::setPerformanceMode()] "
             "is not supported on OpenSLES streams running on pre-Android N-MR1 versions.");
    }
    // Content Type
    if (mContentType != ContentType::Music) {
        LOGW("ContentType [AudioStreamBuilder::setContentType()] "
             "is not supported on OpenSLES streams.");
    }

    // Session Id
    if (mSessionId != SessionId::None) {
        LOGW("SessionId [AudioStreamBuilder::setSessionId()] "
             "is not supported on OpenSLES streams.");
    }
}

SLresult AudioStreamOpenSLES::updateStreamParameters(SLAndroidConfigurationItf configItf) {
    SLresult result = SL_RESULT_SUCCESS;
    mPerformanceMode = PerformanceMode::None; // If we can't query it then assume None.
    return result;
}

// This is called under mLock.
Result AudioStreamOpenSLES::close_l() {
    if (mState == StreamState::Closed) {
        return Result::ErrorClosed;
    }

    AudioStreamBuffered::close();

    onBeforeDestroy();

    if (mObjectInterface != nullptr) {
        (*mObjectInterface)->Destroy(mObjectInterface);
        mObjectInterface = nullptr;
    }

    onAfterDestroy();

    mSimpleBufferQueueInterface = nullptr;
    EngineOpenSLES::getInstance().close();

    setState(StreamState::Closed);
    return Result::OK;
}

SLresult AudioStreamOpenSLES::enqueueCallbackBuffer(SLAndroidSimpleBufferQueueItf bq) {
    return (*bq)->Enqueue(bq, mCallbackBuffer.get(), mBytesPerCallback);
}

int32_t AudioStreamOpenSLES::getBufferDepth(SLAndroidSimpleBufferQueueItf bq) {
    SLAndroidSimpleBufferQueueState queueState;
    SLresult result = (*bq)->GetState(bq, &queueState);
    return (result == SL_RESULT_SUCCESS) ? queueState.count : -1;
}

void AudioStreamOpenSLES::processBufferCallback(SLAndroidSimpleBufferQueueItf bq) {
    bool stopStream = false;
    // Ask the app callback to process the buffer.
    DataCallbackResult result = fireDataCallback(mCallbackBuffer.get(), mFramesPerCallback);
    if (result == DataCallbackResult::Continue) {
        // Pass the buffer to OpenSLES.
        SLresult enqueueResult = enqueueCallbackBuffer(bq);
        if (enqueueResult != SL_RESULT_SUCCESS) {
            LOGE("%s() returned %d", __func__, enqueueResult);
            stopStream = true;
        }
        // Update Oboe client position with frames handled by the callback.
        if (getDirection() == Direction::Input) {
            mFramesRead += mFramesPerCallback;
        } else {
            mFramesWritten += mFramesPerCallback;
        }
    } else if (result == DataCallbackResult::Stop) {
        LOGD("Oboe callback returned Stop");
        stopStream = true;
    } else {
        LOGW("Oboe callback returned unexpected value = %d", result);
        stopStream = true;
    }
    if (stopStream) {
        requestStop();
    }
}

// This callback handler is called every time a buffer has been processed by OpenSL ES.
static void bqCallbackGlue(SLAndroidSimpleBufferQueueItf bq, void *context) {
    (reinterpret_cast<AudioStreamOpenSLES *>(context))->processBufferCallback(bq);
}

SLresult AudioStreamOpenSLES::registerBufferQueueCallback() {
    // The BufferQueue
    SLresult result = (*mObjectInterface)->GetInterface(mObjectInterface, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                &mSimpleBufferQueueInterface);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("get buffer queue interface:%p result:%s",
             mSimpleBufferQueueInterface,
             getSLErrStr(result));
    } else {
        // Register the BufferQueue callback
        result = (*mSimpleBufferQueueInterface)->RegisterCallback(mSimpleBufferQueueInterface,
                                                                  bqCallbackGlue, this);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("RegisterCallback result:%s", getSLErrStr(result));
        }
    }
    return result;
}

int64_t AudioStreamOpenSLES::getFramesProcessedByServer() {
    updateServiceFrameCounter();
    int64_t millis64 = mPositionMillis.get();
    int64_t framesProcessed = millis64 * getSampleRate() / kMillisPerSecond;
    return framesProcessed;
}

Result AudioStreamOpenSLES::waitForStateChange(StreamState currentState,
                                                     StreamState *nextState,
                                                     int64_t timeoutNanoseconds) {
    Result oboeResult = Result::ErrorTimeout;
    int64_t sleepTimeNanos = 20 * kNanosPerMillisecond; // arbitrary
    int64_t timeLeftNanos = timeoutNanoseconds;

    while (true) {
        const StreamState state = getState(); // this does not require a lock
        if (nextState != nullptr) {
            *nextState = state;
        }
        if (currentState != state) { // state changed?
            oboeResult = Result::OK;
            break;
        }

        // Did we timeout or did user ask for non-blocking?
        if (timeLeftNanos <= 0) {
            break;
        }

        if (sleepTimeNanos > timeLeftNanos){
            sleepTimeNanos = timeLeftNanos;
        }
        AudioClock::sleepForNanos(sleepTimeNanos);
        timeLeftNanos -= sleepTimeNanos;
    }

    return oboeResult;
}
