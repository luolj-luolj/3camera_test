
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

#ifndef __CAMERA_ENGINE_H__
#define __CAMERA_ENGINE_H__
#include <jni.h>
#include <android/native_activity.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <thread>

#include "camera_manager.h"
#include "image_reader.h"

/**
 * CameraAppEngine
 */
class CameraAppEngine {
 public:
  explicit CameraAppEngine(char* id, int fps, int AE, int AF,int S);
  ~CameraAppEngine();

  // Manage NDKCamera Object
  void CreateCameraSession();
  void StartPreview(bool start);
  const ImageFormat& GetCompatibleCameraRes() const;


 private:
  NDKCamera* camera_;
  ImageFormat compatibleCameraRes_;
  ImageReader* reader_;
  int camId;
  int s;
};
#endif  // __CAMERA_ENGINE_H__
