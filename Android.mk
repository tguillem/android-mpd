LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/Makefile

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/src \
	$(GLIB_PATH) \
	$(LIBAV_PATH) \
	$(LIBAV_PATH)/Android_config \
	$(YAJL_PATH) \
	$(CURL_PATH)/include \
	$(ANDROID_LIBS_PATH)/headers/sqlite

LOCAL_CFLAGS := $(src_mpd_CFLAGS) \
	$(src_mpd_CPPFLAGS) \
	-std=gnu99 -DSYSTEM_CONFIG_FILE_LOCATION="\"/data/data/be.deadba.ampd/files/mpd.conf\""

LOCAL_SRC_FILES := $(filter %.c %.cxx $(src_mpd_SOURCES))

LOCAL_MODULE := mpd

SOLIBS := $(filter %.so $(src_mpd_LDADD))
LDLIBS := $(patsubst %.so,,$(SOLIBS))

LOCAL_SHARED_LIBRARIES := $(LDLIBS)

LOCAL_LDLIBS := -L$(LOCAL_PATH)/../android-libs/$(TARGET_ARCH)
LOCAL_LDLIBS += $(patsubst lib%,-l%,$(LDLIBS))

include $(BUILD_EXECUTABLE)
