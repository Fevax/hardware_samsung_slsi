/*
 *
 * Copyright 2012 Samsung Electronics S.LSI Co. LTD
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

/*
 * @file    Exynos_OMX_Def.h
 * @brief   Exynos_OMX specific define
 * @author  SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version    2.0.0
 * @history
 *   2012.02.20 : Create
 */

#ifndef EXYNOS_OMX_DEF
#define EXYNOS_OMX_DEF

#include "OMX_Types.h"
#include "OMX_IVCommon.h"

#define VERSIONMAJOR_NUMBER                1
#define VERSIONMINOR_NUMBER                0
#define REVISION_NUMBER                    0
#define STEP_NUMBER                        0


#define MAX_OMX_COMPONENT_NUM              20
#define MAX_OMX_COMPONENT_ROLE_NUM         10
#define MAX_OMX_COMPONENT_NAME_SIZE        OMX_MAX_STRINGNAME_SIZE
#define MAX_OMX_COMPONENT_ROLE_SIZE        OMX_MAX_STRINGNAME_SIZE
#define MAX_OMX_COMPONENT_LIBNAME_SIZE     OMX_MAX_STRINGNAME_SIZE * 2
#define MAX_OMX_MIMETYPE_SIZE              OMX_MAX_STRINGNAME_SIZE

#define MAX_TIMESTAMP        40
#define MAX_FLAGS            40

#define MAX_BUFFER_PLANE     3

#define INDEX_AFTER_EOS      0xE05

#define EXYNOS_OMX_INSTALL_PATH "/system/lib/omx/"

typedef enum _EXYNOS_CODEC_TYPE
{
    SW_CODEC,
    HW_VIDEO_DEC_CODEC,
    HW_VIDEO_ENC_CODEC,
    HW_AUDIO_DEC_CODEC,
    HW_AUDIO_ENC_CODEC
} EXYNOS_CODEC_TYPE;

typedef struct _EXYNOS_OMX_PRIORITYMGMTTYPE
{
    OMX_U32 nGroupPriority; /* the value 0 represents the highest priority */
                            /* for a group of components                   */
    OMX_U32 nGroupID;
} EXYNOS_OMX_PRIORITYMGMTTYPE;

typedef struct _EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bNeedContigMem;
} EXYNOS_OMX_VIDEO_PARAM_PORTMEMTYPE;

typedef struct _EXYNOS_OMX_VIDEO_PARAM_DTSMODE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL        bDTSMode;
} EXYNOS_OMX_VIDEO_PARAM_DTSMODE;

typedef struct _EXYNOS_OMX_VIDEO_CONFIG_BUFFERINFO {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_PTR OMX_IN  pVirAddr;
    OMX_S32 OMX_OUT fd;
} EXYNOS_OMX_VIDEO_CONFIG_BUFFERINFO;

typedef struct _EXYNOS_OMX_VIDEO_CONFIG_QOSINFO {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nQosRatio;
} EXYNOS_OMX_VIDEO_CONFIG_QOSINFO;

#ifdef USE_VP8ENC_SUPPORT
typedef enum OMX_VIDEO_VP84PROFILETYPE {
    OMX_VIDEO_VP8ProfileMain                = 0x01,
    OMX_VIDEO_VP8ProfileUnknown             = 0x6EFFFFFF,
    OMX_VIDEO_VP8ProfileKhronosExtensions   = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_VP8ProfileVendorStartUnused   = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_VP8ProfileMax                 = 0x7FFFFFFF
} OMX_VIDEO_VP8PROFILETYPE;

typedef enum OMX_VIDEO_VP8LEVELTYPE {
    OMX_VIDEO_VP8Level_Version0             = 0x01,
    OMX_VIDEO_VP8Level_Version1             = 0x02,
    OMX_VIDEO_VP8Level_Version2             = 0x04,
    OMX_VIDEO_VP8Level_Version3             = 0x08,
    OMX_VIDEO_VP8LevelUnknown               = 0x6EFFFFFF,
    OMX_VIDEO_VP8LevelKhronosExtensions     = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_VIDEO_VP8LevelVendorStartUnused     = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_VIDEO_VP8LevelMax                   = 0x7FFFFFFF
} OMX_VIDEO_VP8LEVELTYPE;

typedef struct OMX_VIDEO_PARAM_VP8TYPE {
    OMX_U32                     nSize;
    OMX_VERSIONTYPE             nVersion;
    OMX_U32                     nPortIndex;
    OMX_VIDEO_VP8PROFILETYPE    eProfile;
    OMX_VIDEO_VP8LEVELTYPE      eLevel;
    OMX_U32                     nDCTPartitions;
    OMX_BOOL                    bErrorResilientMode;
} OMX_VIDEO_PARAM_VP8TYPE;

typedef struct OMX_VIDEO_VP8REFERENCEFRAMETYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        nPreviousFrameRefresh;
    OMX_BOOL        bGoldenFrameRefresh;
    OMX_BOOL        bAlternateFrameRefresh;
    OMX_BOOL        bUsePreviousFrame;
    OMX_BOOL        bUseGoldenFrame;
    OMX_BOOL        bUseAlternateFrame;
} OMX_VIDEO_VP8REFERENCEFRAMETYPE;

