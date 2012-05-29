LOCAL_PATH := $(my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := mlanevent.exe
LOCAL_SRC_FILES := mlanevent.c
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
