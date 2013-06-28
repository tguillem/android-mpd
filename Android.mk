LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/Android_SRC_FILES.mk

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/src \
	$(GLIB_PATH) \
	$(LIBAV_PATH) \
	$(LIBAV_PATH)/Android_config \
	$(YAJL_PATH) \
	$(CURL_PATH)/include \
	$(ANDROID_LIBS_PATH)/headers/sqlite

LOCAL_CFLAGS := \
	-std=gnu99 -DSYSTEM_CONFIG_FILE_LOCATION="\"/data/data/be.deadba.ampd/files/mpd.conf\""

LOCAL_MODULE := libmpd

LOCAL_SHARED_LIBRARIES := avutil avformat avcodec iconv glib gthread yajl curl

LOCAL_LDLIBS := -L$(LOCAL_PATH)/../android-libs/$(TARGET_ARCH)
LOCAL_LDLIBS += -lsqlite -lz -llog

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_ARM_NEON:= true
endif

include $(BUILD_SHARED_LIBRARY)
