//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// simple_blob_cache.cpp:
//   Small test-only helper to enable a simple disk-backed blob cache for traces/tests.
//
#pragma allow_unsafe_buffers

#include "simple_blob_cache.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>
#include <mutex>

#include "capture/trace_gles_loader_autogen.h"  // For glBlobCacheCallbacksANGLE proc
#include "capture/trace_egl_loader_autogen.h"   // For eglGetProcAddress

namespace
{
// Lazily-allocated singletons to avoid global constructors/destructors.
static std::map<std::string, std::vector<uint8_t>> &BlobCache()
{
    static auto *ptr = new std::map<std::string, std::vector<uint8_t>>();
    return *ptr;
}

static std::atomic<uint64_t> &BlobCacheHits()
{
    static auto *ptr = new std::atomic<uint64_t>(0);
    return *ptr;
}

static std::atomic<uint64_t> &BlobCacheMisses()
{
    static auto *ptr = new std::atomic<uint64_t>(0);
    return *ptr;
}

static std::atomic<uint64_t> &BlobCacheSets()
{
    static auto *ptr = new std::atomic<uint64_t>(0);
    return *ptr;
}

static bool &BlobCacheInitialized()
{
    static auto *ptr = new bool(false);
    return *ptr;
}

static std::mutex &BlobCacheInitMutex()
{
    static auto *m = new std::mutex();
    return *m;
}

// Configurable file path for the on-disk blob cache.
static std::string &BlobCacheFilePath()
{
    static auto *path = new std::string();
    return *path;
}

std::string GetBlobCacheFilePath()
{
    return BlobCacheFilePath();
}

std::string MakeBlobKey(const void *key, size_t keySize)
{
    return std::string(reinterpret_cast<const char *>(key), keySize);
}

void LoadBlobCacheFromFile()
{
    auto path = GetBlobCacheFilePath();
    if (path.empty())
        return;

    std::ifstream in(path, std::ios::binary);
    if (!in.good())
    {
        std::cout << "[BlobCache] Warning: cannot open for read: " << path << std::endl;
        return;
    }
    uint32_t magic = 0, version = 0, count = 0;
    in.read(reinterpret_cast<char *>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char *>(&version), sizeof(version));
    if (magic != 0x41424C43 /*ABLC*/ || version != 1)
        return;
    in.read(reinterpret_cast<char *>(&count), sizeof(count));
    for (uint32_t i = 0; i < count && in.good(); ++i)
    {
        uint32_t klen = 0, vlen = 0;
        in.read(reinterpret_cast<char *>(&klen), sizeof(klen));
        in.read(reinterpret_cast<char *>(&vlen), sizeof(vlen));
        if (!in.good())
            break;
        std::string key(klen, '\0');
        if (klen)
            in.read(&key[0], klen);
        std::vector<uint8_t> val;
        val.resize(vlen);
        if (vlen)
            in.read(reinterpret_cast<char *>(val.data()), vlen);
        BlobCache()[std::move(key)] = std::move(val);
    }
    std::cout << "[BlobCache] Loaded " << BlobCache().size() << " entries from: " << path
              << std::endl;
}

void SaveBlobCacheToFile()
{
    auto path = GetBlobCacheFilePath();
    if (path.empty())
        return;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.good())
    {
        std::cout << "[BlobCache] Warning: cannot open for write: " << path << std::endl;
        return;
    }
    uint32_t magic = 0x41424C43, version = 1;
    uint32_t count = static_cast<uint32_t>(BlobCache().size());
    out.write(reinterpret_cast<const char *>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char *>(&version), sizeof(version));
    out.write(reinterpret_cast<const char *>(&count), sizeof(count));
    for (const auto &kv : BlobCache())
    {
        uint32_t klen = static_cast<uint32_t>(kv.first.size());
        uint32_t vlen = static_cast<uint32_t>(kv.second.size());
        out.write(reinterpret_cast<const char *>(&klen), sizeof(klen));
        out.write(reinterpret_cast<const char *>(&vlen), sizeof(vlen));
        if (klen)
            out.write(kv.first.data(), klen);
        if (vlen)
            out.write(reinterpret_cast<const char *>(kv.second.data()), vlen);
    }
    std::cout << "[BlobCache] Saved " << BlobCache().size() << " entries to: " << path << std::endl;
}

