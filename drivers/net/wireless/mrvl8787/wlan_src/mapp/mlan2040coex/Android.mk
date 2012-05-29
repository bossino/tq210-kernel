LOCAL_PATH := $(my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := mlan2040coex
LOCAL_SRC_FILES := mlan2040coex.c mlan2040misc.c
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
