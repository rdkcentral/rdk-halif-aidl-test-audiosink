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
 * @defgroup AUDIO_SINK_HALTEST_PARSE_HFP Audio Sink HAL Tests parse Hardware feature profile
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
 * @file vts_audiosink_parse_hfp.cpp
 *
 */

#include "vts_audiosink_helper_functions.h"
#include "vts_audiosink_parse_hfp.h"
#include "vts_audiodecoder_parse_hfp.h"
#include "vts_avclock_parse_hfp.h"

using namespace std;

#define AUDIODECODERMGR_AVAILABLEDECODERS "audiodecoder.availableDecoders"
#define AVCLOCK_AVAILABLECLOCKS "avclock.availableClocks"

audioSinkHFP gAudiosinksHFP;

void vts_audiosink_get_available_id(std::vector<int32_t> &availableIds)
{
    string audiosinkpath = AUDIOSINKMGR_IAUDIOSINK;
    int32_t number_of_audiosinks = UT_KVP_PROFILE_GET_LIST_COUNT(audiosinkpath.c_str());
    availableIds.clear();
    for (int32_t i = 0; i < number_of_audiosinks; ++i)
    {
        string audiosinkidpath = audiosinkpath + "." + std::to_string(i) + ".id";
        int32_t audiosink_id = UT_KVP_PROFILE_GET_UINT32(audiosinkidpath.c_str());
        availableIds.push_back(audiosink_id);
    }
}

UT_status_t vts_audiosink_parseAudioSinkHFP()
{
    string audiosinkpath = AUDIOSINKMGR_IAUDIOSINK;
    string platformCapabilitesPath = AUDIOSINKMGR_PLATFORMCAPS;
    std::string availableDecodersPath = AUDIODECODERMGR_AVAILABLEDECODERS;
    std::string availableClocksPath = AVCLOCK_AVAILABLECLOCKS;

    int32_t number_of_audiosinks;

    char str_val[UT_KVP_MAX_ELEMENT_SIZE];

    number_of_audiosinks = UT_KVP_PROFILE_GET_LIST_COUNT(audiosinkpath.c_str());
    if(number_of_audiosinks <= 0)
    {
        UT_LOG_ERROR("No audiosinks found in the HFP");
        return UT_STATUS_FAILURE;
    }

    for(int32_t i = 0; i < number_of_audiosinks; ++i)
    {
        sinkCapabilitiesHFP sinkCaps;
        string audiosinkidpath = audiosinkpath + "." + std::to_string(i) + ".id";
        string supportedContentTypesPath = audiosinkpath + "." + std::to_string(i) + ".supportedContentTypes";
        string supportsSecurePath = audiosinkpath + "." + std::to_string(i) + ".supportsSecure";

        sinkCaps.id = UT_KVP_PROFILE_GET_UINT32(audiosinkidpath.c_str());

        int32_t contentTypeCount = UT_KVP_PROFILE_GET_LIST_COUNT(supportedContentTypesPath.c_str());
        for(int32_t j = 0; j < contentTypeCount; ++j)
        {
            string contentTypePath = supportedContentTypesPath + "." + std::to_string(j);
            UT_KVP_PROFILE_GET_STRING(contentTypePath.c_str(), str_val);
            int32_t contentTypeValue = UT_Control_GetMapValue(contentType_mapTable, str_val, (int32_t) ContentType::UNKNOWN);
            sinkCaps.supportedContentTypes.push_back(static_cast<audiosink::ContentType>(contentTypeValue));
        }

        sinkCaps.supportsSecure = UT_KVP_PROFILE_GET_BOOL(supportsSecurePath.c_str());

        gAudiosinksHFP.availableIds.push_back(sinkCaps.id);

        gAudiosinksHFP.sinkCaps.push_back(std::move(sinkCaps));

    }

    vts_audiodecoder_get_available_id(gAudiosinksHFP.availableDecoderIds);

    vts_avclock_get_available_id(gAudiosinksHFP.availableClockIds);

    // Read platform capabilities attributes (existing)
    string lowlatency_path            = platformCapabilitesPath + ".supportsLowLatency";
    string samplerates_path           = platformCapabilitesPath + ".supportedSampleRatesHz";
    string pcmformats_path            = platformCapabilitesPath + ".supportedPCMFormats";
    string supportsPlanarFormat_path  = platformCapabilitesPath + ".supportsPlanarFormat";
    string supportsSoCProprietary_path = platformCapabilitesPath + ".supportsSoCProprietary";
    string maxNumChannels_path         = platformCapabilitesPath + ".maxNumChannels";
    string supportsDolbyAtmos_path     = platformCapabilitesPath + ".supportsDolbyAtmos";

    gAudiosinksHFP.platformCaps.supportsLowLatency = UT_KVP_PROFILE_GET_BOOL(lowlatency_path.c_str());
    gAudiosinksHFP.platformCaps.supportedSampleRatesHz.clear();
    int32_t sampleRateCount = UT_KVP_PROFILE_GET_LIST_COUNT(samplerates_path.c_str());
    for (int32_t j = 0; j < sampleRateCount; ++j)
    {
        string sampleRatePath = samplerates_path + "." + std::to_string(j);
        gAudiosinksHFP.platformCaps.supportedSampleRatesHz.push_back(UT_KVP_PROFILE_GET_UINT32(sampleRatePath.c_str()));
    }

    gAudiosinksHFP.platformCaps.supportedPCMFormats.clear();
    int32_t pcmFormatCount = UT_KVP_PROFILE_GET_LIST_COUNT(pcmformats_path.c_str());
    for (int32_t j = 0; j < pcmFormatCount; ++j)
    {
        string pcmFormatPath = pcmformats_path + "." + std::to_string(j);
        UT_KVP_PROFILE_GET_STRING(pcmFormatPath.c_str(), str_val);
        gAudiosinksHFP.platformCaps.supportedPCMFormats.push_back(vts_audioDecoder_helper_StringToPCMFormat(std::string(str_val)));
    }
    gAudiosinksHFP.platformCaps.supportsPlanarFormat = UT_KVP_PROFILE_GET_BOOL(supportsPlanarFormat_path.c_str());
    gAudiosinksHFP.platformCaps.maxNumChannels = UT_KVP_PROFILE_GET_UINT32(maxNumChannels_path.c_str());
    gAudiosinksHFP.platformCaps.supportsDolbyAtmos = UT_KVP_PROFILE_GET_BOOL(supportsDolbyAtmos_path.c_str());
    gAudiosinksHFP.platformCaps.supportsSoCProprietary = UT_KVP_PROFILE_GET_BOOL(supportsSoCProprietary_path.c_str());

    return UT_STATUS_OK;
}
/** @} */ // End of AUDIO_SINK_HALTEST_PARSE_HFP
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK
/** @} */ // End of HPK
