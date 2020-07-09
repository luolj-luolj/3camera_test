LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES:= \
    libmediandk \
    libandroid \
    libcamera2ndk \
    libcutils \
    libutils \
    liblog \
    libbinder \
    libdl \
    libm

LOCAL_CFLAGS += -Wall -Wextra -std=c++11 -frtti

LOCAL_SRC_FILES:= \
    main.cpp \
    camera_manager.cpp \
    camera_engine.cpp \
    camera_listeners.cpp \
    image_reader.cpp \
    camera_utils.cpp \
    tof_control.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= 3camera_test

LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_EXECUTABLES)

include $(BUILD_EXECUTABLE)
