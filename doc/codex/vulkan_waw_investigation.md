# Vulkan WRITE_AFTER_WRITE Hazard Investigation (ANGLE Vulkan backend)

This document tracks the analysis and fixes we tried for a Vulkan validation error that appears after ending a render pass with a depth/stencil attachment.

## Symptom

Validation error reported on `vkCmdPipelineBarrier` immediately after `vkCmdEndRenderingKHR`:

- Hazard: WRITE_AFTER_WRITE (layout transition is a write)
- Prior write: Late Fragment Tests with `VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT`
- The barrier’s source access was 0 (no read/write), causing the hazard

Excerpt:

```
[ SYNC-HAZARD-WRITE-AFTER-WRITE ] vkCmdPipelineBarrier(): WRITE_AFTER_WRITE hazard detected.
prior_access = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT(VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
source accesses = 0
```

## Relevant Code Paths

- Barrier construction for images: `ImageHelper::updateLayoutAndBarrier()` (vk_helpers.cpp)
  - Merges VkImageMemoryBarrier into `PipelineBarrierArray` during render-pass finalization
- Render-pass finalization for depth/stencil: `RenderPassCommandBufferHelper::finalizeDepthStencilImageLayout()`
- Barrier execution: `PipelineBarrier::execute()` -> `vkCmdPipelineBarrier`
- Image access mappings: `vk_barrier_data.cpp` (`ImageAccess` => layouts, stages, access masks)

## Attempted Fixes

### 1) Change the image’s current access before the barrier

- In `finalizeDepthStencilImageLayout()`, set `mCurrentAccess` to a write-inclusive `ImageAccess` (e.g. `DepthWriteStencilWrite`) if the pass wrote/cleared DS.
- Result: new error VUID-VkImageMemoryBarrier-oldLayout-01197
  - Validation tracked previous known layout as a DS read-only layout; our change made `oldLayout`=`GENERAL`, mismatch with tracker.
  - Conclusion: Do not change `mCurrentAccess` just to model prior writes; keep oldLayout intact and only adjust synchronization.

### 2) Add a one-shot hint to strengthen the barrier source side

- Added `ImageHelper::forceNextBarrierFromDepthStencilWrite()` and a boolean flag.
- When the pass wrote/cleared DS, call this in `finalizeDepthStencilImageLayout()` (no layout change).
- In `ImageHelper::updateLayoutAndBarrier()`:
  - If flag set, OR `imageMemoryBarrier.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT`.
  - If using pipeline barrier (not event), also OR `srcStageMask |= Early/Late Fragment Tests`.
- Also added INFO logs to confirm source/dest access/stages and layouts.

Initial outcome:

- Execution logs showed two image barriers for the same image in the same `vkCmdPipelineBarrier`:
  - First: `oldLayout=DEPTH_STENCIL_ATTACHMENT_OPTIMAL -> newLayout=DEPTH_STENCIL_READ_ONLY_OPTIMAL`, srcAccess=0x400 (correct)
  - Second: `oldLayout=DEPTH_STENCIL_READ_ONLY_OPTIMAL -> newLayout=RENDERING_LOCAL_READ_KHR`, srcAccess=0x0 (still wrong)
- Root cause: we cleared the one-shot flag after the first barrier; the second barrier for the same image in the same flush remained unstrengthened.

### 3) Make the hint sticky across all barriers in the same cycle

- Removed clearing of the hint in `updateLayoutAndBarrier()` so any subsequent DS barriers for the same image in the same flush are also strengthened.
- Kept oldLayout untouched to avoid 01197.

## Instrumentation Logs (examples)

- From `finalizeDepthStencilImageLayout()`:

```
INFO: RP finalize DS: wrote depth?=0 wrote stencil?=1 curAccess=10 nextAccess=10 barrierType=Event image=0x…
```

- From `PipelineBarrier::execute()` for the same image:

```
INFO: Executing image barrier: srcStages=0x1f89 dstStages=0x788 srcAccess=0x400 dstAccess=0x220 oldLayout=3 newLayout=4 image=0x…
INFO: Executing image barrier: srcStages=0x1f89 dstStages=0x788 srcAccess=0x0   dstAccess=0x620 oldLayout=4 newLayout=1000117000 image=0x…
```

After the sticky fix, the second line should report `srcAccess=0x400` as well.

## “Other Approach” Considered (and rejected)

- Changing `ContextVk::switchOutReadOnlyDepthStencilMode()` to read per-image render-pass usage flags instead of `mDepthStencilAttachmentFlags`.
- While it may alter pass boundaries enough to hide the hazard, it breaks many tests because those flags are not the authoritative Context state and can be stale.
- Conclusion: keep Context logic as-is; fix synchronization at the barrier construction site.

## Current State and Next Steps

