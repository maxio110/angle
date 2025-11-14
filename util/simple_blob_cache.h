//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// simple_blob_cache.h:
//   Small test-only helper to enable a simple disk-backed blob cache for traces/tests.

#ifndef ANGLE_UTIL_SIMPLE_BLOB_CACHE_H_
#define ANGLE_UTIL_SIMPLE_BLOB_CACHE_H_

#include <EGL/egl.h>

// Initialize the simple blob cache if a path has been set.
void InitSimpleBlobCache();

// Set the on-disk file path for the blob cache.
// If unset or set to an empty string, the cache remains disabled.
void SetSimpleBlobCachePath(const char *path);

#endif  // ANGLE_UTIL_SIMPLE_BLOB_CACHE_H_
