# build test exe
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= scrulltest.c

LOCAL_SHARED_LIBRARIES := libcutils libutils
LOCAL_C_INCLUDES := /mnt/fileroot/sky.zhou/workspace/linux-ldd-samples/ch3/
LOCAL_MODULE:= scrulltest

include $(BUILD_EXECUTABLE)
