/*
 * Copyright (C) 2017 The Android Open Source Project
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
#include <utility>
#include <queue>
#include <unistd.h>
#include <cinttypes>
#include <cstring>
#include <camera/NdkCameraManager.h>
#include <media/NdkImage.h>
#include "camera_manager.h"
#include "native_debug.h"
#include "camera_utils.h"

/**
 * Range of Camera Exposure Time:
 *     Camera's capability range have a very long range which may be disturbing
 *     on camera. For this sample purpose, clamp to a range showing visible
 *     video on preview: 100000ns ~ 250000000ns
 */
static const uint64_t kMinExposureTime = static_cast<uint64_t>(1000000);
static const uint64_t kMaxExposureTime = static_cast<uint64_t>(250000000);


void NDKCamera::getstatus(){
    ACameraMetadata* metadataObj;
 static  ACameraMetadata_const_entry val = {
       0,
   };
    CALL_MGR(getCameraCharacteristics(cameraMgr_, activeCameraId_.c_str(),
                                    &metadataObj));
    camera_status_t status = ACameraMetadata_getConstEntry(
       metadataObj, ACAMERA_SENSOR_INFO_EXPOSURE_TIME_RANGE, &val);
    if (status == ACAMERA_OK) {
       exposureRange_.min_ = val.data.i64[0];
    if (exposureRange_.min_ < kMinExposureTime) {
       exposureRange_.min_ = kMinExposureTime;
      }
     exposureRange_.max_ = val.data.i64[1];
    if (exposureRange_.max_ > kMaxExposureTime) {
       exposureRange_.max_ = kMaxExposureTime;
      }
      exposureTime_ = exposureRange_.value(2);
    } else {
     LOGW("Unsupported ACAMERA_SENSOR_INFO_EXPOSURE_TIME_RANGE");
     exposureRange_.min_ = exposureRange_.max_ = 0l;
     exposureTime_ = 0l;
   } 
   status = ACameraMetadata_getConstEntry(
       metadataObj, ACAMERA_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, &val);
   if (status == ACAMERA_OK) {
     // show the available target fps ranges
     LOGI("FPS_RANGES is %d~%d %d~%d %d~%d",
         val.data.i32[0],val.data.i32[1],
         val.data.i32[2],val.data.i32[3],
         val.data.i32[4],val.data.i32[5]);
   } else {
     LOGW("Unsupported ACAMERA_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES");
   }
 
   status = ACameraMetadata_getConstEntry(
       metadataObj, ACAMERA_SENSOR_INFO_SENSITIVITY_RANGE, &val);
 
   if (status == ACAMERA_OK){
     sensitivityRange_.min_ = val.data.i32[0];
     sensitivityRange_.max_ = val.data.i32[1];
 
     sensitivity_ = sensitivityRange_.value(2);
   } else {
     LOGW("failed for ACAMERA_SENSOR_INFO_SENSITIVITY_RANGE");
     sensitivityRange_.min_ = sensitivityRange_.max_ = 0;
     sensitivity_ = 0;
   } 
   status = ACameraMetadata_getConstEntry(
       metadataObj, ACAMERA_CONTROL_AE_AVAILABLE_MODES, &val);
   if(status == ACAMERA_OK)
     LOGI("AE value is %d",val.data.u8[0]);
   else 
     LOGI("failed read AE value");
   status = ACameraMetadata_getConstEntry(
       metadataObj, ACAMERA_CONTROL_AF_AVAILABLE_MODES, &val);
   if(status == ACAMERA_OK)
     LOGI("AF value is %d",val.data.u8[0]);
   else 
     LOGI("failed read AF value");

}


NDKCamera::NDKCamera(char* id, int fps, int AE, int AF)
    : cameraMgr_(nullptr),
      activeCameraId_(""),
      outputContainer_(nullptr),
      captureSessionState_(CaptureSessionState::MAX_STATE),
      cameraFacing_(ACAMERA_LENS_FACING_BACK),
      cameraOrientation_(0),
      exposureTime_(static_cast<int64_t>(0)) {
  valid_ = false;
  requests_.resize(CAPTURE_REQUEST_COUNT);
  memset(requests_.data(), 0, requests_.size() * sizeof(requests_[0]));
  cameras_.clear();
  cameraMgr_ = ACameraManager_create();
  ASSERT(cameraMgr_, "Failed to create cameraManager");

  // Pick up a back-facing camera to preview
  //EnumerateCamera();
  ndkCamId = atoi(id);
  for (int i = 0; i < 6; i++) {
      ndkFpsRange[i] = fps;
  }
  activeCameraId_ = id;
  ndkAE =(uint8_t)AE;
  ndkAF =(uint8_t)AF;
  LOGI("create NDKCamera Id = %s\n", activeCameraId_.c_str());
  ASSERT(activeCameraId_.size(), "Unknown ActiveCameraIdx");

  // Create back facing camera device
  CALL_MGR(openCamera(cameraMgr_, activeCameraId_.c_str(), GetDeviceListener(),
                      &cameras_[activeCameraId_].device_));

  CALL_MGR(registerAvailabilityCallback(cameraMgr_, GetManagerListener()));

  // Initialize camera controls(exposure time and sensitivity), pick
  // up value of 2% * range + min as starting value (just a number, no magic)
  getstatus();
  valid_ = true;
}

