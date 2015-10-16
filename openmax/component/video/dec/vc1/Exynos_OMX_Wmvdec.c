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
 * @file        Exynos_OMX_Wmvdec.c
 * @brief
 * @author      HyeYeon Chung (hyeon.chung@samsung.com)
 * @author      Satish Kumar Reddy (palli.satish@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.07.10 : Create
 *              : Support WMV3 (Vc-1 Simple/Main Profile)
 *              : Support WMvC1 (Vc-1 Advanced Profile)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Exynos_OMX_Macros.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OMX_Vdec.h"
#include "Exynos_OMX_VdecControl.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Thread.h"
#include "library_register.h"
#include "Exynos_OMX_Wmvdec.h"
#include "ExynosVideoApi.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Event.h"

#ifdef USE_ANB
#include "Exynos_OSAL_Android.h"
#endif

/* To use CSC_METHOD_HW in EXYNOS OMX, gralloc should allocate physical memory using FIMC */
/* It means GRALLOC_USAGE_HW_FIMC1 should be set on Native Window usage */
#include "csc.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_WMV_DEC"
#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"

#define WMV_DEC_NUM_OF_EXTRA_BUFFERS 7

//#define FULL_FRAME_SEARCH

/* ASF parser does not send start code on Stagefright */
#define WO_START_CODE
/* Enable or disable "WMV3_ADDITIONAL_START_CODE" based on MFC F/W's need */
#define WMV3_ADDITIONAL_START_CODE

WMV_FORMAT gWvmFormat = WMV_FORMAT_UNKNOWN;

const OMX_U32 wmv3 = 0x33564d57;
const OMX_U32 wvc1 = 0x31435657;
const OMX_U32 wmva = 0x41564d57;


static OMX_ERRORTYPE GetCodecInputPrivateData(OMX_PTR codecBuffer, void *pVirtAddr, OMX_U32 *dataSize)
{
    OMX_ERRORTYPE       ret = OMX_ErrorNone;

EXIT:
    return ret;
}

static OMX_ERRORTYPE GetCodecOutputPrivateData(OMX_PTR codecBuffer, void *addr[], int size[])
{
    OMX_ERRORTYPE       ret = OMX_ErrorNone;
    ExynosVideoBuffer  *pCodecBuffer;

    if (codecBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pCodecBuffer = (ExynosVideoBuffer *)codecBuffer;

    if (addr != NULL) {
        addr[0] = pCodecBuffer->planes[0].addr;
        addr[1] = pCodecBuffer->planes[1].addr;
        addr[2] = pCodecBuffer->planes[2].addr;
    }

    if (size != NULL) {
        size[0] = pCodecBuffer->planes[0].allocSize;
        size[1] = pCodecBuffer->planes[1].allocSize;
        size[2] = pCodecBuffer->planes[2].allocSize;
    }

EXIT:
    return ret;
}

int Check_Wmv_Frame(
    OMX_U8   *pInputStream,
    OMX_U32   buffSize,
    OMX_U32   flag,
    OMX_BOOL  bPreviousFrameEOF,
    OMX_BOOL *pbEndOfFrame)
{
    OMX_U32  compressionID;
    OMX_BOOL bFrameStart;
    OMX_U32  len, readStream;
    OMX_U32  startCode;

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "buffSize = %d", buffSize);

    len = 0;
    bFrameStart = OMX_FALSE;

    if (flag & OMX_BUFFERFLAG_CODECCONFIG) {
        BitmapInfoHhr *pBitmapInfoHeader;
        pBitmapInfoHeader = (BitmapInfoHhr *)pInputStream;

        compressionID = pBitmapInfoHeader->BiCompression;
        if (compressionID == wmv3) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "WMV_FORMAT_WMV3");
            gWvmFormat  = WMV_FORMAT_WMV3;

            *pbEndOfFrame = OMX_TRUE;
            return buffSize;
        }
        else if ((compressionID == wvc1) || (compressionID == wmva)) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "WMV_FORMAT_VC1");
            gWvmFormat  = WMV_FORMAT_VC1;

#ifdef WO_START_CODE
/* ASF parser does not send start code on Stagefright */
            *pbEndOfFrame = OMX_TRUE;
            return buffSize;
#endif
        }
    }

    if (gWvmFormat == WMV_FORMAT_WMV3) {
        *pbEndOfFrame = OMX_TRUE;
        return buffSize;
    }

#ifdef WO_START_CODE
/* ASF parser does not send start code on Stagefright */
    if (gWvmFormat == WMV_FORMAT_VC1) {
        *pbEndOfFrame = OMX_TRUE;
        return buffSize;
    }
#else
 /* TODO : for comformanc test based on common buffer scheme w/o parser */

    if (bPreviousFrameEOF == OMX_FALSE)
        bFrameStart = OMX_TRUE;

    startCode = 0xFFFFFFFF;
    if (bFrameStart == OMX_FALSE) {
        /* find Frame start code */
        while(startCode != 0x10D) {
            readStream = *(pInputStream + len);
            startCode = (startCode << 8) | readStream;
            len++;
            if (len > buffSize)
                goto EXIT;
        }
    }

    /* find next Frame start code */
    startCode = 0xFFFFFFFF;
    while ((startCode != 0x10D)) {
        readStream = *(pInputStream + len);
        startCode = (startCode << 8) | readStream;
        len++;
        if (len > buffSize)
            goto EXIT;
    }

    *pbEndOfFrame = OMX_TRUE;

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "1. Check_Wmv_Frame returned EOF = %d, len = %d, buffSize = %d", *pbEndOfFrame, len - 4, buffSize);

    return len - 4;
#endif

EXIT :
    *pbEndOfFrame = OMX_FALSE;

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "2. Check_Wmv_Frame returned EOF = %d, len = %d, buffSize = %d", *pbEndOfFrame, len - 1, buffSize);

    return --len;
}

static OMX_BOOL Check_Stream_PrefixCode(
    OMX_U8    *pInputStream,
    OMX_U32    streamSize,
    WMV_FORMAT wmvFormat)
{
    switch (wmvFormat) {
    case WMV_FORMAT_WMV3:
#ifdef WMV3_ADDITIONAL_START_CODE
        return OMX_FALSE;
#else
        if (streamSize > 0)
            return OMX_TRUE;
        else
            return OMX_FALSE;
#endif
        break;
    case WMV_FORMAT_VC1:
        /* TODO : for comformanc test based on common buffer scheme w/o parser */
        if (streamSize < 3) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: streamSize is too small (%d)", __FUNCTION__, streamSize);
            return OMX_FALSE;
        } else if ((pInputStream[0] == 0x00) &&
                   (pInputStream[1] == 0x00) &&
                   (pInputStream[2] == 0x01)) {
            return OMX_TRUE;
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: Cannot find prefix", __FUNCTION__);
            return OMX_FALSE;
        }
        break;

    default:
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "%s: undefined wmvFormat (%d)", __FUNCTION__, wmvFormat);
        return OMX_FALSE;
        break;
    }
}

static OMX_BOOL Make_Stream_MetaData(
    OMX_U8    *pInputStream,
    OMX_U32   *pStreamSize,
    WMV_FORMAT wmvFormat)
{
    OMX_U8  *pCurrBuf = pInputStream;
    OMX_U32  currPos  = 0;
    OMX_U32  width, height;

    FunctionIn();

    /* Sequence Layer Data Structure */
    OMX_U8 const_C5[4] = {0x00, 0x00, 0x00, 0xc5};
    OMX_U8 const_04[4] = {0x04, 0x00, 0x00, 0x00};
    OMX_U8 const_0C[4] = {0x0C, 0x00, 0x00, 0x00};
    OMX_U8 struct_B_1[4] = {0xB3, 0x19, 0x00, 0x00};
    OMX_U8 struct_B_2[4] = {0x44, 0x62, 0x05, 0x00};
    OMX_U8 struct_B_3[4] = {0x0F, 0x00, 0x00, 0x00};
    OMX_U8 struct_C[4] = {0x30, 0x00, 0x00, 0x00};

    switch (wmvFormat) {
    case WMV_FORMAT_WMV3:
        if (*pStreamSize >= BITMAPINFOHEADER_SIZE) {
            BitmapInfoHhr *pBitmapInfoHeader;
            pBitmapInfoHeader = (BitmapInfoHhr *)pInputStream;

            width = pBitmapInfoHeader->BiWidth;
            height = pBitmapInfoHeader->BiHeight;
            if (*pStreamSize > BITMAPINFOHEADER_SIZE)
                Exynos_OSAL_Memcpy(struct_C, pInputStream+BITMAPINFOHEADER_SIZE, 4);

            Exynos_OSAL_Memcpy(pCurrBuf + currPos, const_C5, 4);
            currPos +=4;

            Exynos_OSAL_Memcpy(pCurrBuf + currPos, const_04, 4);
            currPos +=4;

            Exynos_OSAL_Memcpy(pCurrBuf + currPos, struct_C, 4);
            currPos +=4;

            /* struct_A : VERT_SIZE */
            pCurrBuf[currPos] =  height & 0xFF;
            pCurrBuf[currPos+1] = (height>>8) & 0xFF;
            pCurrBuf[currPos+2] = (height>>16) & 0xFF;
            pCurrBuf[currPos+3] = (height>>24) & 0xFF;
            currPos +=4;

            /* struct_A : HORIZ_SIZE */
            pCurrBuf[currPos] =  width & 0xFF;
            pCurrBuf[currPos+1] = (width>>8) & 0xFF;
            pCurrBuf[currPos+2] = (width>>16) & 0xFF;
            pCurrBuf[currPos+3] = (width>>24) & 0xFF;
            currPos +=4;

            Exynos_OSAL_Memcpy(pCurrBuf + currPos,const_0C, 4);
            currPos +=4;

            Exynos_OSAL_Memcpy(pCurrBuf + currPos, struct_B_1, 4);
            currPos +=4;

            Exynos_OSAL_Memcpy(pCurrBuf + currPos, struct_B_2, 4);
            currPos +=4;

            Exynos_OSAL_Memcpy(pCurrBuf + currPos, struct_B_3, 4);
            currPos +=4;

            *pStreamSize = currPos;
            return OMX_TRUE;
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: *pStreamSize is too small to contain metadata(%d)", __FUNCTION__, *pStreamSize);
            return OMX_FALSE;
        }
        break;
    case WMV_FORMAT_VC1:
        if (*pStreamSize >= BITMAPINFOHEADER_ASFBINDING_SIZE) {
            Exynos_OSAL_Memcpy(pCurrBuf, pInputStream + BITMAPINFOHEADER_ASFBINDING_SIZE, *pStreamSize - BITMAPINFOHEADER_ASFBINDING_SIZE);
            *pStreamSize -= BITMAPINFOHEADER_ASFBINDING_SIZE;
            return OMX_TRUE;
        } else {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: *pStreamSize is too small to contain metadata(%d)", __FUNCTION__, *pStreamSize);
            return OMX_FALSE;
        }
        break;
    default:
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "%s: It is not necessary to make bitstream metadata for wmvFormat (%d)", __FUNCTION__, wmvFormat);
        return OMX_FALSE;
        break;
    }
}

