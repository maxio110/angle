# ANGLE Frame Capture: onEndFrame crash (Android, Fortnite)

- Date: 2025-11-06
- Area: Frame Capture (capture/FrameCapture.{h,cpp})
- Affected: Android, multi-threaded app (Unreal RHIThread), eglSwapBuffers

## Summary
- Symptom: SIGSEGV in `angle::FrameCaptureShared::onEndFrame(gl::Context*)` during `eglSwapBuffers`.
- Root cause: Data race on shared capture state (`mFrameCalls` and related fields) in `onEndFrame` when another thread concurrently appends capture calls. Iteration without locking led to vector corruption and a crash.
- Fix: Guard all reads/writes to `mFrameCalls` and dependent state inside `onEndFrame` with `mFrameCaptureMutex`, avoiding holding the lock across GPU/driver work (e.g., `finishAllContexts`).

## Crash Details (verbatim log)
```
11-06 17:44:22.107  8621  8906 F libc    : Fatal signal 11 (SIGSEGV), code -1 (SI_QUEUE) in tid 8906 (RHIThread), pid 8621 (cgames.fortnite)
11-06 17:44:23.613  9383  9383 F DEBUG   : *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***
11-06 17:44:23.613  9383  9383 F DEBUG   : Build fingerprint: 'google/frankel/frankel:Baklava/ZP1A.251022.002/14310406:userdebug/dev-keys'
11-06 17:44:23.613  9383  9383 F DEBUG   : Kernel Release: '6.6.98-android15-8-gf5f88cba29ac-ab14027865-4k'
11-06 17:44:23.613  9383  9383 F DEBUG   : Revision: 'EVT1.1'
11-06 17:44:23.613  9383  9383 F DEBUG   : ABI: 'arm64'
11-06 17:44:23.613  9383  9383 F DEBUG   : Timestamp: 2025-11-06 17:44:22.770340634+0800
11-06 17:44:23.613  9383  9383 F DEBUG   : Process uptime: 344s
11-06 17:44:23.613  9383  9383 F DEBUG   : Executable: /system/bin/app_process64
11-06 17:44:23.613  9383  9383 F DEBUG   : Cmdline: com.epicgames.fortnite
11-06 17:44:23.613  9383  9383 F DEBUG   : pid: 8621, tid: 8906, name: RHIThread  >>> com.epicgames.fortnite <<<
11-06 17:44:23.613  9383  9383 F DEBUG   : uid: 10348
11-06 17:44:23.613  9383  9383 F DEBUG   : tagged_addr_ctrl: 0000000000000001 (PR_TAGGED_ADDR_ENABLE)
11-06 17:44:23.613  9383  9383 F DEBUG   : pac_enabled_keys: 000000000000000f (PR_PAC_APIAKEY, PR_PAC_APIBKEY,
PR_PAC_APDAKEY, PR_PAC_APDBKEY)
11-06 17:44:23.613  9383  9383 F DEBUG   : esr: 0000000092000007 (Data Abort Exception 0x24)
11-06 17:44:23.614  9383  9383 F DEBUG   : signal 11 (SIGSEGV), code -1 (SI_QUEUE), fault addr --------
11-06 17:44:23.614  9383  9383 F DEBUG   :     x0  0000000000000012  x1  b400007728cf1050  x2  0000000000000001  x3 0000007497661ea8
11-06 17:44:23.614  9383  9383 F DEBUG   :     x4  00000000000000c0  x5  00000000000000c0  x6  61566e7275746572  x7 65756c61566e7275
11-06 17:44:23.614  9383  9383 F DEBUG   :     x8  00000000fffffffc  x9  0000006800117f0c  x10 00000068006376a0  x11 0000000000000033
11-06 17:44:23.614  9383  9383 F DEBUG   :     x12 0000000000000000  x13 0000000000000000  x14 0000000024680675  x15 0000000024680674
11-06 17:44:23.614  9383  9383 F DEBUG   :     x16 0000000000000001  x17 00000077ddb701e0  x18 000000000000085e  x19 b400007658d3e9c0
11-06 17:44:23.614  9383  9383 F DEBUG   :     x20 b400007728cf1050  x21 b400007668cfc710  x22 b40000677032b358  x23 b400006770567eb0
11-06 17:44:23.614  9383  9383 F DEBUG   :     x24 0000000000000001  x25 b400007589368ff0  x26 b400007699671870  x27 0000006922ab7190
11-06 17:44:23.614  9383  9383 F DEBUG   :     x28 00000073ff16b32c  x29 0000007497661f40
11-06 17:44:23.614  9383  9383 F DEBUG   :     lr  00000068002d26d4  sp  0000007497661e00  pc  00000068002d26cc  pst 0000000080001000
11-06 17:44:23.614  9383  9383 F DEBUG   :     esr 0000000092000007
11-06 17:44:23.614  9383  9383 F DEBUG   : 25 total frames
11-06 17:44:23.614  9383  9383 F DEBUG   : backtrace:
11-06 17:44:23.614  9383  9383 F DEBUG   :       #00 pc 00000000002796cc  /data/app/~~xBm1FlQ1Xj2PJAoWrndMJA==/org.chromium.angle-wsGHjyNuYICrdCxiYXYAXw==/lib/arm64/libGLESv2_angle.so (angle::FrameCaptureShared::onEndFrame(gl::Context*)+268)
11-06 17:44:23.614  9383  9383 F DEBUG   :       #01 pc 00000000004daee0  /data/app/~~xBm1FlQ1Xj2PJAoWrndMJA==/org.chromium.angle-wsGHjyNuYICrdCxiYXYAXw==/lib/arm64/libGLESv2_angle.so (egl::Surface::swap(gl::Context*)+40)
11-06 17:44:23.615  9383  9383 F DEBUG   :       #02 pc 00000000001ca218  /data/app/~~xBm1FlQ1Xj2PJAoWrndMJA==/org.chromium.angle-wsGHjyNuYICrdCxiYXYAXw==/lib/arm64/libGLESv2_angle.so (egl::SwapBuffers(egl::Thread*, egl::Display*, egl::SurfaceID)+80)
11-06 17:44:23.615  9383  9383 F DEBUG   :       #03 pc 00000000001cd4ac  /data/app/~~xBm1FlQ1Xj2PJAoWrndMJA==/org.chromium.angle-wsGHjyNuYICrdCxiYXYAXw==/lib/arm64/libGLESv2_angle.so (EGL_SwapBuffers+216)
11-06 17:44:23.615  9383  9383 F DEBUG   :       #04 pc 000000000001653c  /data/app/~~xBm1FlQ1Xj2PJAoWrndMJA==/org.chromium.angle-wsGHjyNuYICrdCxiYXYAXw==/lib/arm64/libEGL_angle.so (eglSwapBuffers+140)
11-06 17:44:23.615  9383  9383 F DEBUG   :       #05 pc 0000000000026430  /system/lib64/libEGL.so (android::eglSwapBuffersWithDamageKHRImpl(void*, void*, int const*, int)+832)
11-06 17:44:23.615  9383  9383 F DEBUG   :       #06 pc 0000000000022154  /system/lib64/libEGL.so (eglSwapBuffers+52)
```

