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
#include "vts_audiosink.h"

using namespace com::rdk::hal;

/*currently HAL FEATURE PROFILE contain string value for codec instead of enum. so the helper required to convert*/
audiodecoder::Codec vts_audiosink_helper_StringTocodec(const std::string& format)
{
    static const std::unordered_map<std::string, audiodecoder::Codec> formatMap = {
    {"PCM", audiodecoder::Codec::PCM},
    {"AAC_LC", audiodecoder::Codec::AAC_LC},
    {"HE_AAC", audiodecoder::Codec::HE_AAC},
    {"HE_AAC2", audiodecoder::Codec::HE_AAC2},
    {"AAC_ELD", audiodecoder::Codec::AAC_ELD},
    {"DOLBY_AC3", audiodecoder::Codec::DOLBY_AC3},
    {"DOLBY_AC3_PLUS", audiodecoder::Codec::DOLBY_AC3_PLUS},
    {"DOLBY_AC3_PLUS_JOC", audiodecoder::Codec::DOLBY_AC3_PLUS_JOC},
    {"DOLBY_AC4", audiodecoder::Codec::DOLBY_AC4},
    {"DOLBY_MAT", audiodecoder::Codec::DOLBY_MAT},
    {"DOLBY_MAT2", audiodecoder::Codec::DOLBY_MAT2},
    {"DOLBY_TRUEHD", audiodecoder::Codec::DOLBY_TRUEHD},
    {"MPEG2", audiodecoder::Codec::MPEG2},
    {"MP3", audiodecoder::Codec::MP3},
    {"FLAC", audiodecoder::Codec::FLAC},
    {"VORBIS", audiodecoder::Codec::VORBIS},
    {"DTS", audiodecoder::Codec::DTS},
    {"OPUS", audiodecoder::Codec::OPUS},
    {"WMA", audiodecoder::Codec::WMA},
    {"REALAUDIO", audiodecoder::Codec::REALAUDIO},
    {"USAC", audiodecoder::Codec::USAC},
    {"X_HE_AAC", audiodecoder::Codec::X_HE_AAC}
    };

    auto it = formatMap.find(format);
    if (it != formatMap.end())
    {
        return it->second;
    }
    return static_cast<audiodecoder::Codec>(-1);
}

audiodecoder::PCMFormat vts_audiosink_helper_StringToFormat(const std::string& format)
{
    static const std::unordered_map<std::string, audiodecoder::PCMFormat> formatMap = {
        {"F64LE", audiodecoder::PCMFormat::F64LE},
        {"F64BE", audiodecoder::PCMFormat::F64BE},
        {"F32LE", audiodecoder::PCMFormat::F32LE},
        {"F32BE", audiodecoder::PCMFormat::F32BE},
        {"S32LE", audiodecoder::PCMFormat::S32LE},
        {"S32BE", audiodecoder::PCMFormat::S32BE},
        {"U32LE", audiodecoder::PCMFormat::U32LE},
        {"U32BE", audiodecoder::PCMFormat::U32BE},
        {"S24_32LE", audiodecoder::PCMFormat::S24_32LE},
        {"S24_32BE", audiodecoder::PCMFormat::S24_32BE},
        {"U24_32LE", audiodecoder::PCMFormat::U24_32LE},
        {"U24_32BE", audiodecoder::PCMFormat::U24_32BE},
        {"S24LE", audiodecoder::PCMFormat::S24LE},
        {"S24BE", audiodecoder::PCMFormat::S24BE},
        {"U24LE", audiodecoder::PCMFormat::U24LE},
        {"U24BE", audiodecoder::PCMFormat::U24BE},
        {"S20LE", audiodecoder::PCMFormat::S20LE},
        {"S20BE", audiodecoder::PCMFormat::S20BE},
        {"U20LE", audiodecoder::PCMFormat::U20LE},
        {"U20BE", audiodecoder::PCMFormat::U20BE},
        {"S18LE", audiodecoder::PCMFormat::S18LE},
        {"S18BE", audiodecoder::PCMFormat::S18BE},
        {"U18LE", audiodecoder::PCMFormat::U18LE},
        {"U18BE", audiodecoder::PCMFormat::U18BE},
        {"S16LE", audiodecoder::PCMFormat::S16LE},
        {"S16BE", audiodecoder::PCMFormat::S16BE},
        {"U16LE", audiodecoder::PCMFormat::U16LE},
        {"U16BE", audiodecoder::PCMFormat::U16BE},
        {"S8", audiodecoder::PCMFormat::S8},
        {"U8", audiodecoder::PCMFormat::U8},
    };

    auto it = formatMap.find(format);
    if (it != formatMap.end())
    {
        return it->second;
    }
    return static_cast<audiodecoder::PCMFormat>(-1);
}

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

void vts_audiosink_helper_validateCapabilities(const PlatformCapabilities& capabilities)
{
    UT_LOG_INFO("Inside of the validater helper fucntion");

    const auto& _sinkmanager =  gAudiosinks_hfp;

    UT_LOG_INFO("Validing platform capabilites in helper fucntion");

    /* checking lowlatency support from getPlatformCapabilities() and hfp platformcapabilites */
    UT_ASSERT_EQUAL(stringToBool(_sinkmanager[0].supportedcaps[0].lowlatency), capabilities.supportsLowLatency);

    /* checking samplerate support from getPlatformCapabilities() and hfp platformcapabilites */
    UT_ASSERT_EQUAL(std::stoi(_sinkmanager[0].supportedcaps[0].samplerate), capabilities.systemMixerSampleRateHz);

    /* checking mixer pcmformat from getPlatformCapabilities() and hfp platformcapabilites */
    UT_ASSERT_EQUAL(vts_audiosink_helper_StringToFormat(_sinkmanager[0].supportedcaps[0].mixer_pcmformat), capabilities.systemMixerPCMFormat);

    /* checking supports PlanarFormat from getPlatformCapabilities() and hfp platformcapabilites */
    UT_ASSERT_EQUAL(stringToBool(_sinkmanager[0].supportedcaps[0].supportsPlanarFormat), capabilities.supportsPlanarFormat);
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
                android::String16 string16Val = propVal.get<PropertyValue::Value::Tag::stringValue>();
                android::String8 string8Val(string16Val);  // Convert String16 to String8
                val = std::string(string8Val.string());    // Convert String8 to std::string
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

        case audiosink::Property::DOLBY_ATMOS_LOCK:
            return std::holds_alternative<int32_t>(val) &&
                   (std::get<int32_t>(val) == 0 || std::get<int32_t>(val) == 1);

        default:
            return true;
    }
}


/** @} */ // End of AUDIO_SINK_HALTEST_HELPER
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK
/** @} */ // End of HPK