/**
 * A helper class to assist image size comparison, by comparing the absolute
 * size
 * regardless of the portrait or landscape mode.
 */
class DisplayDimension {
 public:
  DisplayDimension(int32_t w, int32_t h) : w_(w), h_(h), portrait_(false) {
    if (h > w) {
      // make it landscape
      w_ = h;
      h_ = w;
      portrait_ = true;
    }
  }
  DisplayDimension(const DisplayDimension& other) {
    w_ = other.w_;
    h_ = other.h_;
    portrait_ = other.portrait_;
  }

  DisplayDimension(void) {
    w_ = 0;
    h_ = 0;
    portrait_ = false;
  }
  DisplayDimension& operator=(const DisplayDimension& other) {
    w_ = other.w_;
    h_ = other.h_;
    portrait_ = other.portrait_;

    return (*this);
  }

  bool IsSameRatio(DisplayDimension& other) {
    return (w_ * other.h_ == h_ * other.w_);
  }
  bool operator>(DisplayDimension& other) {
    return (w_ >= other.w_ & h_ >= other.h_);
  }
  bool operator==(DisplayDimension& other) {
    return (w_ == other.w_ && h_ == other.h_ && portrait_ == other.portrait_);
  }
  DisplayDimension operator-(DisplayDimension& other) {
    DisplayDimension delta(w_ - other.w_, h_ - other.h_);
    return delta;
  }
  void Flip(void) { portrait_ = !portrait_; }
  bool IsPortrait(void) { return portrait_; }
  int32_t width(void) { return w_; }
  int32_t height(void) { return h_; }
  int32_t org_width(void) { return (portrait_ ? h_ : w_); }
  int32_t org_height(void) { return (portrait_ ? w_ : h_); }

 private:
  int32_t w_, h_;
  bool portrait_;
};

/**
 * Find a compatible camera modes:
 *    1) the same aspect ration as the native display window, which should be a
 *       rotated version of the physical device
 *    2) the smallest resolution in the camera mode list
 * This is to minimize the later color space conversion workload.
 */
bool NDKCamera::MatchCaptureSizeRequest(int32_t requestWidth,
                                        int32_t requestHeight,
                                        ImageFormat* view) {
  return MatchCaptureSizeRequest(requestWidth, requestHeight, view, nullptr);
}


bool NDKCamera::MatchCaptureSizeRequest(int32_t requestWidth,
                                        int32_t requestHeight,
                                        ImageFormat* resView,
                                        ImageFormat* resCap) {
  DisplayDimension disp(requestWidth, requestHeight);
  if (cameraOrientation_ == 90 || cameraOrientation_ == 270) {
    disp.Flip();
  }
  ACameraMetadata* metadata;
  CALL_MGR(
      getCameraCharacteristics(cameraMgr_, activeCameraId_.c_str(), &metadata));
  ACameraMetadata_const_entry entry;
  CALL_METADATA(getConstEntry(
      metadata, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry));
  // format of the data: format, width, height, input?, type int32
  bool foundIt = false;
  DisplayDimension foundRes(4000, 4000);
  DisplayDimension maxJPG(0, 0);

  for (int i = 0; i < entry.count; i += 4) {
    int32_t input = entry.data.i32[i + 3];
    int32_t format = entry.data.i32[i + 0];
    if (input) continue;

    if (format == AIMAGE_FORMAT_YUV_420_888 || format == AIMAGE_FORMAT_JPEG) {
      DisplayDimension res(entry.data.i32[i + 1],
                           entry.data.i32[i + 2]);
      if (!disp.IsSameRatio(res)) continue;
      if (format == AIMAGE_FORMAT_YUV_420_888 && foundRes > res) {
        foundIt = true;
        foundRes = res;
      } else if (format == AIMAGE_FORMAT_JPEG && res > maxJPG) {
        maxJPG = res;
      }
    }
  }

  if (foundIt) {
    resView->width = foundRes.org_width();
    resView->height = foundRes.org_height();
    if (resCap) {
      resCap->width = maxJPG.org_width();
      resCap->height = maxJPG.org_height();
    }
  } else {
    LOGW("Did not find any compatible camera resolution, taking 640x480");
    if (disp.IsPortrait()) {
      resView->width = 480;
      resView->height = 640;
    } else {
      resView->width = 640;
      resView->height = 480;
    }
    if (resCap)
      *resCap = *resView;
  }
  resView->format = AIMAGE_FORMAT_YUV_420_888;
  if (resCap) resCap->format = AIMAGE_FORMAT_JPEG;
  return foundIt;
}