## Investigation
- The crash consistently points to `FrameCaptureShared::onEndFrame` while app code uses a separate render thread (Unreal `RHIThread`).
- Capture paths append to `mFrameCalls` under `mFrameCaptureMutex` (e.g., `captureCall(...)`). However, `onEndFrame` scanned and consumed `mFrameCalls` without taking the same lock.
- Under load, concurrent mutation during iteration corrupts the underlying vector, causing SIGSEGV.

## Fix Applied
- File: `src/libANGLE/capture/FrameCapture.cpp`
- Changes:
  - Guard scanning of `mFrameCalls` for resource type detection with `std::lock_guard<angle::SimpleMutex>`.
  - Guard tracking of `mActiveFrameIndices` when `mFrameCalls` is non-empty.
  - Guard the call to `writeMainContextCppReplay(...)` (consumes `mFrameCalls`).
  - Guard `reset()` of per-frame state.
- Concurrency considerations: Do not hold `mFrameCaptureMutex` across operations that might call into the driver or block (e.g., `finishAllContexts()`). Locks are scoped narrowly to protect container access.

## Rationale
- Aligns all accesses to `mFrameCalls` with the established synchronization (`mFrameCaptureMutex`), preventing races without broad lock contention.
- Minimizes lock scope to reduce risk of deadlocks and performance regression.

## Validation Plan
- Re-run the reproduction (Android Fortnite with ANGLE frame capture enabled) and validate no crash during `eglSwapBuffers`.
- Stress test with `--gtest_repeat=-1` for capture-enabled tests where available.
- Audit other `FrameCaptureShared` and `FrameCaptureCommon` access paths to ensure they respect the same mutex.
- Optional: Add TSAN bots or local TSAN runs for capture code paths.

## Follow-ups
- Consider snapshotting `mFrameCalls` under lock into a local vector and releasing the lock before heavier processing to further shorten critical sections.
- Add a small unit/end2end test to emulate concurrent capture + end-of-frame consumption, if feasible.

```
What changed (local patch summary):
- Guarded loops and state updates in FrameCapture.cpp with mFrameCaptureMutex.
- Avoided holding the lock across finishAllContexts.
```