static OMX_BOOL Make_Stream_StartCode(
    OMX_U8    *pInputStream,
    OMX_U32    *pStreamSize,
    WMV_FORMAT wmvFormat)
{
    OMX_U8  frameStartCode[4] = {0x00, 0x00, 0x01, 0x0d};
#ifdef WMV3_ADDITIONAL_START_CODE
     /* first 4 bytes : size of Frame, second 4 bytes : present Time stamp */
    OMX_U8  frameStartCode2[8] = {0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
#endif
    OMX_U32 i;

    switch (wmvFormat) {
    case WMV_FORMAT_WMV3:
#ifdef WMV3_ADDITIONAL_START_CODE
        Exynos_OSAL_Memmove(pInputStream+8, pInputStream, *pStreamSize);
        Exynos_OSAL_Memcpy(pInputStream, frameStartCode2, 8);
        *pStreamSize += 8;
#endif
        return OMX_TRUE;
        break;

    case WMV_FORMAT_VC1:
        /* Should find better way to shift data */
        Exynos_OSAL_Memmove(pInputStream+4, pInputStream, *pStreamSize);
        Exynos_OSAL_Memcpy(pInputStream, frameStartCode, 4);
        *pStreamSize += 4;
        return OMX_TRUE;
        break;

    default:
        Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "%s: undefined wmvFormat (%d)", __FUNCTION__, wmvFormat);
        return OMX_FALSE;
        break;
    }
}

