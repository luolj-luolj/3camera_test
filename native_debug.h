#include <android/log.h>

#define LOG_TAG "3CAMERA-TEST"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ASSERT(cond, fmt, ...)                                \
  if (!(cond)) {                                              \
    __android_log_assert(#cond, LOG_TAG, fmt, ##__VA_ARGS__); \
  }

/* error codes */
enum {
    STATUS_SUCCESS,
    STATUS_ERROR,
    STATUS_INVALID_LENGTH,
    STATUS_NO_MEMORY,
    STATUS_NULL_POINTER,
    STATUS_CLIENT_ERROR,
};
