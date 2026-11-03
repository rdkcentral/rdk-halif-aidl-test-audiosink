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
 * @defgroup AUDIO_SINK_HALTEST_HELPER Audio Sink HAL Tests helper functions
 * @{
 * @parblock
 *
 * ### Helper functions for VTS tests :
 *
 * Helper functions required for the module across all vendors.
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_audiodecoder_helper_functions.cpp
 *
 */

#include <algorithm>
#include "vts_audiosink_helper_functions.h"
#include "vts_audiosink_parse_hfp.h"

using namespace com::rdk::hal;

/* ContentType mapping table for UT_Control_GetMapString */
const ut_control_keyStringMapping_t contentType_mapTable[] =
{
    {"UNKNOWN",       (int32_t) ContentType::UNKNOWN},
    {"SPEECH",        (int32_t) ContentType::SPEECH},
    {"MUSIC",         (int32_t) ContentType::MUSIC},
    {"MOVIE",         (int32_t) ContentType::MOVIE},
    {"SONIFICATION",  (int32_t) ContentType::SONIFICATION},
    {NULL, -1}
};

/* State mapping table for UT_Control_GetMapString */
const ut_control_keyStringMapping_t state_mapTable[] =
{
    {"UNKNOWN",   (int32_t) ::com::rdk::hal::audiosink::State::UNKNOWN},
    {"CLOSED",    (int32_t) ::com::rdk::hal::audiosink::State::CLOSED},
    {"OPENING",   (int32_t) ::com::rdk::hal::audiosink::State::OPENING},
    {"READY",     (int32_t) ::com::rdk::hal::audiosink::State::READY},
    {"STARTING",  (int32_t) ::com::rdk::hal::audiosink::State::STARTING},
    {"STARTED",   (int32_t) ::com::rdk::hal::audiosink::State::STARTED},
    {"FLUSHING",  (int32_t) ::com::rdk::hal::audiosink::State::FLUSHING},
    {"STOPPING",  (int32_t) ::com::rdk::hal::audiosink::State::STOPPING},
    {"CLOSING",   (int32_t) ::com::rdk::hal::audiosink::State::CLOSING},
    {NULL, -1}
};

/* AudioSink Property mapping table for UT_Control_GetMapString */
const ut_control_keyStringMapping_t audiosinkProperty_mapTable[] =
{
    {"RESOURCE_ID",       (int32_t) audiosink::Property::RESOURCE_ID},
    {"REFERENCE_LEVEL",   (int32_t) audiosink::Property::REFERENCE_LEVEL},
    {"RENDER_LATENCY_NS", (int32_t) audiosink::Property::RENDER_LATENCY_NS},
    {"QUEUE_DEPTH_NS",    (int32_t) audiosink::Property::QUEUE_DEPTH_NS},
    {"MIXER_ID",          (int32_t) audiosink::Property::MIXER_ID},
    {"AV_SOURCE",         (int32_t) audiosink::Property::AV_SOURCE},
    {"METRIC_xxxx",       (int32_t) audiosink::Property::METRIC_xxxx},
    {NULL, -1}
};

/* VolumeRamp mapping table for UT_Control_GetMapString */
const ut_control_keyStringMapping_t volumeRamp_mapTable[] =
{
    {"LINEAR",    (int32_t) audiosink::VolumeRamp::LINEAR},
    {"IN_CUBIC",  (int32_t) audiosink::VolumeRamp::IN_CUBIC},
    {"OUT_CUBIC", (int32_t) audiosink::VolumeRamp::OUT_CUBIC},
    {NULL, -1}
};



audiodecoder::ChannelType vts_audiosink_helper_StringToChannelType(const std::string& channelType)
{
    static const std::unordered_map<std::string, audiodecoder::ChannelType> channelTypeMap = {
        {"MONO", audiodecoder::ChannelType::MONO},
        {"FRONT_LEFT", audiodecoder::ChannelType::FRONT_LEFT},
        {"FRONT_RIGHT", audiodecoder::ChannelType::FRONT_RIGHT},
        {"FRONT_CENTER", audiodecoder::ChannelType::FRONT_CENTER},
        {"LFE", audiodecoder::ChannelType::LFE},
        {"SIDE_LEFT", audiodecoder::ChannelType::SIDE_LEFT},
        {"SIDE_RIGHT", audiodecoder::ChannelType::SIDE_RIGHT},
        {"UP_LEFT", audiodecoder::ChannelType::UP_LEFT},
        {"UP_RIGHT", audiodecoder::ChannelType::UP_RIGHT},
        {"BACK_LEFT", audiodecoder::ChannelType::BACK_LEFT},
        {"BACK_RIGHT", audiodecoder::ChannelType::BACK_RIGHT},
        {"BACK_CENTER", audiodecoder::ChannelType::BACK_CENTER}
    };

    auto it = channelTypeMap.find(channelType);
    if (it != channelTypeMap.end())
    {
        return it->second;
    }
    return static_cast<audiodecoder::ChannelType>(-1);
}

bool stringToBool(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    if (lowerStr == "true" || lowerStr == "1" || lowerStr == "yes") {
        return true;
    } else if (lowerStr == "false" || lowerStr == "0" || lowerStr == "no") {
        return false;
    } else {
        // Handle invalid input (e.g., throw an exception, return a default)
        throw std::invalid_argument("Invalid string for boolean conversion: " + str);
    }
}

void vts_audiosink_helper_validatePlatformCapabilities(const audiosink::PlatformCapabilities& capabilities)
{
    UT_LOG_INFO("Inside of the validater helper fucntion");

    const auto& platformCapsHFP =  gAudiosinksHFP.platformCaps;

    UT_LOG_INFO("Validing platform capabilites in helper fucntion");

    /* checking lowlatency support from getPlatformCapabilities() and hfp platformcapabilites */
    UT_ASSERT_EQUAL(platformCapsHFP.supportsLowLatency, capabilities.supportsLowLatency);

    /* checking supported sample rates from getPlatformCapabilities() and hfp platformcapabilites (order independent) */
    {
        std::vector<int32_t> expected = platformCapsHFP.supportedSampleRatesHz;
        std::vector<int32_t> actual   = capabilities.supportedSampleRatesHz;
        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(), actual.end());
        UT_ASSERT_TRUE(expected == actual);
    }

    /* checking supported pcm formats from getPlatformCapabilities() and hfp platformcapabilites (order independent) */
    {
        std::vector<audiodecoder::PCMFormat> expected = platformCapsHFP.supportedPCMFormats;
        std::vector<audiodecoder::PCMFormat> actual   = capabilities.supportedPCMFormats;
        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(), actual.end());
        UT_ASSERT_TRUE(expected == actual);
    }

    /* checking supports PlanarFormat from getPlatformCapabilities() and hfp platformcapabilites */
    UT_ASSERT_EQUAL(platformCapsHFP.supportsPlanarFormat, capabilities.supportsPlanarFormat);

    /* supportsSoCProprietary */
    UT_ASSERT_EQUAL(platformCapsHFP.supportsSoCProprietary, capabilities.supportsSoCProprietary);

    /* maxNumChannels */
    UT_ASSERT_EQUAL(platformCapsHFP.maxNumChannels, capabilities.maxNumChannels);

    /* supportsDolbyAtmos */
    UT_ASSERT_EQUAL(platformCapsHFP.supportsDolbyAtmos, capabilities.supportsDolbyAtmos);
}

