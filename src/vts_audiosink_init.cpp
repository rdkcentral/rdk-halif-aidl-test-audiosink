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
 * @addtogroup AUDIO_SINK_HALTEST Audio SINK HAL Tests
 * @{
 */

/**
 * @defgroup AUDIO_SINK_HALTEST_INIT Audio Sink HAL Tests helper functions
 * @{
 * @parblock
 *
 * ### Initialization functions for VTS tests :
 *
 * Initialization functions required for the module across all vendors.
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_audiosink_init.cpp
 *
 */

#include <ut.h>
#include <ut_log.h>
#include "vts_audiosink_helper_functions.h"
#include "vts_audiosink_parse_hfp.h"
#include "vts_audiosink.h"

UT_status_t vts_audiosink_getAudioSinkService();
sp<IAudioSinkManager> gAudioSinkManager;

void *gAudioSinkTestConfiguration;

UT_status_t vts_getAudioDecoderService()
{
    gAudioDecoderManager = android::interface_cast<audiodecoder::IAudioDecoderManager>(
            defaultServiceManager()->getService(String16(audiodecoder::IAudioDecoderManager::serviceName().c_str())));
    if (gAudioDecoderManager == nullptr)
    {
        UT_LOG_ERROR("AudioDecoder Service not available");
        return UT_STATUS_FAILURE;
    }
    return UT_STATUS_OK;
}

UT_status_t VTS_AudioSink_init()
{
    if(vts_audiosink_getAudioSinkService() != UT_STATUS_OK)
    {
        return UT_STATUS_FAILURE;
    }
    if(vts_getAudioDecoderService() != UT_STATUS_OK)
    {
        return UT_STATUS_FAILURE;
    }
    if(vts_audiosink_parseAudioSinkHFP() != UT_STATUS_OK)
    {
        UT_LOG_ERROR("Failed to process the HFP file");
        return UT_STATUS_FAILURE;
    }
    //gAudioSinkTestConfiguration = vts_kvp_create_instance(VTS_TEST_CONFIG_PATH);

    return UT_STATUS_OK;
}

void VTS_AudioSink_destroy()
{
    vts_kvp_destroy_instance(gAudioSinkTestConfiguration);
}

/** @} */ // End of AUDIO_SINK_HALTEST_INIT
/** @} */ // End of AUDIO_SINK_HALTEST
/** @} */ // End of AUDIO_SINK
/** @} */ // End of HPK
