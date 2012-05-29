LOCAL_PATH := $(my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := uaputl.exe
LOCAL_SRC_FILES := uapcmd.c uaputl.c uaphostcmd.c
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