void vts_audiosink_helper_validateSinkCapabilities(int32_t sinkId, const audiosink::Capabilities& capabilities)
{
    const auto& sinkCapsHFP = gAudiosinksHFP.sinkCaps;

    auto it = std::find_if(sinkCapsHFP.begin(), sinkCapsHFP.end(),
                           [sinkId](const sinkCapabilitiesHFP& hfpCaps) {
                               return hfpCaps.id == sinkId;
                           });

    UT_ASSERT_NOT_EQUAL(it, sinkCapsHFP.end());

    /* checking supportsSecure support from getSinkCapabilities() and hfp sinkcapabilites */
    UT_ASSERT_EQUAL(it->supportsSecure, capabilities.supportsSecure);

    /* checking supportedContentTypes from getSinkCapabilities() and hfp sinkcapabilites */
    std::unordered_set<int32_t> supportedContentTypesSet;

    UT_ASSERT_FALSE(capabilities.supportedContentTypes.empty());
    UT_ASSERT_EQUAL(capabilities.supportedContentTypes.size(), it->supportedContentTypes.size());

    if(capabilities.supportedContentTypes.size() != it->supportedContentTypes.size())
    {
        return;
    }

    for (const auto& contentType : capabilities.supportedContentTypes)
    {
        UT_LOG_INFO("contentType = %d", static_cast<int32_t>(contentType));
        supportedContentTypesSet.insert(static_cast<int32_t>(contentType));
    }

    for (const auto& contentType : it->supportedContentTypes)
    {
        UT_LOG_INFO("supported type = %d", static_cast<int32_t>(contentType));
        UT_ASSERT_NOT_EQUAL(supportedContentTypesSet.find(static_cast<int32_t>(contentType)), supportedContentTypesSet.end());
    }
}