- Code now strengthens all DS barriers emitted right after a DS-writing pass, without changing layouts.
- Action: Re-run the failing repro with Vulkan validation enabled and confirm that both DS barriers for the same image have `srcAccess` including `DEPTH_STENCIL_ATTACHMENT_WRITE_BIT` and WAW is gone.
- If further issues appear, consider:
  - Including aspectMask/level/layer in logs to match validation’s `pImageMemoryBarriers[N]`.
  - Coalescing multiple barriers for the same image if they target the same subresource range.

## Touched Code (high level)

- `src/libANGLE/renderer/vulkan/vk_helpers.h`
  - ImageHelper: added `forceNextBarrierFromDepthStencilWrite()` and a sticky flag.
- `src/libANGLE/renderer/vulkan/vk_helpers.cpp`
  - RenderPass DS finalization: set the hint and added INFO logs.
  - ImageHelper::updateLayoutAndBarrier(): strengthen src side (access/stage) for DS write and keep flag sticky; added INFO logs.
  - ImageHelper::resetCachedProperties(): reset the hint.
- `src/libANGLE/renderer/vulkan/vk_helpers.h`
  - PipelineBarrier::execute(): added INFO logs (debug-build only) to dump DS image barriers.

---

Prepared to help revert the debug logging once the issue is resolved.

## Session 2025-11-15: Unit test vs Trace parity and logging

This session investigated why our new unit tests don’t reproduce the WRITE_AFTER_WRITE seen in the
Fortnite restricted trace and added extensive logging to compare environments and render paths.

Summary of what we tried

- Added several unit tests in `ReadOnlyFeedbackLoopTest.cpp`:
  - `DepthWriteThenReadOnlyFeedbackLoop` (initial minimal repro attempt).
  - `DepthWriteSampleThenDepthWrite` (immediate DS write after sampling to force a pass break).
  - `DepthWriteSampleSwitchFBOThenDepthWrite` (explicit FBO switch + tiny blit to break the pass).
  - `FortniteMimicSequence` (closest to the trace ordering; two FBOs sharing DS, read/draw buffers
    set, fence + `glClientWaitSync`, 1x1 blit, then resume DS writes).
- Added logging in tests and trace harness to capture environment:
  - Test (`ReadOnlyFeedbackLoopVulkanOnlyTest.FortniteMimicSequence`): prints
    `GL_RENDERER/GL_VERSION/GL_VENDOR` and whether
    `GL_ANGLE_read_only_depth_stencil_feedback_loops` is present.
  - Trace (`src/tests/restricted_traces/fortnite/fortnite_0001.cpp::SetupReplay`): prints the same
    plus whether `EGL_ANGLE_feature_control` exists.
- Instrumented ANGLE’s Vulkan backend:
  - `vk_renderer.cpp`: INFO logs when `supportsDynamicRenderingLocalRead` and
    `preferDynamicRendering` are evaluated.
  - `vk_helpers.cpp`: INFO log when `finalizeDepthStencilImageLayout` selects
    `ImageAccess::DepthStencilWriteAndInput` (the “local read” path for DS).

Key observations from runs

- The restricted trace on llvmpipe fails with WAW on a barrier transitioning
  `READ_ONLY (4) -> RENDERING_LOCAL_READ_KHR (1000117000)` where `source accesses = 0`. The label in
  logs is “Render pass closed due to framebuffer change”.
- The unit tests, even on the same llvmpipe device and ES 3.2, continue to emit the classic
  layout sequence (`0 -> 3` DS attachment, then `3 -> 4` DS read-only) and the second barrier has
  `srcAccess=0x400`. No WAW is reported.

Why the paths differ

- ANGLE uses the DS "local read" layout (`VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR`) only if
  `finalizeDepthStencilImageLayout` selects `ImageAccess::DepthStencilWriteAndInput`, which is
  gated by `mRenderPassDesc.hasDepthStencilFramebufferFetch()`.
- The unit tests implement a read-only DS feedback loop (sample the attached DS texture) but do not
  engage DS framebuffer fetch. Therefore ANGLE picks classic READ_ONLY layouts instead of “local
  read” for DS.

Concrete diffs we saw in logs

- Trace (llvmpipe):
  - Second barrier: `oldLayout=4 -> newLayout=1000117000`, `source accesses = 0` (WAW).
  - `TRACE-INFO` shows Mesa/llvmpipe renderer; `EGL_ANGLE_feature_control=no`.
- Unit test (llvmpipe, ES 3.2):
  - Barriers: `oldLayout=0 -> newLayout=3`, then `3 -> 4` with `srcAccess=0x400`.
  - `TEST-INFO` shows Mesa/llvmpipe renderer; extension present.
  - No INFO logs indicating that DS local-read (`DepthStencilWriteAndInput`) was selected.

What we changed in the tests to match the trace

