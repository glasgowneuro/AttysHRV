/**
 * Copyright (C) 2022 Facebook Technologies, LLC and its affiliates.
 * Copyright (C) 2022 Bernd Porr, <bernd@glasgowneuro.tech>
 * GNU GENERAL PUBLIC LICENSE, Version 3, 29 June 2007
 */

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID 1 \

#include <openxr/openxr.h>
#include <openxr/openxr_oculus.h>
#include <openxr/openxr_oculus_helpers.h>
#include <openxr/openxr_platform.h>