bool vts_audiosink_helper_areElementsUnique(const std::vector<int>& values)
{
    std::unordered_set<int> uniqueElements;

    for (const auto& value : values)
    {
        if(value < 0)
        {
            UT_LOG_ERROR("find Invalid decoder ID : %d", value);
            return false;
        }
        if (uniqueElements.find(value) != uniqueElements.end())
        {
            UT_LOG_ERROR("decoder ID is not unique : %d", value);
            return false;
        }
        uniqueElements.insert(value);
    }
    return true;
}

bool vts_audiosink_helper_validatePropertyValue(int sink_id, const std::optional<PropertyValue>& propertyValue, const audiosink::Property property)
{
    if (!propertyValue.has_value() || !propertyValue->value.has_value())
    {
        return false;
    }

    std::variant<int32_t, std::string> val;
    if (propertyValue->value.has_value())
    {
        const auto& propVal = propertyValue->value.value();
        switch (propVal.getTag())
        {
            case PropertyValue::Value::Tag::intValue:
                val = propVal.get<PropertyValue::Value::Tag::intValue>();
                break;
            case PropertyValue::Value::Tag::stringValue:
            {
                val = propVal.get<PropertyValue::Value::Tag::stringValue>();  // std::string (@utf8InCpp)
            }
                break;
            case PropertyValue::Value::Tag::booleanValue:
                val = propVal.get<PropertyValue::Value::Tag::booleanValue>() ? 1 : 0;  // Store as int32_t (1 for true, 0 for false)
                break;

            default:
                std::cerr << "Unsupported type in PropertyValue::Value" << std::endl;
                break;
        }
    }

    switch (property)
    {
        case audiosink::Property::RESOURCE_ID:
             if(std::holds_alternative<int>(val) && std::get<int>(val) == sink_id)
             {
                return true;
             }
             else
             {
                return false;
             }

        case audiosink::Property::REFERENCE_LEVEL:
            return std::holds_alternative<int32_t>(val) &&
                   (std::get<int32_t>(val) == -31 || std::get<int32_t>(val) >= 1);

        case audiosink::Property::AV_SOURCE:
            return std::holds_alternative<int32_t>(val) &&
                   (std::get<int32_t>(val) == static_cast<int32_t>(AVSource::AUTO) ||
                    std::get<int32_t>(val) == static_cast<int32_t>(AVSource::UNKNOWN) ||
                    (std::get<int32_t>(val) >= static_cast<int32_t>(AVSource::IP) &&
                     std::get<int32_t>(val) <= static_cast<int32_t>(AVSource::SYSTEM)) ||
                    (std::get<int32_t>(val) >= static_cast<int32_t>(AVSource::HDMI_1) &&
                     std::get<int32_t>(val) <= static_cast<int32_t>(AVSource::HDMI_5)) ||
                    (std::get<int32_t>(val) >= static_cast<int32_t>(AVSource::COMPOSITE_1) &&
                     std::get<int32_t>(val) <= static_cast<int32_t>(AVSource::COMPOSITE_5)));

        case audiosink::Property::RENDER_LATENCY_NS:
            return std::holds_alternative<int32_t>(val) && std::get<int32_t>(val) >= 0;

        case audiosink::Property::QUEUE_DEPTH_NS:
            return std::holds_alternative<int32_t>(val) && std::get<int32_t>(val) >= 0;

        case audiosink::Property::MIXER_ID:
            return std::holds_alternative<std::int32_t>(val) &&
                   (std::get<std::int32_t>(val) == 0 || std::get<std::int32_t>(val) >= 1);

        default:
            return true;
    }
}

/** @} */ // End of AUDIO_SINK_HALTEST_HELPER
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK
/** @} */ // End of HPK