typedef struct OMX_VIDEO_VP8REFERENCEFRAMEINFOTYPE {
    OMX_U32         nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32         nPortIndex;
    OMX_BOOL        bIsIntraFrame;
    OMX_BOOL        bIsGoldenOrAlternateFrame;
} OMX_VIDEO_VP8REFERENCEFRAMEINFOTYPE;
#endif

typedef enum _EXYNOS_OMX_INDEXTYPE
{
#define EXYNOS_INDEX_PARAM_ENABLE_THUMBNAIL "OMX.SEC.index.enableThumbnailMode"
    OMX_IndexParamEnableThumbnailMode       = 0x7F000001,
#define EXYNOS_INDEX_CONFIG_VIDEO_INTRAPERIOD "OMX.SEC.index.VideoIntraPeriod"
    OMX_IndexConfigVideoIntraPeriod         = 0x7F000002,
#ifdef USE_S3D_SUPPORT
#define EXYNOS_INDEX_PARAM_GET_S3D "OMX.SEC.index.S3DMode"
    OMX_IndexVendorS3DMode                  = 0x7F000003,
#endif
#define EXYNOS_INDEX_PARAM_NEED_CONTIG_MEMORY "OMX.SEC.index.NeedContigMemory"
    OMX_IndexVendorNeedContigMemory         = 0x7F000004,
#define EXYNOS_INDEX_CONFIG_GET_BUFFER_FD "OMX.SEC.index.GetBufferFD"
    OMX_IndexVendorGetBufferFD              = 0x7F000005,
#define EXYNOS_INDEX_PARAM_SET_DTS_MODE "OMX.SEC.index.SetDTSMode"
    OMX_IndexVendorSetDTSMode               = 0x7F000006,
#define EXYNOS_INDEX_CONFIG_SET_QOS_RATIO "OMX.SEC.index.SetQosRatio"
    OMX_IndexVendorSetQosRatio              = 0x7F000007,

    /* for Android Native Window */
#define EXYNOS_INDEX_PARAM_ENABLE_ANB "OMX.google.android.index.enableAndroidNativeBuffers"
    OMX_IndexParamEnableAndroidBuffers      = 0x7F000011,
#define EXYNOS_INDEX_PARAM_GET_ANB "OMX.google.android.index.getAndroidNativeBufferUsage"
    OMX_IndexParamGetAndroidNativeBuffer    = 0x7F000012,
#define EXYNOS_INDEX_PARAM_USE_ANB "OMX.google.android.index.useAndroidNativeBuffer"
    OMX_IndexParamUseAndroidNativeBuffer    = 0x7F000013,
    /* for Android Store Metadata Inbuffer */
#define EXYNOS_INDEX_PARAM_STORE_METADATA_BUFFER "OMX.google.android.index.storeMetaDataInBuffers"
    OMX_IndexParamStoreMetaDataBuffer       = 0x7F000014,
    /* prepend SPS/PPS to I/IDR for H.264 Encoder */
#define EXYNOS_INDEX_PARAM_PREPEND_SPSPPS_TO_IDR "OMX.google.android.index.prependSPSPPSToIDRFrames"
    OMX_IndexParamPrependSPSPPSToIDR        = 0x7F000015,

#ifdef USE_VP8ENC_SUPPORT
    /* for VP8 encoder */
#define EXYNOS_INDEX_PARAM_VIDEO_VP8_TYPE "OMX.SEC.index.VideoVp8Type"
    OMX_IndexParamVideoVp8                  = 0x7F00016,
#define EXYNOS_INDEX_CONFIG_VIDEO_VP8_REFERENCEFRAME "OMX.SEC.index.VideoVp8ReferenceFrame"
    OMX_IndexConfigVideoVp8ReferenceFrame   = 0x7F00017,
#define EXYNOS_INDEX_CONFIG_VIDEO_VP8_REFERENCEFRAMETYPE "OMX.SEC.index.VideoVp8ReferenceFrameType"
    OMX_IndexConfigVideoVp8ReferenceFrameType = 0x7F00018,
#endif

    /* for Android PV OpenCore*/
    OMX_COMPONENT_CAPABILITY_TYPE_INDEX     = 0xFF7A347
} EXYNOS_OMX_INDEXTYPE;

typedef enum _EXYNOS_OMX_ERRORTYPE
{
    OMX_ErrorNoEOF              = (OMX_S32) 0x90000001,
    OMX_ErrorInputDataDecodeYet = (OMX_S32) 0x90000002,
    OMX_ErrorInputDataEncodeYet = (OMX_S32) 0x90000003,
    OMX_ErrorCodecInit          = (OMX_S32) 0x90000004,
    OMX_ErrorCodecDecode        = (OMX_S32) 0x90000005,
    OMX_ErrorCodecEncode        = (OMX_S32) 0x90000006,
    OMX_ErrorCodecFlush         = (OMX_S32) 0x90000007,
    OMX_ErrorOutputBufferUseYet = (OMX_S32) 0x90000008,
    OMX_ErrorCorruptedFrame     = (OMX_S32) 0x90000009
} EXYNOS_OMX_ERRORTYPE;