void NDKCamera::CreateSession(ANativeWindow* previewWindow,
                              ANativeWindow* jpgWindow, bool manualPreview,
                              int32_t imageRotation) {
  // Create output from this app's ANativeWindow, and add into output container
  requests_[PREVIEW_REQUEST_IDX].outputNativeWindow_ = previewWindow;
  requests_[PREVIEW_REQUEST_IDX].template_ = TEMPLATE_PREVIEW;
  requests_[JPG_CAPTURE_REQUEST_IDX].outputNativeWindow_ = jpgWindow;
  requests_[JPG_CAPTURE_REQUEST_IDX].template_ = TEMPLATE_STILL_CAPTURE;

  CALL_CONTAINER(create(&outputContainer_));
  for (auto& req : requests_) {
    if (!req.outputNativeWindow_) continue;

    ANativeWindow_acquire(req.outputNativeWindow_);
    CALL_OUTPUT(create(req.outputNativeWindow_, &req.sessionOutput_));
    CALL_CONTAINER(add(outputContainer_, req.sessionOutput_));
    CALL_TARGET(create(req.outputNativeWindow_, &req.target_));
    CALL_DEV(createCaptureRequest(cameras_[activeCameraId_].device_,
                                  req.template_, &req.request_));
    CALL_REQUEST(addTarget(req.request_, req.target_));
  }

  // Create a capture session for the given preview request
  captureSessionState_ = CaptureSessionState::READY;
  CALL_DEV(createCaptureSession(cameras_[activeCameraId_].device_,
                                outputContainer_, GetSessionListener(),
                                &captureSession_));

  LOGI("CreateSession camId = %d, captureSession_=%p\n",
      ndkCamId, captureSession_,captureSessionState_);
  if (jpgWindow) {
    ACaptureRequest_setEntry_i32(requests_[JPG_CAPTURE_REQUEST_IDX].request_,
                                 ACAMERA_JPEG_ORIENTATION, 1, &imageRotation);
  }

  // https://developer.android.google.cn/ndk/reference/group/camera.html?hl=zh-cn
  // Android Developers->NDK->reference->camera
  // tof fps setting using other API.
  if (ndkCamId != 2) {
      printf("ACAMERA_CONTROL_AE_TARGET_FPS_RANGE = %d,%d\n", ndkFpsRange[4],ndkFpsRange[5]);
      CALL_REQUEST(setEntry_i32(requests_[PREVIEW_REQUEST_IDX].request_,
                              ACAMERA_CONTROL_AE_TARGET_FPS_RANGE, 6, ndkFpsRange));
  }

  if (!manualPreview) {
    return;
  }
  /*
   * Only preview request is in manual mode, JPG is always in Auto mode
   * JPG capture mode could also be switch into manual mode and control
   * the capture parameters, this sample leaves JPG capture to be auto mode
   * (auto control has better effect than author's manual control)
   */
  uint8_t aeModeOff = ACAMERA_CONTROL_AE_MODE_OFF;
  uint8_t aeModeOn  = ACAMERA_CONTROL_AE_MODE_ON;
  CALL_REQUEST(setEntry_u8(requests_[PREVIEW_REQUEST_IDX].request_,
                           ACAMERA_CONTROL_AE_MODE, 1, &aeModeOn));
  CALL_REQUEST(setEntry_i32(requests_[PREVIEW_REQUEST_IDX].request_,
                            ACAMERA_SENSOR_SENSITIVITY, 1, &sensitivity_));
  CALL_REQUEST(setEntry_i64(requests_[PREVIEW_REQUEST_IDX].request_,
                            ACAMERA_SENSOR_EXPOSURE_TIME, 1, &exposureTime_));
}