OMX_ERRORTYPE WmvCodecOpen(EXYNOS_WMVDEC_HANDLE *pWmvDec)
{
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if (pWmvDec == NULL) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }

    /* alloc ops structure */
    pDecOps    = (ExynosVideoDecOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecOps));
    pInbufOps  = (ExynosVideoDecBufferOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecBufferOps));
    pOutbufOps = (ExynosVideoDecBufferOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecBufferOps));

    if ((pDecOps == NULL) || (pInbufOps == NULL) || (pOutbufOps == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to allocate decoder ops buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pWmvDec->hMFCWmvHandle.pDecOps    = pDecOps;
    pWmvDec->hMFCWmvHandle.pInbufOps  = pInbufOps;
    pWmvDec->hMFCWmvHandle.pOutbufOps = pOutbufOps;

    /* function pointer mapping */
    pDecOps->nSize    = sizeof(ExynosVideoDecOps);
    pInbufOps->nSize  = sizeof(ExynosVideoDecBufferOps);
    pOutbufOps->nSize = sizeof(ExynosVideoDecBufferOps);

    Exynos_Video_Register_Decoder(pDecOps, pInbufOps, pOutbufOps);

    /* check mandatory functions for decoder ops */
    if ((pDecOps->Init == NULL) || (pDecOps->Finalize == NULL) ||
        (pDecOps->Get_ActualBufferCount == NULL) || (pDecOps->Set_FrameTag == NULL) ||
        (pDecOps->Get_FrameTag == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Mandatory functions must be supplied");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* check mandatory functions for buffer ops */
    if ((pInbufOps->Setup == NULL) || (pOutbufOps->Setup == NULL) ||
        (pInbufOps->Run == NULL) || (pOutbufOps->Run == NULL) ||
        (pInbufOps->Stop == NULL) || (pOutbufOps->Stop == NULL) ||
        (pInbufOps->Enqueue == NULL) || (pOutbufOps->Enqueue == NULL) ||
        (pInbufOps->Dequeue == NULL) || (pOutbufOps->Dequeue == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Mandatory functions must be supplied");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* alloc context, open, querycap */
#ifdef USE_DMA_BUF
    pWmvDec->hMFCWmvHandle.hMFCHandle = pWmvDec->hMFCWmvHandle.pDecOps->Init(V4L2_MEMORY_DMABUF);
#else
    pWmvDec->hMFCWmvHandle.hMFCHandle = pWmvDec->hMFCWmvHandle.pDecOps->Init(V4L2_MEMORY_USERPTR);
#endif
    if (pWmvDec->hMFCWmvHandle.hMFCHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to allocate context buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    if (ret != OMX_ErrorNone) {
        if (pDecOps != NULL) {
            Exynos_OSAL_Free(pDecOps);
            pWmvDec->hMFCWmvHandle.pDecOps = NULL;
        }
        if (pInbufOps != NULL) {
            Exynos_OSAL_Free(pInbufOps);
            pWmvDec->hMFCWmvHandle.pInbufOps = NULL;
        }
        if (pOutbufOps != NULL) {
            Exynos_OSAL_Free(pOutbufOps);
            pWmvDec->hMFCWmvHandle.pOutbufOps = NULL;
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE WmvCodecClose(EXYNOS_WMVDEC_HANDLE *pWmvDec)
{
    OMX_ERRORTYPE            ret = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if (pWmvDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pWmvDec->hMFCWmvHandle.hMFCHandle;
    pDecOps    = pWmvDec->hMFCWmvHandle.pDecOps;
    pInbufOps  = pWmvDec->hMFCWmvHandle.pInbufOps;
    pOutbufOps = pWmvDec->hMFCWmvHandle.pOutbufOps;

    if (hMFCHandle != NULL) {
        pDecOps->Finalize(hMFCHandle);
        pWmvDec->hMFCWmvHandle.hMFCHandle = NULL;
    }
    if (pOutbufOps != NULL) {
        Exynos_OSAL_Free(pOutbufOps);
        pWmvDec->hMFCWmvHandle.pOutbufOps = NULL;
    }
    if (pInbufOps != NULL) {
        Exynos_OSAL_Free(pInbufOps);
        pWmvDec->hMFCWmvHandle.pInbufOps = NULL;
    }
    if (pDecOps != NULL) {
        Exynos_OSAL_Free(pDecOps);
        pWmvDec->hMFCWmvHandle.pDecOps = NULL;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE WmvCodecStart(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE            ret = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    EXYNOS_WMVDEC_HANDLE   *pWmvDec = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)((EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate)->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pWmvDec = (EXYNOS_WMVDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pWmvDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pWmvDec->hMFCWmvHandle.hMFCHandle;
    pDecOps    = pWmvDec->hMFCWmvHandle.pDecOps;
    pInbufOps  = pWmvDec->hMFCWmvHandle.pInbufOps;
    pOutbufOps = pWmvDec->hMFCWmvHandle.pOutbufOps;

    if (nPortIndex == INPUT_PORT_INDEX)
        pInbufOps->Run(hMFCHandle);
    else if (nPortIndex == OUTPUT_PORT_INDEX)
        pOutbufOps->Run(hMFCHandle);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE WmvCodecStop(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE            ret = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    EXYNOS_WMVDEC_HANDLE   *pWmvDec = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)((EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate)->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pWmvDec = (EXYNOS_WMVDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pWmvDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pWmvDec->hMFCWmvHandle.hMFCHandle;
    pDecOps    = pWmvDec->hMFCWmvHandle.pDecOps;
    pInbufOps  = pWmvDec->hMFCWmvHandle.pInbufOps;
    pOutbufOps = pWmvDec->hMFCWmvHandle.pOutbufOps;

    if ((nPortIndex == INPUT_PORT_INDEX) && (pInbufOps != NULL))
        pInbufOps->Stop(hMFCHandle);
    else if ((nPortIndex == OUTPUT_PORT_INDEX) && (pOutbufOps != NULL))
        pOutbufOps->Stop(hMFCHandle);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE WmvCodecOutputBufferProcessRun(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE            ret = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    EXYNOS_WMVDEC_HANDLE   *pWmvDec = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)((EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate)->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pWmvDec = (EXYNOS_WMVDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pWmvDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pWmvDec->hMFCWmvHandle.hMFCHandle;
    pDecOps    = pWmvDec->hMFCWmvHandle.pDecOps;
    pInbufOps  = pWmvDec->hMFCWmvHandle.pInbufOps;
    pOutbufOps = pWmvDec->hMFCWmvHandle.pOutbufOps;

    if (nPortIndex == INPUT_PORT_INDEX) {
        if (pWmvDec->bSourceStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pWmvDec->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    if (nPortIndex == OUTPUT_PORT_INDEX) {
        if (pWmvDec->bDestinationStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pWmvDec->hDestinationStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE WMVCodecRegistCodecBuffers(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex,
    OMX_U32              nBufferCnt)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMVDEC_HANDLE            *pWmvDec            = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                            *hMFCHandle         = pWmvDec->hMFCWmvHandle.hMFCHandle;
    CODEC_DEC_BUFFER               **ppCodecBuffer      = NULL;
    ExynosVideoDecBufferOps         *pBufOps            = NULL;
    ExynosVideoPlane                *pPlanes            = NULL;

    OMX_U32 nPlaneCnt = 0;
    int i, j;

    FunctionIn();

    if (nPortIndex == INPUT_PORT_INDEX) {
        ppCodecBuffer   = &(pVideoDec->pMFCDecInputBuffer[0]);
        pBufOps         = pWmvDec->hMFCWmvHandle.pInbufOps;
    } else {
        ppCodecBuffer   = &(pVideoDec->pMFCDecOutputBuffer[0]);
        pBufOps         = pWmvDec->hMFCWmvHandle.pOutbufOps;
    }
    nPlaneCnt = pExynosComponent->pExynosPort[nPortIndex].nPlaneCnt;

    pPlanes = (ExynosVideoPlane *)Exynos_OSAL_Malloc(sizeof(ExynosVideoPlane) * nPlaneCnt);
    if (pPlanes == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* Register buffer */
    for (i = 0; i < nBufferCnt; i++) {
        for (j = 0; j < nPlaneCnt; j++) {
            pPlanes[j].addr         = ppCodecBuffer[i]->pVirAddr[j];
            pPlanes[j].fd           = ppCodecBuffer[i]->fd[j];
            pPlanes[j].allocSize    = ppCodecBuffer[i]->bufferSize[j];
        }

        if (pBufOps->Register(hMFCHandle, pPlanes, nPlaneCnt) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "PORT[%d]: Failed to Register buffer", nPortIndex);
            ret = OMX_ErrorInsufficientResources;
            Exynos_OSAL_Free(pPlanes);
            goto EXIT;
        }
    }

    Exynos_OSAL_Free(pPlanes);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE WmvCodecEnQueueAllBuffer(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMVDEC_HANDLE         *pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pWmvDec->hMFCWmvHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    int i, nOutbufs;

    ExynosVideoDecOps       *pDecOps    = pWmvDec->hMFCWmvHandle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pWmvDec->hMFCWmvHandle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pWmvDec->hMFCWmvHandle.pOutbufOps;

    FunctionIn();

    if ((nPortIndex != INPUT_PORT_INDEX) && (nPortIndex != OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((nPortIndex == INPUT_PORT_INDEX) &&
        (pWmvDec->bSourceStart == OMX_TRUE)) {
        Exynos_CodecBufferReset(pExynosComponent, INPUT_PORT_INDEX);

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecInputBuffer[%d]: 0x%x", i, pVideoDec->pMFCDecInputBuffer[i]);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecInputBuffer[%d]->pVirAddr[0]: 0x%x", i, pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[0]);

            Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, pVideoDec->pMFCDecInputBuffer[i]);
        }

        pInbufOps->Clear_Queue(hMFCHandle);
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pWmvDec->bDestinationStart == OMX_TRUE)) {
        Exynos_CodecBufferReset(pExynosComponent, OUTPUT_PORT_INDEX);

        for (i = 0; i < pWmvDec->hMFCWmvHandle.maxDPBNum; i++) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecOutputBuffer[%d]: 0x%x", i, pVideoDec->pMFCDecOutputBuffer[i]);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecOutputBuffer[%d]->pVirAddr[0]: 0x%x", i, pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[0]);

            Exynos_CodecBufferEnQueue(pExynosComponent, OUTPUT_PORT_INDEX, pVideoDec->pMFCDecOutputBuffer[i]);
        }
        pOutbufOps->Clear_Queue(hMFCHandle);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE WmvCodecSrcSetup(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMVDEC_HANDLE          *pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pWmvDec->hMFCWmvHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_U32                     oneFrameSize = pSrcInputData->dataLen;
    OMX_BOOL                    bMetaData         = OMX_FALSE;

    ExynosVideoDecOps       *pDecOps    = pWmvDec->hMFCWmvHandle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pWmvDec->hMFCWmvHandle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pWmvDec->hMFCWmvHandle.pOutbufOps;
    ExynosVideoGeometry      bufferConf;
    OMX_U32                  inputBufferNumber = 0;
    int i;

    FunctionIn();

    if ((oneFrameSize <= 0) && (pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS)) {
        BYPASS_BUFFER_INFO *pBufferInfo = (BYPASS_BUFFER_INFO *)Exynos_OSAL_Malloc(sizeof(BYPASS_BUFFER_INFO));
        if (pBufferInfo == NULL) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        pBufferInfo->nFlags     = pSrcInputData->nFlags;
        pBufferInfo->timeStamp  = pSrcInputData->timeStamp;
        ret = Exynos_OSAL_Queue(&pWmvDec->bypassBufferInfoQ, (void *)pBufferInfo);
        Exynos_OSAL_SignalSet(pWmvDec->hDestinationStartEvent);

        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pVideoDec->bThumbnailMode == OMX_TRUE)
        pDecOps->Set_DisplayDelay(hMFCHandle, 0);

    if ((pDecOps->Enable_DTSMode != NULL) &&
        (pVideoDec->bDTSMode == OMX_TRUE))
        pDecOps->Enable_DTSMode(hMFCHandle);

    if (pSrcInputData->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
        BitmapInfoHhr *pBitmapInfoHeader;
        pBitmapInfoHeader = (BitmapInfoHhr *)pSrcInputData->buffer.singlePlaneBuffer.dataBuffer;
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "OMX_BUFFERFLAG_CODECCONFIG");
        if (pBitmapInfoHeader->BiCompression == wmv3) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "WMV_FORMAT_WMV3");
            pWmvDec->hMFCWmvHandle.wmvFormat  = WMV_FORMAT_WMV3;
        } else if ((pBitmapInfoHeader->BiCompression == wvc1) || (pBitmapInfoHeader->BiCompression == wmva)) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "WMV_FORMAT_VC1");
            pWmvDec->hMFCWmvHandle.wmvFormat  = WMV_FORMAT_VC1;
        }
    }

    /* input buffer info */
    Exynos_OSAL_Memset(&bufferConf, 0, sizeof(bufferConf));
    if (pWmvDec->hMFCWmvHandle.wmvFormat == WMV_FORMAT_WMV3) {
        bufferConf.eCompressionFormat = VIDEO_CODING_VC1_RCV;
    } else if (pWmvDec->hMFCWmvHandle.wmvFormat == WMV_FORMAT_VC1) {
        bufferConf.eCompressionFormat = VIDEO_CODING_VC1;
    } else {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Unsupported WMV Codec Format Type");
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    pInbufOps->Set_Shareable(hMFCHandle);
    if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
        bufferConf.nSizeImage = pExynosInputPort->portDefinition.format.video.nFrameWidth
                                * pExynosInputPort->portDefinition.format.video.nFrameHeight * 3 / 2;
    } else if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        bufferConf.nSizeImage = DEFAULT_MFC_INPUT_BUFFER_SIZE;
    }
    bufferConf.nPlaneCnt = pExynosInputPort->nPlaneCnt;
    inputBufferNumber = MAX_INPUTBUFFER_NUM_DYNAMIC;

    /* should be done before prepare input buffer */
    if (pInbufOps->Enable_Cacheable(hMFCHandle) != VIDEO_ERROR_NONE) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* set input buffer geometry */
    if (pInbufOps->Set_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to set geometry for input buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* setup input buffer */
    if (pInbufOps->Setup(hMFCHandle, inputBufferNumber) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to setup input buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* set output geometry */
    Exynos_OSAL_Memset(&bufferConf, 0, sizeof(bufferConf));

#ifdef USE_DUALDPB_MODE
    switch (pExynosOutputPort->portDefinition.format.video.eColorFormat) {
    case OMX_COLOR_FormatYUV420SemiPlanar:
        bufferConf.eColorFormat = VIDEO_COLORFORMAT_NV12;
        break;
    case OMX_COLOR_FormatYUV420Planar:
        bufferConf.eColorFormat = VIDEO_COLORFORMAT_I420;
        break;
    case OMX_SEC_COLOR_FormatYVU420Planar:
        bufferConf.eColorFormat = VIDEO_COLORFORMAT_YV12;
        break;
    case OMX_SEC_COLOR_FormatNV21Linear:
        bufferConf.eColorFormat = VIDEO_COLORFORMAT_NV21;
        break;
    case OMX_SEC_COLOR_FormatNV12Tiled:
        bufferConf.eColorFormat = VIDEO_COLORFORMAT_NV12_TILED;
        break;
    default:
        bufferConf.eColorFormat = VIDEO_COLORFORMAT_NV12_TILED;
        break;
    }

    if (bufferConf.eColorFormat != VIDEO_COLORFORMAT_NV12_TILED) {
        if ((pDecOps->Enable_DualDPBMode != NULL) &&
            (pDecOps->Enable_DualDPBMode(hMFCHandle) == VIDEO_ERROR_NONE)) {
            pVideoDec->bDualDPBMode = OMX_TRUE;
            pExynosOutputPort->nPlaneCnt = Exynos_OSAL_GetPlaneCount(pExynosOutputPort->portDefinition.format.video.eColorFormat);
        } else {
            bufferConf.eColorFormat = VIDEO_COLORFORMAT_NV12_TILED;
            pExynosOutputPort->nPlaneCnt = MFC_DEFAULT_OUTPUT_BUFFER_PLANE;
        }
    }
    pWmvDec->hMFCWmvHandle.MFCOutputColorType = bufferConf.eColorFormat;
#else
    pWmvDec->hMFCWmvHandle.MFCOutputColorType = bufferConf.eColorFormat = VIDEO_COLORFORMAT_NV12_TILED;
#endif
    bufferConf.nPlaneCnt = pExynosOutputPort->nPlaneCnt;
    if (pOutbufOps->Set_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to set geometry for output buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    bMetaData = Make_Stream_MetaData(pSrcInputData->buffer.singlePlaneBuffer.dataBuffer, &oneFrameSize, pWmvDec->hMFCWmvHandle.wmvFormat);
    if (bMetaData == OMX_FALSE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Fail to Make Stream MetaData");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* input buffer enqueue for header parsing */
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "oneFrameSize: %d", oneFrameSize);
    OMX_U32 nAllocLen[MAX_BUFFER_PLANE] = {pSrcInputData->bufferHeader->nAllocLen, 0, 0};
    if (pInbufOps->ExtensionEnqueue(hMFCHandle,
                            (unsigned char **)&pSrcInputData->buffer.singlePlaneBuffer.dataBuffer,
                            (unsigned char **)&pSrcInputData->buffer.singlePlaneBuffer.fd,
                            (unsigned int *)nAllocLen, (unsigned int *)&oneFrameSize,
                            pExynosInputPort->nPlaneCnt, pSrcInputData->bufferHeader) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to enqueue input buffer for header parsing");
//        ret = OMX_ErrorInsufficientResources;
        ret = (OMX_ERRORTYPE)OMX_ErrorCodecInit;
        goto EXIT;
    }

    /* start header parsing */
    if (pInbufOps->Run(hMFCHandle) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to run input buffer for header parsing");
        ret = OMX_ErrorCodecInit;
        goto EXIT;
    }

    /* get geometry for output */
    Exynos_OSAL_Memset(&pWmvDec->hMFCWmvHandle.codecOutbufConf, 0, sizeof(ExynosVideoGeometry));
    if (pOutbufOps->Get_Geometry(hMFCHandle, &pWmvDec->hMFCWmvHandle.codecOutbufConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to get geometry for parsed header info");
        WmvCodecStop(pOMXComponent, INPUT_PORT_INDEX);
        pInbufOps->Cleanup_Buffer(hMFCHandle);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* get dpb count */
    pWmvDec->hMFCWmvHandle.maxDPBNum = pDecOps->Get_ActualBufferCount(hMFCHandle);
    if (pVideoDec->bThumbnailMode == OMX_FALSE)
        pWmvDec->hMFCWmvHandle.maxDPBNum += EXTRA_DPB_NUM;
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "WmvCodecSetup nOutbufs: %d", pWmvDec->hMFCWmvHandle.maxDPBNum);

    pWmvDec->hMFCWmvHandle.bConfiguredMFCSrc = OMX_TRUE;

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        if ((pExynosInputPort->portDefinition.format.video.nFrameWidth != pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameWidth) ||
            (pExynosInputPort->portDefinition.format.video.nFrameHeight != pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameHeight)) {
            pExynosOutputPort->exceptionFlag = NEED_PORT_DISABLE;

            pExynosInputPort->portDefinition.format.video.nFrameWidth = pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameWidth;
            pExynosInputPort->portDefinition.format.video.nFrameHeight = pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameHeight;
            pExynosInputPort->portDefinition.format.video.nStride = ((pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameWidth + 15) & (~15));
            pExynosInputPort->portDefinition.format.video.nSliceHeight = ((pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameHeight + 15) & (~15));

            Exynos_UpdateFrameSize(pOMXComponent);

            /** Send Port Settings changed call back **/
            (*(pExynosComponent->pCallbacks->EventHandler))
                (pOMXComponent,
                 pExynosComponent->callbackData,
                 OMX_EventPortSettingsChanged, /* The command was completed */
                 OMX_DirOutput, /* This is the port index */
                 0,
                 NULL);
        }
    } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        if ((pExynosInputPort->portDefinition.format.video.nFrameWidth != pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameWidth) ||
            (pExynosInputPort->portDefinition.format.video.nFrameHeight != pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameHeight) ||
            (pExynosOutputPort->portDefinition.nBufferCountActual != pWmvDec->hMFCWmvHandle.maxDPBNum)) {
            pExynosOutputPort->exceptionFlag = NEED_PORT_DISABLE;

            pExynosInputPort->portDefinition.format.video.nFrameWidth = pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameWidth;
            pExynosInputPort->portDefinition.format.video.nFrameHeight = pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameHeight;
            pExynosInputPort->portDefinition.format.video.nStride = ((pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameWidth + 15) & (~15));
            pExynosInputPort->portDefinition.format.video.nSliceHeight = ((pWmvDec->hMFCWmvHandle.codecOutbufConf.nFrameHeight + 15) & (~15));

            pExynosOutputPort->portDefinition.nBufferCountActual = pWmvDec->hMFCWmvHandle.maxDPBNum - 2;
            pExynosOutputPort->portDefinition.nBufferCountMin = pWmvDec->hMFCWmvHandle.maxDPBNum - 2;

            Exynos_UpdateFrameSize(pOMXComponent);

            /** Send Port Settings changed call back **/
            (*(pExynosComponent->pCallbacks->EventHandler))
                (pOMXComponent,
                 pExynosComponent->callbackData,
                 OMX_EventPortSettingsChanged, /* The command was completed */
                 OMX_DirOutput, /* This is the port index */
                 0,
                 NULL);
        }
    }
    Exynos_OSAL_SleepMillisec(0);
    ret = OMX_ErrorInputDataDecodeYet;
    WmvCodecStop(pOMXComponent, INPUT_PORT_INDEX);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE WmvCodecDstSetup(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMVDEC_HANDLE         *pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pWmvDec->hMFCWmvHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoDecOps       *pDecOps    = pWmvDec->hMFCWmvHandle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pWmvDec->hMFCWmvHandle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pWmvDec->hMFCWmvHandle.pOutbufOps;

    int i, nOutbufs;

    OMX_U32 nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    OMX_U32 dataLen[MAX_BUFFER_PLANE]   = {0, 0, 0};

    FunctionIn();

    for (i = 0; i < pExynosOutputPort->nPlaneCnt; i++)
        nAllocLen[i] = pWmvDec->hMFCWmvHandle.codecOutbufConf.nAlignPlaneSize[i];

    pOutbufOps->Set_Shareable(hMFCHandle);

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        /* should be done before prepare output buffer */
        if (pOutbufOps->Enable_Cacheable(hMFCHandle) != VIDEO_ERROR_NONE) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        /* get dpb count */
        nOutbufs = pWmvDec->hMFCWmvHandle.maxDPBNum;
        if (pOutbufOps->Setup(hMFCHandle, nOutbufs) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to setup output buffer");
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        ret = Exynos_Allocate_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX, nOutbufs, nAllocLen);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        ret = WMVCodecRegistCodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX, nOutbufs);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        /* Enqueue output buffer */
        for (i = 0; i < nOutbufs; i++) {
            pOutbufOps->Enqueue(hMFCHandle, (unsigned char **)pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr,
                            (unsigned int *)dataLen, pExynosOutputPort->nPlaneCnt, NULL);
        }

        if (pOutbufOps->Run(hMFCHandle) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to run output buffer");
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        ExynosVideoPlane planes[MAX_BUFFER_PLANE] = {0, 0, 0};
        int plane;

        /* get dpb count */
        nOutbufs = pExynosOutputPort->portDefinition.nBufferCountActual;
        if (pOutbufOps->Setup(hMFCHandle, nOutbufs) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to setup output buffer");
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        /* Register output buffer */
        /*************/
        /*    TBD    */
        /*************/
#ifdef USE_ANB
        if (pExynosOutputPort->bIsANBEnabled == OMX_TRUE) {
            for (i = 0; i < pExynosOutputPort->assignedBufferNum; i++) {
                for (plane = 0; plane < pExynosOutputPort->nPlaneCnt; plane++) {
                    planes[plane].fd = pExynosOutputPort->extendBufferHeader[i].buf_fd[plane];
                    planes[plane].addr = pExynosOutputPort->extendBufferHeader[i].pYUVBuf[plane];
                    planes[plane].allocSize = nAllocLen[plane];
                }

                if (pOutbufOps->Register(hMFCHandle, planes, pExynosOutputPort->nPlaneCnt) != VIDEO_ERROR_NONE) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to Register output buffer");
                    ret = OMX_ErrorInsufficientResources;
                    goto EXIT;
                }
                pOutbufOps->Enqueue(hMFCHandle, (unsigned char **)pExynosOutputPort->extendBufferHeader[i].pYUVBuf,
                              (unsigned int *)dataLen, pExynosOutputPort->nPlaneCnt, NULL);
            }

            if (pOutbufOps->Apply_RegisteredBuffer(hMFCHandle) != VIDEO_ERROR_NONE) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to Apply output buffer");
                ret = OMX_ErrorHardware;
                goto EXIT;
            }
        } else {
            ret = OMX_ErrorNotImplemented;
            goto EXIT;
        }
#else
        ret = OMX_ErrorNotImplemented;
        goto EXIT;
#endif
    }

    pWmvDec->hMFCWmvHandle.bConfiguredMFCDst = OMX_TRUE;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid ) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nParamIndex) {
    case OMX_IndexParamVideoWmv:
    {
        OMX_VIDEO_PARAM_WMVTYPE *pDstWmvParam = (OMX_VIDEO_PARAM_WMVTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_WMVTYPE *pSrcWmvParam = NULL;
        EXYNOS_WMVDEC_HANDLE    *pWmvDec = NULL;
        ret = Exynos_OMX_Check_SizeVersion(pDstWmvParam, sizeof(OMX_VIDEO_PARAM_WMVTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstWmvParam->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pSrcWmvParam = &pWmvDec->WmvComponent[pDstWmvParam->nPortIndex];

        Exynos_OSAL_Memcpy(pDstWmvParam, pSrcWmvParam, sizeof(OMX_VIDEO_PARAM_WMVTYPE));
    }
        break;

    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;
        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_WMV_DEC_ROLE);
    }
        break;
    case OMX_IndexParamVideoErrorCorrection:
    {
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pDstErrorCorrectionType = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pSrcErrorCorrectionType = NULL;
        EXYNOS_WMVDEC_HANDLE                *pWmvDec                 = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstErrorCorrectionType, sizeof(OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstErrorCorrectionType->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pSrcErrorCorrectionType = &pWmvDec->errorCorrectionType[INPUT_PORT_INDEX];

        pDstErrorCorrectionType->bEnableHEC = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    default:
        ret = Exynos_OMX_VideoDecodeGetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid ) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nIndex) {
    case OMX_IndexParamVideoWmv:
    {
        OMX_VIDEO_PARAM_WMVTYPE *pDstWmvParam = NULL;
        OMX_VIDEO_PARAM_WMVTYPE *pSrcWmvParam = (OMX_VIDEO_PARAM_WMVTYPE *)pComponentParameterStructure;
        EXYNOS_WMVDEC_HANDLE    *pWmvDec = NULL;
        ret = Exynos_OMX_Check_SizeVersion(pSrcWmvParam, sizeof(OMX_VIDEO_PARAM_WMVTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pSrcWmvParam->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pDstWmvParam = &pWmvDec->WmvComponent[pSrcWmvParam->nPortIndex];

        Exynos_OSAL_Memcpy(pDstWmvParam, pSrcWmvParam, sizeof(OMX_VIDEO_PARAM_WMVTYPE));
    }
        break;
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((pExynosComponent->currentState != OMX_StateLoaded) && (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (!Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_WMV_DEC_ROLE)) {
            pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
        } else {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoErrorCorrection:
    {
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pSrcErrorCorrectionType = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pDstErrorCorrectionType = NULL;
        EXYNOS_WMVDEC_HANDLE                *pWmvDec                 = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcErrorCorrectionType, sizeof(OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pSrcErrorCorrectionType->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pDstErrorCorrectionType = &pWmvDec->errorCorrectionType[INPUT_PORT_INDEX];

        pDstErrorCorrectionType->bEnableHEC = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    default:
        ret = Exynos_OMX_VideoDecodeSetParameter(hComponent, nIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_GetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nIndex,
    OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentConfigStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nIndex) {
    default:
        ret = Exynos_OMX_VideoDecodeGetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_SetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nIndex,
    OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentConfigStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nIndex) {
    default:
        ret = Exynos_OMX_VideoDecodeSetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_GetExtensionIndex(
    OMX_IN  OMX_HANDLETYPE  hComponent,
    OMX_IN  OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE   *pIndexType)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if ((cParameterName == NULL) || (pIndexType == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_ENABLE_THUMBNAIL) == 0) {
        EXYNOS_WMVDEC_HANDLE *pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        ret = OMX_ErrorNone;
    } else {
        ret = Exynos_OMX_VideoDecodeGetExtensionIndex(hComponent, cParameterName, pIndexType);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_ComponentRoleEnum(
    OMX_HANDLETYPE hComponent,
    OMX_U8        *cRole,
    OMX_U32        nIndex)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if ((hComponent == NULL) || (cRole == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (nIndex == (MAX_COMPONENT_ROLE_NUM-1)) {
        Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_WMV_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorNoMore;
    }

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Init */
OMX_ERRORTYPE Exynos_WmvDec_Init(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT      *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_WMVDEC_HANDLE          *pWmvDec           = (EXYNOS_WMVDEC_HANDLE *)pVideoDec->hCodecHandle;
    OMX_PTR                hMFCHandle = pWmvDec->hMFCWmvHandle.hMFCHandle;

    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    CSC_METHOD csc_method = CSC_METHOD_SW;
    int i, plane;

    FunctionIn();

    pWmvDec->hMFCWmvHandle.bConfiguredMFCSrc = OMX_FALSE;
    pWmvDec->hMFCWmvHandle.bConfiguredMFCDst = OMX_FALSE;
    pExynosComponent->bUseFlagEOF = OMX_TRUE;
    pExynosComponent->bSaveFlagEOS = OMX_FALSE;
    pExynosComponent->bBehaviorEOS = OMX_FALSE;

    /* WMV Codec Open */
    ret = WmvCodecOpen(pWmvDec);
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    pDecOps    = pWmvDec->hMFCWmvHandle.pDecOps;
    pInbufOps  = pWmvDec->hMFCWmvHandle.pInbufOps;
    pOutbufOps = pWmvDec->hMFCWmvHandle.pOutbufOps;

    pExynosInputPort->nPlaneCnt = MFC_DEFAULT_INPUT_BUFFER_PLANE;
    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        OMX_U32 nPlaneSize[MAX_BUFFER_PLANE] = {DEFAULT_MFC_INPUT_BUFFER_SIZE, 0, 0};
        Exynos_OSAL_SemaphoreCreate(&pExynosInputPort->codecSemID);
        Exynos_OSAL_QueueCreate(&pExynosInputPort->codecBufferQ, MAX_QUEUE_ELEMENTS);
        ret = Exynos_Allocate_CodecBuffers(pOMXComponent, INPUT_PORT_INDEX, MFC_INPUT_BUFFER_NUM_MAX, nPlaneSize);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++)
            Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, pVideoDec->pMFCDecInputBuffer[i]);
    } else if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    pExynosOutputPort->nPlaneCnt = MFC_DEFAULT_OUTPUT_BUFFER_PLANE;
    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        Exynos_OSAL_SemaphoreCreate(&pExynosOutputPort->codecSemID);
        Exynos_OSAL_QueueCreate(&pExynosOutputPort->codecBufferQ, MAX_QUEUE_ELEMENTS);
    } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    pWmvDec->bSourceStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pWmvDec->hSourceStartEvent);
    pWmvDec->bDestinationStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pWmvDec->hDestinationStartEvent);

    Exynos_OSAL_Memset(pExynosComponent->timeStamp, -19771003, sizeof(OMX_TICKS) * MAX_TIMESTAMP);
    Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
    pWmvDec->hMFCWmvHandle.indexTimestamp = 0;
    pWmvDec->hMFCWmvHandle.outputIndexTimestamp = 0;
    /* Default WMV codec format is set as VC1*/
    pWmvDec->hMFCWmvHandle.wmvFormat = WMV_FORMAT_VC1;

    pExynosComponent->getAllDelayBuffer = OMX_FALSE;

    Exynos_OSAL_QueueCreate(&pWmvDec->bypassBufferInfoQ, QUEUE_ELEMENTS);

#ifdef USE_CSC_HW
    csc_method = CSC_METHOD_HW;
#endif
    pVideoDec->csc_handle = csc_init(csc_method);
    if (pVideoDec->csc_handle == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    pVideoDec->csc_set_format = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Terminate */
OMX_ERRORTYPE Exynos_WmvDec_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT      *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_WMVDEC_HANDLE    *pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    OMX_PTR                hMFCHandle = pWmvDec->hMFCWmvHandle.hMFCHandle;

    ExynosVideoDecOps       *pDecOps    = pWmvDec->hMFCWmvHandle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pWmvDec->hMFCWmvHandle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pWmvDec->hMFCWmvHandle.pOutbufOps;

    int i, plane;

    FunctionIn();

    if (pVideoDec->csc_handle != NULL) {
        csc_deinit(pVideoDec->csc_handle);
        pVideoDec->csc_handle = NULL;
    }

    Exynos_OSAL_QueueTerminate(&pWmvDec->bypassBufferInfoQ);

    Exynos_OSAL_SignalTerminate(pWmvDec->hDestinationStartEvent);
    pWmvDec->hDestinationStartEvent = NULL;
    pWmvDec->bDestinationStart = OMX_FALSE;
    Exynos_OSAL_SignalTerminate(pWmvDec->hSourceStartEvent);
    pWmvDec->hSourceStartEvent = NULL;
    pWmvDec->bSourceStart = OMX_FALSE;

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        Exynos_Free_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX);
        Exynos_OSAL_QueueTerminate(&pExynosOutputPort->codecBufferQ);
        Exynos_OSAL_SemaphoreTerminate(pExynosOutputPort->codecSemID);
    } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        Exynos_Free_CodecBuffers(pOMXComponent, INPUT_PORT_INDEX);
        Exynos_OSAL_QueueTerminate(&pExynosInputPort->codecBufferQ);
        Exynos_OSAL_SemaphoreTerminate(pExynosInputPort->codecSemID);
    } else if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }
    WmvCodecClose(pWmvDec);

    Exynos_ResetAllPortConfig(pOMXComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_SrcIn(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMVDEC_HANDLE          *pWmvDec           = (EXYNOS_WMVDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pWmvDec->hMFCWmvHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    OMX_BUFFERHEADERTYPE tempBufferHeader;
    void *pPrivate = NULL;

    OMX_U32  oneFrameSize = pSrcInputData->dataLen;
    OMX_BOOL bStartCode   = OMX_FALSE;

    ExynosVideoDecOps       *pDecOps     = pWmvDec->hMFCWmvHandle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps   = pWmvDec->hMFCWmvHandle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps  = pWmvDec->hMFCWmvHandle.pOutbufOps;
    ExynosVideoErrorType     codecReturn = VIDEO_ERROR_NONE;

    int i;

    FunctionIn();

    if (pWmvDec->hMFCWmvHandle.bConfiguredMFCSrc == OMX_FALSE) {
        ret = WmvCodecSrcSetup(pOMXComponent, pSrcInputData);
        goto EXIT;
    }
    if (pWmvDec->hMFCWmvHandle.bConfiguredMFCDst == OMX_FALSE) {
        ret = WmvCodecDstSetup(pOMXComponent);
    }

    bStartCode = Check_Stream_PrefixCode(pSrcInputData->buffer.singlePlaneBuffer.dataBuffer, oneFrameSize, pWmvDec->hMFCWmvHandle.wmvFormat);
    if ((bStartCode == OMX_FALSE) &&
        ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) != OMX_BUFFERFLAG_EOS)) {
        if (pSrcInputData->allocSize < oneFrameSize+4) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Can't attach startcode due to lack of buffer space");
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
            goto EXIT;
        }

        bStartCode = Make_Stream_StartCode(pSrcInputData->buffer.singlePlaneBuffer.dataBuffer, &oneFrameSize, pWmvDec->hMFCWmvHandle.wmvFormat);
        if (bStartCode == OMX_FALSE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Fail to Make Stream Start Code");
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
            goto EXIT;
        }
    }

    if ((bStartCode == OMX_TRUE) || ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)){
        pExynosComponent->timeStamp[pWmvDec->hMFCWmvHandle.indexTimestamp] = pSrcInputData->timeStamp;
        pExynosComponent->nFlags[pWmvDec->hMFCWmvHandle.indexTimestamp] = pSrcInputData->nFlags;
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "input timestamp %lld us (%.2f secs), Tag: %d, nFlags: 0x%x", pSrcInputData->timeStamp, pSrcInputData->timeStamp / 1E6, pWmvDec->hMFCWmvHandle.indexTimestamp, pSrcInputData->nFlags);
        pDecOps->Set_FrameTag(hMFCHandle, pWmvDec->hMFCWmvHandle.indexTimestamp);
        pWmvDec->hMFCWmvHandle.indexTimestamp++;
        pWmvDec->hMFCWmvHandle.indexTimestamp %= MAX_TIMESTAMP;
#ifdef USE_QOS_CTRL
        if ((pVideoDec->bQosChanged == OMX_TRUE) &&
            (pDecOps->Set_QosRatio != NULL)) {
            pDecOps->Set_QosRatio(hMFCHandle, pVideoDec->nQosRatio);
            pVideoDec->bQosChanged = OMX_FALSE;
        }
#endif
        /* queue work for input buffer */
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "oneFrameSize: %d, bufferHeader: 0x%x, dataBuffer: 0x%x", oneFrameSize, pSrcInputData->bufferHeader, pSrcInputData->buffer.singlePlaneBuffer.dataBuffer);
        OMX_U32 nAllocLen[MAX_BUFFER_PLANE] = {pSrcInputData->bufferHeader->nAllocLen, 0, 0};

        if (pExynosInputPort->bufferProcessType == BUFFER_COPY) {
            tempBufferHeader.nFlags     = pSrcInputData->nFlags;
            tempBufferHeader.nTimeStamp = pSrcInputData->timeStamp;
            pPrivate = (void *)&tempBufferHeader;
        } else {
            pPrivate = (void *)pSrcInputData->bufferHeader;
        }
        codecReturn = pInbufOps->ExtensionEnqueue(hMFCHandle,
                                (unsigned char **)&pSrcInputData->buffer.singlePlaneBuffer.dataBuffer,
                                (unsigned char **)&pSrcInputData->buffer.singlePlaneBuffer.fd,
                                (unsigned int *)nAllocLen, (unsigned int *)&oneFrameSize,
                                pExynosInputPort->nPlaneCnt, pPrivate);
        if (codecReturn != VIDEO_ERROR_NONE) {
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s : %d", __FUNCTION__, __LINE__);
            goto EXIT;
        }
        WmvCodecStart(pOMXComponent, INPUT_PORT_INDEX);
        if (pWmvDec->bSourceStart == OMX_FALSE) {
            pWmvDec->bSourceStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pWmvDec->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
        if (pWmvDec->bDestinationStart == OMX_FALSE) {
            pWmvDec->bDestinationStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pWmvDec->hDestinationStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_SrcOut(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcOutputData)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMVDEC_HANDLE         *pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pWmvDec->hMFCWmvHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT     *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    ExynosVideoDecOps       *pDecOps    = pWmvDec->hMFCWmvHandle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pWmvDec->hMFCWmvHandle.pInbufOps;
    ExynosVideoBuffer       *pVideoBuffer;
    ExynosVideoBuffer        videoBuffer;

    FunctionIn();

    if (pInbufOps->ExtensionDequeue(hMFCHandle, &videoBuffer) == VIDEO_ERROR_NONE)
        pVideoBuffer = &videoBuffer;
    else
        pVideoBuffer = NULL;

    pSrcOutputData->dataLen       = 0;
    pSrcOutputData->usedDataLen   = 0;
    pSrcOutputData->remainDataLen = 0;
    pSrcOutputData->nFlags        = 0;
    pSrcOutputData->timeStamp     = 0;
    pSrcOutputData->bufferHeader  = NULL;

    if (pVideoBuffer == NULL) {
        pSrcOutputData->buffer.singlePlaneBuffer.dataBuffer = NULL;
        pSrcOutputData->allocSize  = 0;
        pSrcOutputData->pPrivate = NULL;
        pSrcOutputData->bufferHeader = NULL;
    } else {
        pSrcOutputData->buffer.singlePlaneBuffer.dataBuffer = pVideoBuffer->planes[0].addr;
        pSrcOutputData->buffer.singlePlaneBuffer.fd = pVideoBuffer->planes[0].fd;
        pSrcOutputData->allocSize  = pVideoBuffer->planes[0].allocSize;

        if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
            int i;
            for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++) {
                if (pSrcOutputData->buffer.singlePlaneBuffer.dataBuffer ==
                        pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[0]) {
                    pVideoDec->pMFCDecInputBuffer[i]->dataSize = 0;
                    pSrcOutputData->pPrivate = pVideoDec->pMFCDecInputBuffer[i];
                    break;
                }
            }

            if (i >= MFC_INPUT_BUFFER_NUM_MAX) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Can not find buffer");
                ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
                goto EXIT;
            }
        }

        /* For Share Buffer */
        if (pExynosInputPort->bufferProcessType == BUFFER_SHARE)
            pSrcOutputData->bufferHeader = (OMX_BUFFERHEADERTYPE*)pVideoBuffer->pPrivate;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_DstIn(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstInputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMVDEC_HANDLE            *pWmvDec            = (EXYNOS_WMVDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                            *hMFCHandle         = pWmvDec->hMFCWmvHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT             *pExynosOutputPort  = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoDecOps       *pDecOps     = pWmvDec->hMFCWmvHandle.pDecOps;
    ExynosVideoDecBufferOps *pOutbufOps  = pWmvDec->hMFCWmvHandle.pOutbufOps;
    ExynosVideoErrorType     codecReturn = VIDEO_ERROR_NONE;

    OMX_U32 dataLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    int i;

    FunctionIn();

    if (pDstInputData->buffer.multiPlaneBuffer.dataBuffer[0] == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to find input buffer");
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    for (i = 0; i < pExynosOutputPort->nPlaneCnt; i++) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "%s : %d => ADDR[i]: 0x%x", __FUNCTION__, __LINE__, i,
                                        pDstInputData->buffer.multiPlaneBuffer.dataBuffer[i]);
    }

    codecReturn = pOutbufOps->Enqueue(hMFCHandle, (unsigned char **)pDstInputData->buffer.multiPlaneBuffer.dataBuffer,
                     (unsigned int *)dataLen, pExynosOutputPort->nPlaneCnt, pDstInputData->bufferHeader);

    if (codecReturn != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s : %d", __FUNCTION__, __LINE__);
        ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
        goto EXIT;
    }
    WmvCodecStart(pOMXComponent, OUTPUT_PORT_INDEX);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_DstOut(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstOutputData)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMVDEC_HANDLE         *pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pWmvDec->hMFCWmvHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    ExynosVideoDecOps       *pDecOps    = pWmvDec->hMFCWmvHandle.pDecOps;
    ExynosVideoDecBufferOps *pOutbufOps = pWmvDec->hMFCWmvHandle.pOutbufOps;
    ExynosVideoBuffer       *pVideoBuffer = NULL;
    ExynosVideoFrameStatusType displayStatus = VIDEO_FRAME_STATUS_UNKNOWN;
    ExynosVideoGeometry *bufferGeometry;
    DECODE_CODEC_EXTRA_BUFFERINFO *pBufferInfo = NULL;
    OMX_S32 indexTimestamp = 0;
    int plane;

    FunctionIn();

    if (pWmvDec->bDestinationStart == OMX_FALSE) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    while (1) {
        pVideoBuffer = pOutbufOps->Dequeue(hMFCHandle);
        if (pVideoBuffer == (ExynosVideoBuffer *)VIDEO_ERROR_DQBUF_EIO) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "HW is not available");
            ret = OMX_ErrorHardware;
            goto EXIT;
        }

        if (pVideoBuffer == NULL) {
            ret = OMX_ErrorNone;
            goto EXIT;
        }
        displayStatus = pVideoBuffer->displayStatus;
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "displayStatus: 0x%x", displayStatus);

        if ((displayStatus == VIDEO_FRAME_STATUS_DISPLAY_DECODING) ||
            (displayStatus == VIDEO_FRAME_STATUS_DISPLAY_ONLY) ||
            (displayStatus == VIDEO_FRAME_STATUS_CHANGE_RESOL) ||
            (displayStatus == VIDEO_FRAME_STATUS_DECODING_FINISHED) ||
            (CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
            ret = OMX_ErrorNone;
            break;
        }
    }

    pWmvDec->hMFCWmvHandle.outputIndexTimestamp++;
    pWmvDec->hMFCWmvHandle.outputIndexTimestamp %= MAX_TIMESTAMP;

    pDstOutputData->allocSize = pDstOutputData->dataLen = 0;
    for (plane = 0; plane < pExynosOutputPort->nPlaneCnt; plane++) {
        pDstOutputData->buffer.multiPlaneBuffer.dataBuffer[plane] = pVideoBuffer->planes[plane].addr;
        pDstOutputData->buffer.multiPlaneBuffer.fd[plane] = pVideoBuffer->planes[plane].fd;
        pDstOutputData->allocSize += pVideoBuffer->planes[plane].allocSize;
        pDstOutputData->dataLen +=  pVideoBuffer->planes[plane].dataSize;
    }
    pDstOutputData->usedDataLen = 0;
    pDstOutputData->pPrivate = pVideoBuffer;
    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        int i = 0;
        pDstOutputData->pPrivate = NULL;
        for (i = 0; i < MFC_OUTPUT_BUFFER_NUM_MAX; i++) {
            if (pDstOutputData->buffer.multiPlaneBuffer.dataBuffer[0] ==
                pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[0]) {
                pDstOutputData->pPrivate = pVideoDec->pMFCDecOutputBuffer[i];
                break;
            }
        }

        if (pDstOutputData->pPrivate == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Can not find buffer");
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
            goto EXIT;
        }
    }

    /* For Share Buffer */
    pDstOutputData->bufferHeader = (OMX_BUFFERHEADERTYPE *)pVideoBuffer->pPrivate;

    pBufferInfo = (DECODE_CODEC_EXTRA_BUFFERINFO *)pDstOutputData->extInfo;
    bufferGeometry = &pWmvDec->hMFCWmvHandle.codecOutbufConf;
    pBufferInfo->imageWidth = bufferGeometry->nFrameWidth;
    pBufferInfo->imageHeight = bufferGeometry->nFrameHeight;
    switch (bufferGeometry->eColorFormat) {
    case VIDEO_COLORFORMAT_NV12:
        pBufferInfo->ColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
        break;
#ifdef USE_DUALDPB_MODE
    case VIDEO_COLORFORMAT_I420:
        pBufferInfo->ColorFormat = OMX_COLOR_FormatYUV420Planar;
        break;
    case VIDEO_COLORFORMAT_YV12:
        pBufferInfo->ColorFormat = OMX_SEC_COLOR_FormatYVU420Planar;
        break;
    case VIDEO_COLORFORMAT_NV21:
        pBufferInfo->ColorFormat = OMX_SEC_COLOR_FormatNV21Linear;
        break;
#endif
    case VIDEO_COLORFORMAT_NV12_TILED:
    default:
        pBufferInfo->ColorFormat = OMX_SEC_COLOR_FormatNV12Tiled;
        break;
    }

    indexTimestamp = pDecOps->Get_FrameTag(hMFCHandle);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "out indexTimestamp: %d", indexTimestamp);
    if ((indexTimestamp < 0) || (indexTimestamp >= MAX_TIMESTAMP)) {
        if ((pExynosComponent->checkTimeStamp.needSetStartTimeStamp != OMX_TRUE) &&
            (pExynosComponent->checkTimeStamp.needCheckStartTimeStamp != OMX_TRUE)) {
            if (indexTimestamp == INDEX_AFTER_EOS) {
                pDstOutputData->timeStamp = 0x00;
                pDstOutputData->nFlags = 0x00;
            } else {
                pDstOutputData->timeStamp = pExynosComponent->timeStamp[pWmvDec->hMFCWmvHandle.outputIndexTimestamp];
                pDstOutputData->nFlags = pExynosComponent->nFlags[pWmvDec->hMFCWmvHandle.outputIndexTimestamp];
                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "missing out indexTimestamp: %d", indexTimestamp);
            }
        } else {
            pDstOutputData->timeStamp = 0x00;
            pDstOutputData->nFlags = 0x00;
        }
    } else {
        /* For timestamp correction. if mfc support frametype detect */
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "disp_pic_frame_type: %d", pVideoBuffer->frameType);

        /* NEED TIMESTAMP REORDER */
        if (pVideoDec->bDTSMode == OMX_TRUE) {
            if ((pVideoBuffer->frameType == VIDEO_FRAME_I) ||
                ((pVideoBuffer->frameType == VIDEO_FRAME_OTHERS) &&
                    ((pExynosComponent->nFlags[indexTimestamp] & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) ||
                (pExynosComponent->checkTimeStamp.needCheckStartTimeStamp == OMX_TRUE))
                pWmvDec->hMFCWmvHandle.outputIndexTimestamp = indexTimestamp;
            else
                indexTimestamp = pWmvDec->hMFCWmvHandle.outputIndexTimestamp;
        }

        pDstOutputData->timeStamp = pExynosComponent->timeStamp[indexTimestamp];
        pDstOutputData->nFlags = pExynosComponent->nFlags[indexTimestamp];

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "timestamp %lld us (%.2f secs), indexTimestamp: %d, nFlags: 0x%x", pDstOutputData->timeStamp, pDstOutputData->timeStamp / 1E6, indexTimestamp, pDstOutputData->nFlags);
    }

    if ((displayStatus == VIDEO_FRAME_STATUS_CHANGE_RESOL) ||
        (displayStatus == VIDEO_FRAME_STATUS_DECODING_FINISHED) ||
        ((pDstOutputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "displayStatus:%d, nFlags0x%x", displayStatus, pDstOutputData->nFlags);
        pDstOutputData->remainDataLen = 0;

        if (((pDstOutputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) &&
            (pExynosComponent->bBehaviorEOS == OMX_TRUE)) {
            pDstOutputData->remainDataLen = bufferGeometry->nFrameWidth * bufferGeometry->nFrameHeight * 3 / 2;
            pExynosComponent->bBehaviorEOS = OMX_FALSE;
        }
    } else {
        pDstOutputData->remainDataLen = bufferGeometry->nFrameWidth * bufferGeometry->nFrameHeight * 3 / 2;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_srcInputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_WMVDEC_HANDLE    *pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    EXYNOS_OMX_BASEPORT      *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosInputPort)) || (!CHECK_PORT_POPULATED(pExynosInputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    ret = Exynos_WmvDec_SrcIn(pOMXComponent, pSrcInputData);
    if ((ret != OMX_ErrorNone) && (ret != OMX_ErrorInputDataDecodeYet)) {
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_srcOutputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcOutputData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_WMVDEC_HANDLE    *pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    EXYNOS_OMX_BASEPORT     *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosInputPort)) || (!CHECK_PORT_POPULATED(pExynosInputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX)) {
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }
    if ((pWmvDec->bSourceStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pExynosInputPort))) {
        Exynos_OSAL_SignalWait(pWmvDec->hSourceStartEvent, DEF_MAX_WAIT_TIME);
        Exynos_OSAL_SignalReset(pWmvDec->hSourceStartEvent);
    }

    ret = Exynos_WmvDec_SrcOut(pOMXComponent, pSrcOutputData);
    if ((ret != OMX_ErrorNone) &&
        (pExynosComponent->currentState == OMX_StateExecuting)) {
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_dstInputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstInputData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_WMVDEC_HANDLE    *pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    EXYNOS_OMX_BASEPORT      *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosOutputPort)) || (!CHECK_PORT_POPULATED(pExynosOutputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        if ((pWmvDec->bDestinationStart == OMX_FALSE) &&
           (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
            Exynos_OSAL_SignalWait(pWmvDec->hDestinationStartEvent, DEF_MAX_WAIT_TIME);
            Exynos_OSAL_SignalReset(pWmvDec->hDestinationStartEvent);
        }
        if (Exynos_OSAL_GetElemNum(&pWmvDec->bypassBufferInfoQ) > 0) {
            BYPASS_BUFFER_INFO *pBufferInfo = (BYPASS_BUFFER_INFO *)Exynos_OSAL_Dequeue(&pWmvDec->bypassBufferInfoQ);
            if (pBufferInfo == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }

            pDstInputData->bufferHeader->nFlags     = pBufferInfo->nFlags;
            pDstInputData->bufferHeader->nTimeStamp = pBufferInfo->timeStamp;
            Exynos_OMX_OutputBufferReturn(pOMXComponent, pDstInputData->bufferHeader);
            Exynos_OSAL_Free(pBufferInfo);

            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }
    if (pWmvDec->hMFCWmvHandle.bConfiguredMFCDst == OMX_TRUE) {
        ret = Exynos_WmvDec_DstIn(pOMXComponent, pDstInputData);
        if (ret != OMX_ErrorNone) {
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_WmvDec_dstOutputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstOutputData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_WMVDEC_HANDLE    *pWmvDec = (EXYNOS_WMVDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    EXYNOS_OMX_BASEPORT     *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosOutputPort)) || (!CHECK_PORT_POPULATED(pExynosOutputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        if ((pWmvDec->bDestinationStart == OMX_FALSE) &&
           (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
            Exynos_OSAL_SignalWait(pWmvDec->hDestinationStartEvent, DEF_MAX_WAIT_TIME);
            Exynos_OSAL_SignalReset(pWmvDec->hDestinationStartEvent);
        }
        if (Exynos_OSAL_GetElemNum(&pWmvDec->bypassBufferInfoQ) > 0) {
            EXYNOS_OMX_DATABUFFER *dstOutputUseBuffer   = &pExynosOutputPort->way.port2WayDataBuffer.outputDataBuffer;
            OMX_BUFFERHEADERTYPE  *pOMXBuffer           = NULL;
            BYPASS_BUFFER_INFO    *pBufferInfo          = NULL;

            if (dstOutputUseBuffer->dataValid == OMX_FALSE) {
                pOMXBuffer = Exynos_OutputBufferGetQueue_Direct(pExynosComponent);
                if (pOMXBuffer == NULL) {
                    ret = OMX_ErrorUndefined;
                    goto EXIT;
                }
            } else {
                pOMXBuffer = dstOutputUseBuffer->bufferHeader;
            }

            pBufferInfo = Exynos_OSAL_Dequeue(&pWmvDec->bypassBufferInfoQ);
            if (pBufferInfo == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }

            pOMXBuffer->nFlags      = pBufferInfo->nFlags;
            pOMXBuffer->nTimeStamp  = pBufferInfo->timeStamp;
            Exynos_OMX_OutputBufferReturn(pOMXComponent, pOMXBuffer);
            Exynos_OSAL_Free(pBufferInfo);

            dstOutputUseBuffer->dataValid = OMX_FALSE;

            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }
    ret = Exynos_WmvDec_DstOut(pOMXComponent, pDstOutputData);
    if ((ret != OMX_ErrorNone) &&
        (pExynosComponent->currentState == OMX_StateExecuting)) {
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OSCL_EXPORT_REF OMX_ERRORTYPE Exynos_OMX_ComponentInit(
    OMX_HANDLETYPE  hComponent,
    OMX_STRING      componentName)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_WMVDEC_HANDLE            *pWmvDec            = NULL;
    OMX_S32                          wmvFormat          = WMV_FORMAT_UNKNOWN;
    int i = 0;

    FunctionIn();

    if ((hComponent == NULL) || (componentName == NULL)) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }
    if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_WMV_DEC, componentName) != 0) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorBadParameter, componentName:%s, Line:%d", componentName, __LINE__);
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_VideoDecodeComponentInit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pExynosComponent->codecType = HW_VIDEO_DEC_CODEC;

    pExynosComponent->componentName = (OMX_STRING)Exynos_OSAL_Malloc(MAX_OMX_COMPONENT_NAME_SIZE);
    if (pExynosComponent->componentName == NULL) {
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosComponent->componentName, 0, MAX_OMX_COMPONENT_NAME_SIZE);

    pWmvDec = Exynos_OSAL_Malloc(sizeof(EXYNOS_WMVDEC_HANDLE));
    if (pWmvDec == NULL) {
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pWmvDec, 0, sizeof(EXYNOS_WMVDEC_HANDLE));
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    pVideoDec->hCodecHandle = (OMX_HANDLETYPE)pWmvDec;
    pWmvDec->hMFCWmvHandle.wmvFormat = wmvFormat;

    Exynos_OSAL_Strcpy(pExynosComponent->componentName, EXYNOS_OMX_COMPONENT_WMV_DEC);

    /* Set componentVersion */
    pExynosComponent->componentVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pExynosComponent->componentVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pExynosComponent->componentVersion.s.nRevision     = REVISION_NUMBER;
    pExynosComponent->componentVersion.s.nStep         = STEP_NUMBER;
    /* Set specVersion */
    pExynosComponent->specVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pExynosComponent->specVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pExynosComponent->specVersion.s.nRevision     = REVISION_NUMBER;
    pExynosComponent->specVersion.s.nStep         = STEP_NUMBER;

    /* Input port */
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight= DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "video/wmv");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    //pExynosPort->bufferProcessType = BUFFER_SHARE;
    pExynosPort->bufferProcessType = BUFFER_COPY;
    pExynosPort->portWayType = WAY2_PORT;

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight= DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_COPY | BUFFER_ANBSHARE;
    pExynosPort->portWayType = WAY2_PORT;

    for(i = 0; i < ALL_PORT_NUM; i++) {
        INIT_SET_SIZE_VERSION(&pWmvDec->WmvComponent[i], OMX_VIDEO_PARAM_WMVTYPE);
        pWmvDec->WmvComponent[i].nPortIndex = i;
        pWmvDec->WmvComponent[i].eFormat    = OMX_VIDEO_WMVFormat9;
    }

    pOMXComponent->GetParameter      = &Exynos_WmvDec_GetParameter;
    pOMXComponent->SetParameter      = &Exynos_WmvDec_SetParameter;
    pOMXComponent->GetConfig         = &Exynos_WmvDec_GetConfig;
    pOMXComponent->SetConfig         = &Exynos_WmvDec_SetConfig;
    pOMXComponent->GetExtensionIndex = &Exynos_WmvDec_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum = &Exynos_WmvDec_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit   = &Exynos_OMX_ComponentDeinit;

    pExynosComponent->exynos_codec_componentInit      = &Exynos_WmvDec_Init;
    pExynosComponent->exynos_codec_componentTerminate = &Exynos_WmvDec_Terminate;

    pVideoDec->exynos_codec_srcInputProcess  = &Exynos_WmvDec_srcInputBufferProcess;
    pVideoDec->exynos_codec_srcOutputProcess = &Exynos_WmvDec_srcOutputBufferProcess;
    pVideoDec->exynos_codec_dstInputProcess  = &Exynos_WmvDec_dstInputBufferProcess;
    pVideoDec->exynos_codec_dstOutputProcess = &Exynos_WmvDec_dstOutputBufferProcess;

    pVideoDec->exynos_codec_start         = &WmvCodecStart;
    pVideoDec->exynos_codec_stop          = &WmvCodecStop;
    pVideoDec->exynos_codec_bufferProcessRun = &WmvCodecOutputBufferProcessRun;
    pVideoDec->exynos_codec_enqueueAllBuffer = &WmvCodecEnQueueAllBuffer;

    pVideoDec->exynos_checkInputFrame                 = &Check_Wmv_Frame;
    pVideoDec->exynos_codec_getCodecInputPrivateData  = &GetCodecInputPrivateData;
    pVideoDec->exynos_codec_getCodecOutputPrivateData = &GetCodecOutputPrivateData;

    pVideoDec->hSharedMemory = Exynos_OSAL_SharedMemory_Open();
    if (pVideoDec->hSharedMemory == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        Exynos_OSAL_Free(pWmvDec);
        pWmvDec = ((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle = NULL;
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pExynosComponent->currentState = OMX_StateLoaded;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ComponentDeinit(
    OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    EXYNOS_WMVDEC_HANDLE        *pWmvDec            = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

    Exynos_OSAL_SharedMemory_Close(pVideoDec->hSharedMemory);

    Exynos_OSAL_Free(pExynosComponent->componentName);
    pExynosComponent->componentName = NULL;

    pWmvDec = (EXYNOS_WMVDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pWmvDec != NULL) {
        Exynos_OSAL_Free(pWmvDec);
        pWmvDec = pVideoDec->hCodecHandle = NULL;
    }

    ret = Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}