typedef enum _EXYNOS_OMX_COMMANDTYPE
{
    EXYNOS_OMX_CommandComponentDeInit = 0x7F000001,
    EXYNOS_OMX_CommandEmptyBuffer,
    EXYNOS_OMX_CommandFillBuffer,
    EXYNOS_OMX_CommandFakeBuffer
} EXYNOS_OMX_COMMANDTYPE;

typedef enum _EXYNOS_OMX_TRANS_STATETYPE {
    EXYNOS_OMX_TransStateInvalid,
    EXYNOS_OMX_TransStateLoadedToIdle,
    EXYNOS_OMX_TransStateIdleToExecuting,
    EXYNOS_OMX_TransStateExecutingToIdle,
    EXYNOS_OMX_TransStateIdleToLoaded,
    EXYNOS_OMX_TransStateMax = 0X7FFFFFFF
} EXYNOS_OMX_TRANS_STATETYPE;

typedef enum _EXYNOS_OMX_COLOR_FORMATTYPE {
    OMX_SEC_COLOR_FormatNV12TPhysicalAddress        = 0x7F000001, /**< Reserved region for introducing Vendor Extensions */
    OMX_SEC_COLOR_FormatNV12LPhysicalAddress        = 0x7F000002,
    OMX_SEC_COLOR_FormatNV12LVirtualAddress         = 0x7F000003,
    OMX_SEC_COLOR_FormatNV21LPhysicalAddress        = 0x7F000010,
    OMX_SEC_COLOR_FormatNV21Linear                  = 0x7F000011,
    OMX_SEC_COLOR_FormatYVU420Planar                = 0x7F000012,

    /* to copy a encoded data for drm component using gsc or fimc */
    OMX_SEC_COLOR_FormatEncodedData                 = OMX_COLOR_FormatYCbYCr,
#ifdef USE_KHRONOS_OMX_HEADER
    /* for Android SurfaceMediaSource*/
    OMX_COLOR_FormatAndroidOpaque                   = 0x7F000789
#endif
}EXYNOS_OMX_COLOR_FORMATTYPE;

typedef enum _EXYNOS_OMX_SUPPORTFORMAT_TYPE
{
    supportFormat_0 = 0x00,
    supportFormat_1,
    supportFormat_2,
    supportFormat_3,
    supportFormat_4,
    supportFormat_5,
    supportFormat_6,
    supportFormat_7,
} EXYNOS_OMX_SUPPORTFORMAT_TYPE;

typedef enum _EXYNOS_OMX_BUFFERPROCESS_TYPE
{
    BUFFER_DEFAULT  = 0x00,
    BUFFER_COPY     = 0x01,
    BUFFER_SHARE    = 0x02,
    BUFFER_METADATA = 0x04,
    BUFFER_ANBSHARE = 0x08
} EXYNOS_OMX_BUFFERPROCESS_TYPE;

typedef struct _EXYNOS_OMX_VIDEO_PROFILELEVEL
{
    OMX_S32  profile;
    OMX_S32  level;
} EXYNOS_OMX_VIDEO_PROFILELEVEL;

typedef struct _EXYNOS_OMX_VIDEO_THUMBNAILMODE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bEnable;
} EXYNOS_OMX_VIDEO_THUMBNAILMODE;

#ifdef USE_S3D_SUPPORT
typedef enum _EXYNOS_OMX_FPARGMT_TYPE
{
    OMX_SEC_FPARGMT_INVALID           = -1,
    OMX_SEC_FPARGMT_CHECKERBRD_INTERL = 0x00,
    OMX_SEC_FPARGMT_COLUMN_INTERL     = 0x01,
    OMX_SEC_FPARGMT_ROW_INTERL        = 0x02,
    OMX_SEC_FPARGMT_SIDE_BY_SIDE      = 0x03,
    OMX_SEC_FPARGMT_TOP_BOTTOM        = 0x04,
    OMX_SEC_FPARGMT_TEMPORAL_INTERL   = 0x05,
    OMX_SEC_FPARGMT_NONE              = 0x0A
} EXYNOS_OMX_FPARGMT_TYPE;
#endif

typedef enum _EXYNOS_OMX_EVENTTYPE
{
   OMX_EventVendorStart    = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
#ifdef USE_S3D_SUPPORT
   OMX_EventS3DInformation,
#endif
} EXYNOS_OMX_EVENTTYPE;

#ifdef USE_KHRONOS_OMX_HEADER
#define OMX_VIDEO_CodingVPX     0x09    /**< Google VPX, formerly known as On2 VP8 */
#endif

#ifndef __OMX_EXPORTS
#define __OMX_EXPORTS
#define EXYNOS_EXPORT_REF __attribute__((visibility("default")))
#define EXYNOS_IMPORT_REF __attribute__((visibility("default")))
#endif

#endif
