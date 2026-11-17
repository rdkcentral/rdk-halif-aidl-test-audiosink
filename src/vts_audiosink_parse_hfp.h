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
 * @defgroup AUDIO_SINK_HALTEST_PARSE_HFP_HEADER Audio Sink HAL Tests parse Hardware feature profile Header File
 * @{
 * @parblock
 *
 * ### Parse HFP functions for Audio Sink :
 *
 * Parse HFP functions required for the module across all vendors.
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_audiosink_parse_hfp.h
 *
 */

#ifndef __VTS_AUDIOSINK_PARSE_HFP_H__
#define __VTS_AUDIOSINK_PARSE_HFP_H__

#include <stdio.h>
#include <vector>
#include <string>

#include <ut.h>
#include <ut_kvp_profile.h>
#include <ut_kvp.h>
#include <ut_log.h>

using namespace std;
using namespace com::rdk::hal;

struct platformCapabilitiesHFP
{
    bool supportsLowLatency;
    vector<int32_t> supportedSampleRatesHz;
    vector<audiodecoder::PCMFormat> supportedPCMFormats;
    bool supportsPlanarFormat;
    bool supportsSoCProprietary;
    int32_t maxNumChannels;
    bool supportsDolbyAtmos;
};

struct sinkCapabilitiesHFP
{
    int32_t id;
    bool supportsSecure;
    vector<audiosink::ContentType> supportedContentTypes;
};

struct audioSinkHFP
{
    std::vector<sinkCapabilitiesHFP> sinkCaps;
    std::vector<int32_t> availableIds;
    platformCapabilitiesHFP platformCaps;
    vector<int32_t> availableDecoderIds;
    vector<int32_t> availableClockIds;
};

extern audioSinkHFP gAudiosinksHFP;

#define AUDIOSINKMGR "audiosink"
#define AUDIOSINKMGR_IAUDIOSINK      "audiosink.IAudioSink"
#define AUDIOSINKMGR_PLATFORMCAPS    "audiosink.platformCapabilities"

UT_status_t vts_audiosink_parseAudioSinkHFP();
void vts_audiosink_get_available_id(std::vector<int32_t> &availableIds);

#endif //__VTS_AUDIOSINK_PARSE_HFP_H__

/** @} */ // End of AUDIO_SINK_HALTEST_PARSE_HFP_HEADER
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK
/** @} */ // End of HPK

