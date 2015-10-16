LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(BOARD_USE_DUALDPB_MODE), true)
LOCAL_CFLAGS += -DUSE_DUALDPB_MODE
endif

ifeq ($(BOARD_USE_VP8ENC_SUPPORT), true)
LOCAL_CFLAGS += -DUSE_VP8ENC_SUPPORT
endif

ifeq ($(TARGET_SOC), exynos5430)
	LOCAL_CFLAGS += -DSOC_EXYNOS5430
endif

ifeq ($(TARGET_SOC), exynos5433)
	LOCAL_CFLAGS += -DSOC_EXYNOS5430
endif


LOCAL_SRC_FILES := \
	dec/ExynosVideoDecoder.c \
	enc/ExynosVideoEncoder.c

LOCAL_C_INCLUDES := \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \
	$(LOCAL_PATH)/include \
	$(TOP)/hardware/samsung_slsi/exynos/include

LOCAL_ADDITIONAL_DEPENDENCIES += \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

ifeq ($(BOARD_USE_KHRONOS_OMX_HEADER), true)
LOCAL_C_INCLUDES += $(TOP)/hardware/samsung_slsi/openmax/include/khronos
else
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/include/media/openmax
endif

LOCAL_MODULE := libExynosVideoApi
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm

include $(BUILD_STATIC_LIBRARY)