void SaveBlobCacheAtExit()
{
    SaveBlobCacheToFile();
    std::cout << "[BlobCache] Stats: hits=" << BlobCacheHits().load()
              << ", misses=" << BlobCacheMisses().load() << ", sets=" << BlobCacheSets().load()
              << std::endl;
}

// GL_ANGLE_blob_cache callbacks
GLsizeiptr GL_APIENTRY GLBlobGet(const void *key,
                                        GLsizeiptr keySize,
                                        void *value,
                                        GLsizeiptr valueSize,
                                        const void *userParam)
{
    (void)userParam;
    auto it = BlobCache().find(MakeBlobKey(key, static_cast<size_t>(keySize)));
    if (it == BlobCache().end())
    {
        ++BlobCacheMisses();
        return 0;
    }
    const auto &blob = it->second;
    if (!value)
    {
        ++BlobCacheHits();
        return static_cast<GLsizeiptr>(blob.size());
    }
    GLsizeiptr n = static_cast<GLsizeiptr>(
        std::min<size_t>(blob.size(), static_cast<size_t>(valueSize)));
    if (n > 0)
        std::memcpy(value, blob.data(), static_cast<size_t>(n));
    ++BlobCacheHits();
    return n;
}

void GL_APIENTRY GLBlobSet(const void *key,
                                  GLsizeiptr keySize,
                                  const void *value,
                                  GLsizeiptr valueSize,
                                  const void *userParam)
{
    (void)userParam;
    if (!value || valueSize <= 0)
        return;
    auto &slot = BlobCache()[MakeBlobKey(key, static_cast<size_t>(keySize))];
    slot.assign(reinterpret_cast<const uint8_t *>(value),
                reinterpret_cast<const uint8_t *>(value) + static_cast<size_t>(valueSize));
    ++BlobCacheSets();
}
}  // anonymous namespace

void InitSimpleBlobCache()
{
    std::lock_guard<std::mutex> lock(BlobCacheInitMutex());

    if (const char *envPath = std::getenv("ANGLE_SIMPLE_BLOB_CACHE_PATH"))
    {
        BlobCacheFilePath() = envPath;
    }

    if (BlobCacheInitialized())
    {
        return;
    }

    auto path = GetBlobCacheFilePath();
    if (path.empty())
    {
        std::cout << "[BlobCache] No path configured; exit (no-op)" << std::endl;
        return;
    }

    LoadBlobCacheFromFile();
    std::cout << "[BlobCache] Load complete; entries=" << BlobCache().size() << std::endl;

    auto glBlobCacheCallbacksANGLEPtr =
        reinterpret_cast<PFNGLBLOBCACHECALLBACKSANGLEPROC>(
            eglGetProcAddress("glBlobCacheCallbacksANGLE"));
    if (glBlobCacheCallbacksANGLEPtr)
    {
        glBlobCacheCallbacksANGLEPtr(&GLBlobSet, &GLBlobGet, nullptr);
    }
    std::atexit(SaveBlobCacheAtExit);
    BlobCacheInitialized() = true;
}

// No EGL variant — tests can rely on GL_ANGLE_blob_cache for now.

void SetSimpleBlobCachePath(const char *path)
{
    if (!path)
    {
        BlobCacheFilePath().clear();
        unsetenv("ANGLE_SIMPLE_BLOB_CACHE_PATH");
        return;
    }
    BlobCacheFilePath() = std::string(path);
    setenv("ANGLE_SIMPLE_BLOB_CACHE_PATH", BlobCacheFilePath().c_str(), /*overwrite*/ 1);
}
