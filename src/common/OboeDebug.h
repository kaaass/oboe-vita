/*
 * Copyright (C) 2015 The Android Open Source Project
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
 *
 */

#ifndef OBOE_DEBUG_H
#define OBOE_DEBUG_H

#include "log.h"

#ifndef MODULE_NAME
#define MODULE_NAME  "OboeAudio"
#endif

// when OBOE_ENABLE_LOGGING enabled, you should implement your own __android_log_print

#if defined(OBOE_ENABLE_LOGGING) && OBOE_LOG_LEVEL >= 0
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL, MODULE_NAME, __VA_ARGS__)
#else
#define LOGF(...)
#endif

#if defined(OBOE_ENABLE_LOGGING) && OBOE_LOG_LEVEL >= 1
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, MODULE_NAME, __VA_ARGS__)
#else
#define LOGE(...)
#endif

#if defined(OBOE_ENABLE_LOGGING) && OBOE_LOG_LEVEL >= 2
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, MODULE_NAME, __VA_ARGS__)
#else
#define LOGW(...)
#endif

#if defined(OBOE_ENABLE_LOGGING) && OBOE_LOG_LEVEL >= 3
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, MODULE_NAME, __VA_ARGS__)
#else
#define LOGI(...)
#endif

#if defined(OBOE_ENABLE_LOGGING) && OBOE_LOG_LEVEL >= 4
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, MODULE_NAME, __VA_ARGS__)
#else
#define LOGD(...)
#endif

#if defined(OBOE_ENABLE_LOGGING) && OBOE_LOG_LEVEL >= 5
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, MODULE_NAME, __VA_ARGS__)
#else
#define LOGV(...)
#endif

#endif //OBOE_DEBUG_H