void NDKCamera::CreateSession(ANativeWindow* previewWindow) {
  if (ndkCamId == 2) {
      CreateSession(previewWindow, nullptr, false, 0);
  } else {
      CreateSession(previewWindow, nullptr, false, 0);
  }
}

NDKCamera::~NDKCamera() {
  LOGI("~NDKCamera() for Cam %s ==1\n", activeCameraId_.c_str());
  valid_ = false;
  // stop session if it is on:
  if (captureSessionState_ == CaptureSessionState::ACTIVE) {
    ACameraCaptureSession_stopRepeating(captureSession_);
  }
  LOGI("~NDKCamera() for Cam %s ==2\n", activeCameraId_.c_str());
  
  ACameraCaptureSession_close(captureSession_);
  LOGI("~NDKCamera() for Cam %s ==3\n", activeCameraId_.c_str());

  for (auto& req : requests_) {
    if (!req.outputNativeWindow_) continue;

    CALL_REQUEST(removeTarget(req.request_, req.target_));
    ACaptureRequest_free(req.request_);
    ACameraOutputTarget_free(req.target_);

    CALL_CONTAINER(remove(outputContainer_, req.sessionOutput_));
    ACaptureSessionOutput_free(req.sessionOutput_);

    ANativeWindow_release(req.outputNativeWindow_);
  }
  requests_.resize(0);
  ACaptureSessionOutputContainer_free(outputContainer_);
  LOGI("~NDKCamera() for Cam %s ==4\n", activeCameraId_.c_str());

  /*for (auto& cam : cameras_) {
    if (cam.second.device_) {
      CALL_DEV(close(cam.second.device_));
    }
  }*/
  CALL_DEV(close(cameras_[activeCameraId_].device_));
  cameras_.clear();
  LOGI("~NDKCamera() for Cam %s ==5\n", activeCameraId_.c_str());
  if (cameraMgr_) {
    CALL_MGR(unregisterAvailabilityCallback(cameraMgr_, GetManagerListener()));
    ACameraManager_delete(cameraMgr_);
    cameraMgr_ = nullptr;
  }
  LOGI("~NDKCamera() for Cam %s ==6\n", activeCameraId_.c_str());
}

/**
 * EnumerateCamera()
 *     Loop through cameras on the system, pick up
 *     1) back facing one if available
 *     2) otherwise pick the first one reported to us
 */
void NDKCamera::EnumerateCamera() {
  ACameraIdList* cameraIds = nullptr;
  CALL_MGR(getCameraIdList(cameraMgr_, &cameraIds));

  for (int i = 0; i < cameraIds->numCameras; ++i) {
    const char* id = cameraIds->cameraIds[i];

    ACameraMetadata* metadataObj;
    CALL_MGR(getCameraCharacteristics(cameraMgr_, id, &metadataObj));

    int32_t count = 0;
    const uint32_t* tags = nullptr;
    ACameraMetadata_getAllTags(metadataObj, &count, &tags);
    for (int tagIdx = 0; tagIdx < count; ++tagIdx) {
      if (ACAMERA_LENS_FACING == tags[tagIdx]) {
        ACameraMetadata_const_entry lensInfo = {
            0,
        };
        CALL_METADATA(getConstEntry(metadataObj, tags[tagIdx], &lensInfo));
        CameraId cam(id);
        cam.facing_ = static_cast<acamera_metadata_enum_android_lens_facing_t>(
            lensInfo.data.u8[0]);
        cam.owner_ = false;
        cam.device_ = nullptr;
        cameras_[cam.id_] = cam;
        if (cam.facing_ == ACAMERA_LENS_FACING_FRONT) {
          activeCameraId_ = cam.id_;
        }
        break;
      }
    }
    ACameraMetadata_free(metadataObj);
  }

  ASSERT(cameras_.size(), "No Camera Available on the device");
  if (activeCameraId_.length() == 0) {
    // if no back facing camera found, pick up the first one to use...
    activeCameraId_ = cameras_.begin()->second.id_;
  }
  ACameraManager_deleteCameraIdList(cameraIds);
}

/**
 * GetSensorOrientation()
 *     Retrieve current sensor orientation regarding to the phone device
 * orientation
 *     SensorOrientation is NOT settable.
 */