- ES 3.2 variants were instantiated to better match the trace.
- Read/draw buffers (`glReadBuffer/glDrawBuffers`) are set on both FBOs around the DS sampling pass.
- Pass break matched with `glFenceSync/glClientWaitSync(…, 0, 0)` and 1x1 blit between FBOs that
  share the DS texture.
- Stencil/depth state aligned with the trace before the sampling draw (disable cull/depth; use
  `glStencilFunc(GL_EQUAL, 4, 20); glStencilMask(0);`).

Why it still passes

- Without engaging DS framebuffer fetch (where ANGLE sets `DepthStencilWriteAndInput`), the DS image
  continues to use classic READ_ONLY layouts. The unit tests therefore do not reach the
  `READ_ONLY -> LOCAL_READ` barrier that failed in the trace.

Recommended path forward (no barrier hacks)

- Add a new unit test variant that uses DS framebuffer fetch in the fragment shader when ANGLE
  exposes the coherent framebuffer fetch depth/stencil extension
  (`supportsShaderFramebufferFetchDepthStencil`). This will naturally make ANGLE select
  `ImageAccess::DepthStencilWriteAndInput` for DS (local-read path), so the unit test will exercise
  the same barrier sequence as the trace.
  - Skip the test when the extension is not available on the device.
- Keep using the trace as the ground truth for the WAW repro on devices that don’t expose DS
  framebuffer fetch.

Reference commands

- Build tests:
  - `autoninja -C out/Debug angle_end2end_tests`
- Run unit tests with validation:
  - `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \
     ./out/Debug/angle_end2end_tests \
     --gtest_filter="*ReadOnlyFeedbackLoopVulkanOnlyTest*FortniteMimicSequence*"`
- Run restricted trace with validation (see `restricted_traces/README.md`).

Notes

- We avoided “hacking” barrier data to reproduce the WAW in tests, per request. The intention is to
  fix the test setup so it hits the same DS local-read path as the trace, not to force a failure.

## Session 2025-11-18: Forcing pipeline barrier at 4→LOCAL_READ with VkEvent enabled (trace parity)

Goal: Match the trace’s WRITE_AFTER_WRITE at the 4→LOCAL_READ transition while
`UseVkEventForImageBarrier` remains ENABLED.

Status and approach

- Verified that “Executing image barrier …” logs only for pipeline barriers (vk_helpers.h), while
  event barriers log only a concise line with `barrierType=Event`.
- With VkEvent disabled earlier (for sanity), the post‑break 4→LOCAL_READ consistently became a
  pipeline barrier with `srcAccess=0`, matching the trace. With VkEvent re‑enabled (parity), we
  adjusted only GL sequencing to bias ANGLE to use a pipeline barrier at that step.

TracePortedWAWRepro (added)

- Two FBOs share one `GL_DEPTH24_STENCIL8`; 4 MRTs with explicit read/draw buffers on both.
- Pass 1 (FBO A): DS writes (clear + draw).
- Pass 2 (FBO A): DS read‑only sampling with the exact depth/stencil state (disable cull; depth
  reads; `glDepthMask(GL_FALSE)`; stencil read‑only `glStencilFunc(GL_EQUAL,4,20)`). Multiple draws
  to extend the read‑only phase.
- Pass break: fence + `glClientWaitSync(…,0,0)`; `glBlitFramebuffer(1×1)` from A→B.
- Post‑break (FBO B): Iterated sequences to encourage pipeline barrier while VkEvent enabled:
  - Begin pass with a color clear.
  - Tiny DS write draw → DS read‑only draw → (immediately) DS local‑read draw with depth writes off.
  - Variants used FBO toggles, memory barriers, and occasional detach/reattach of DS.

Findings

- With VkEvent enabled, the 4→LOCAL_READ transition still often logs as `barrierType=Event` in this
  environment; issuing a local‑read draw then triggers VUID‑00344 (descriptor requires
  `RENDERING_LOCAL_READ` while the previous known layout is still `READ_ONLY`). This indicates the
  layout change is scheduled too late relative to pipeline bind.
- In some NoAssert runs earlier, the 4→LOCAL_READ transition flipped to pipeline barrier and printed
  “Executing image barrier … srcAccess=0”, though syncval didn’t flag WAW here.

Plan (ongoing)

- Force a first DS image barrier for the same image in the same render pass on FBO B (e.g. DS write
  then DS read‑only), then immediately require DS local‑read; ANGLE’s fallback rules turn the second
  barrier (4→LOCAL_READ) into a pipeline barrier even when VkEvent is enabled.
- If still hitting 00344, insert a minimal pass boundary (e.g. tiny depth write or `glReadPixels`)
  and require local‑read at the next pass begin to schedule the 4→LOCAL_READ as a pipeline barrier.
- Continue iterating until the test prints `barrierType=Pipeline` for 4→LOCAL_READ with
  `Executing image barrier … srcAccess=0`, and syncval reports `SYNC‑HAZARD‑WRITE‑AFTER‑WRITE`.
