/**
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

/** Description
 *   Demonstrate NDK Camera interface added to android-24
 */

#include <cstdio>
#include <cstring>
#include "camera_engine.h"
#include "native_debug.h"

#define rgbWidth  1920
#define rgbHeight 1080

#define fishWidth  1280*2
#define fishHeight 400*2

#define tofWidth  640 //224
#define tofHeight 480 //173*10

CameraAppEngine::CameraAppEngine(char* id, int fps, int AE, int AF, int S)
    : camera_(nullptr),
     s(S),
     reader_(nullptr) {
    camera_ = new NDKCamera(id, fps, AE, AF);
    camId = atoi(id);
    LOGI("camId:%d camera_=%p, fps=%d, AE=%d, AF=%d, S=%d\n", camId, camera_, fps, AE, AF,S);
    if (camera_ == nullptr)
        LOGE("create NDKCamera failed\n");
    camera_->MatchCaptureSizeRequest(0, 0, &compatibleCameraRes_);
}

CameraAppEngine::~CameraAppEngine() {
    LOGI("~CameraAppEngine() for %d\n", camId);
    if (camera_) {
      delete camera_;
      camera_ = nullptr;
    }

    if (reader_) {
      delete reader_;
      reader_ = nullptr;
    }

}

/**
 * Create a capture session with given Java Surface Object
 * @param surface a {@link Surface} object.
 */
void CameraAppEngine::CreateCameraSession() {
    ImageFormat view{0, 0, 0};
    if (camId == 0)
    {
       view.width = rgbWidth;
       view.height = rgbHeight;
    } else if (camId == 1) {
       view.width = fishWidth;
       view.height = fishHeight;
    } else if (camId == 2) {
       view.width = tofWidth;
       view.height = tofHeight;
    }

    if (camId == 2) {
        reader_ = new ImageReader(&view, AIMAGE_FORMAT_RAW_PRIVATE, camId,s);
        reader_->SetPresentRotation(1);
        camera_->CreateSession(reader_->GetNativeWindow());
    } else {
        if (camId == 0)
            reader_ = new ImageReader(&view, AIMAGE_FORMAT_YUV_420_888, camId,s);
        else if(camId == 1)
            reader_ = new ImageReader(&view, AIMAGE_FORMAT_YUV_420_888, camId,s);
        reader_->SetPresentRotation(1);
        camera_->CreateSession(reader_->GetNativeWindow());
    }
    LOGI("CreateCameraSession camId = %d, reader_=%p\n", camId, reader_);
}

/**
 *
 * @return saved camera preview resolution for this session
 */
const ImageFormat& CameraAppEngine::GetCompatibleCameraRes() const {
    return compatibleCameraRes_;
}

/**
 *
 * @param start is true to start preview, false to stop preview
 * @return  true if preview started, false when error happened
 */
void CameraAppEngine::StartPreview(bool start) { camera_->StartPreview(start); }