bool NDKCamera::GetSensorOrientation(int32_t* facing, int32_t* angle) {
  if (!cameraMgr_) {
    return false;
  }

  ACameraMetadata* metadataObj;
  ACameraMetadata_const_entry face, orientation;
  CALL_MGR(getCameraCharacteristics(cameraMgr_, activeCameraId_.c_str(),
                                    &metadataObj));
  CALL_METADATA(getConstEntry(metadataObj, ACAMERA_LENS_FACING, &face));
  cameraFacing_ = static_cast<int32_t>(face.data.u8[0]);

  CALL_METADATA(
      getConstEntry(metadataObj, ACAMERA_SENSOR_ORIENTATION, &orientation));

  LOGI("====Current SENSOR_ORIENTATION: %8d", orientation.data.i32[0]);

  ACameraMetadata_free(metadataObj);
  cameraOrientation_ = orientation.data.i32[0];

  if (facing) *facing = cameraFacing_;
  if (angle) *angle = cameraOrientation_;
  return true;
}

/**
 * StartPreview()
 *   Toggle preview start/stop
 */
void NDKCamera::StartPreview(bool start) {
  if (start) {
    CALL_SESSION(setRepeatingRequest(captureSession_, nullptr, 1,
                                     &requests_[PREVIEW_REQUEST_IDX].request_,
                                     nullptr));
  } else if (!start && captureSessionState_ == CaptureSessionState::ACTIVE) {
    ACameraCaptureSession_stopRepeating(captureSession_);
  }
}

/**
 * Capture one jpg photo into
 *     /sdcard/DCIM/Camera
 * refer to WriteFile() for details
 */
bool NDKCamera::TakePhoto(void) {
  if (captureSessionState_ == CaptureSessionState::ACTIVE) {
    ACameraCaptureSession_stopRepeating(captureSession_);
  }

  CALL_SESSION(capture(captureSession_, GetCaptureCallback(), 1,
                       &requests_[JPG_CAPTURE_REQUEST_IDX].request_,
                       &requests_[JPG_CAPTURE_REQUEST_IDX].sessionSequenceId_));
  return true;
}

void NDKCamera::UpdateCameraRequestParameter(int32_t code, int64_t val) {
  ACaptureRequest* request = requests_[PREVIEW_REQUEST_IDX].request_;
  switch (code) {
    case ACAMERA_SENSOR_EXPOSURE_TIME:
      exposureTime_ = val;
      CALL_REQUEST(setEntry_i64(request, ACAMERA_SENSOR_EXPOSURE_TIME, 1,
                                &exposureTime_));
      break;

    case ACAMERA_SENSOR_SENSITIVITY:
      sensitivity_ = val;
      CALL_REQUEST(
          setEntry_i32(request, ACAMERA_SENSOR_SENSITIVITY, 1, &sensitivity_));
      break;
    default:
      ASSERT(false, "==ERROR==: error code for CameraParameterChange: %d",
             code);
      return;
  }

  uint8_t aeModeOff = ACAMERA_CONTROL_AE_MODE_OFF;
  uint8_t aeModeOn  = ACAMERA_CONTROL_AE_MODE_ON;
  CALL_REQUEST(setEntry_u8(request, ACAMERA_CONTROL_AE_MODE, 1, &aeModeOn));
  CALL_SESSION(
      setRepeatingRequest(captureSession_, nullptr, 1, &request,
                          &requests_[PREVIEW_REQUEST_IDX].sessionSequenceId_));
}

/**
 * Retrieve Camera Exposure adjustable range.
 *
 * @param min Camera minimium exposure time in nanoseconds
 * @param max Camera maximum exposure tiem in nanoseconds
 *
 * @return true  min and max are loaded with the camera's exposure values
 *         false camera has not initialized, no value available
 */
bool NDKCamera::GetExposureRange(int64_t* min, int64_t* max, int64_t* curVal) {
  if (!exposureTime_ || !min || !max || !curVal) {
    return false;
  }
  *min = exposureRange_.min_;
  *max = exposureRange_.max_;
  *curVal = exposureTime_;

  return true;
}

/**
 * Retrieve Camera sensitivity range.
 *
 * @param min Camera minimium sensitivity
 * @param max Camera maximum sensitivity
 *
 * @return true  min and max are loaded with the camera's sensitivity values
 *         false camera has not initialized, no value available
 */
bool NDKCamera::GetSensitivityRange(int64_t* min, int64_t* max,
                                    int64_t* curVal) {
  if (!sensitivity_ || !min || !max || !curVal) {
    return false;
  }
  *min = static_cast<int64_t>(sensitivityRange_.min_);
  *max = static_cast<int64_t>(sensitivityRange_.max_);
  *curVal = sensitivity_;
  return true;
}
