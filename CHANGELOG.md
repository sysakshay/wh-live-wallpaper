# 📜 Changelog - Live Video Wallpaper (`live-video-wallpaper`)

All notable changes to the **Live Video Wallpaper** Windhawk mod will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.1.0] - 2026-07-22

### 🔊 Audio & Volume Control
- **Audio Volume Dropdown Setting:** Added a new `audioVolume` setting in Windhawk UI (`100%`, `90%`, `80%`, ..., `Muted (0%)`) capped between 0% and 100%.
- **Reliable Audio Stream Restoration:** Fixed audio state synchronization when unmuting or resuming playback after switching focused applications.

### 🖥️ Aspect Ratio Scaling & Display Fixes
- **Fixed Vertical Stretching:** Solved issue where video wallpaper did not stretch vertically to fill 100% of the screen on wide, ultrawide (21:9 / 32:9), or 1:1 square monitors across `Fill`, `Cover`, and `Fit` modes.
- **Explicit Destination Rectangle:** Passed explicit destination bounds (`pDst = &dest`) to `IMFMediaEngine::TransferVideoFrame`, forcing the D3D11 Video Processor to fill 100% of display dimensions.

### ⚡ Event-Driven Occlusion Detection & CPU Savings
- **Zero-Latency Event Hooks (< 16ms):** Hooked Windows event `EVENT_SYSTEM_FOREGROUND` with `WINEVENT_SKIPOWNPROCESS` for instant audio/video pause when maximizing or focusing application windows.
- **Configurable Safety-Net Polling (`occlusionInterval`):** Added a user setting (`fast`: 100ms, `normal`: 250ms, `relaxed`: 500ms) for background polling safety-nets, allowing lower-end laptops to reduce CPU usage.

### 🎯 Desktop Input & Context Menu Responsiveness
- **Input Hit-Test Transparency:** Added `WM_NCHITTEST` (`HTTRANSPARENT`), `WM_MOUSEACTIVATE` (`MA_NOACTIVATEANDEAT`), `WS_EX_TRANSPARENT`, and `WS_DISABLED` window styles.
- **Instant Context Menus:** The desktop window manager (`User32`) completely bypasses the live wallpaper layer during mouse hit-testing, opening desktop right-click menus with zero latency.
- **Z-Order Timer Optimization:** Added `SWP_NOREDRAW` to `PinBehindTargetWindow()` to prevent desktop invalidation timer churn.

### 📂 Non-Blocking Async File Picker
- **Background Thread Dialog:** Refactored `Ctrl + Alt + G` file selection to run on a background thread so the 60 FPS video wallpaper loop never stutters while picking files.
- **Expanded Format Support:** Supports `.mp4`, `.m4v`, `.mov`, `.wmv`, and `.webm`.
- **Windhawk UI Path Sync:** Selected file paths automatically populate inside the Windhawk UI **Local video path** input box.

### 🔬 Low-Level Performance & Code Health
- **DXGI Flip-Model RTV Caching:** Caches D3D11 render target views while tracking DXGI swap-chain buffer rotation in `Fit` mode, eliminating up to 120 driver-level view creation calls per second inside `ClearTexture`.
- **Zero Heap Allocations in Profiler:** Replaced `std::vector` heap allocations in `RollingHistory` percentile sorting with 60 Hz stack buffers (`float temp[Capacity]`).
- **Cached WorkerW Hierarchy:** $O(1)$ cached `WorkerW` window handle validation avoids running `EnumWindows` across system windows every second.
- **GDI Texture Flag Gating:** Gated `D3D11_RESOURCE_MISC_GDI_COMPATIBLE` on offscreen render targets to apply only when the Profiler HUD is active.
- **Header Cleanups:** Removed unused `#include <vector>` header to resolve compiler cleaner warnings.

---

## [1.0.0] - Initial Release

### 🚀 Core Features
- **Hardware-Accelerated Playback:** Direct3D 11 rendering and Windows Media Foundation zero-copy video decoding directly onto the Windows desktop window hierarchy (`Progman` / `WorkerW`).
- **Built-in Performance Profiler HUD (`Ctrl + Alt + D`):** Live HUD overlay displaying true wall-clock FPS, frame-time percentiles (`P95`/`P99`), VRAM/commit stats, and pipeline stage timings (`Decode`, `Transfer`, `Present`).
- **Smart Occlusion Detection:** Automatically pauses video rendering when top-level or maximized application windows fully cover the desktop (`P95` CPU occlusion check < 0.1ms).
- **Battery-Aware Power Saving:** Configurable behavior (`Pause`, `Drop to 15 FPS`, or `Play normally`) when running on laptop battery power.
- **Instant First-Frame Presentation:** Decodes and presents the initial video frame immediately upon loading so the wallpaper never gets stuck on a black screen when starting in paused/battery mode.
- **Flexible Scaling Modes:** Support for `Fill` (stretch/zoom to fill screen) and `Fit` (letterbox with cinema-style black borders).
- **Interactive File Picker (`Ctrl + Alt + G`):** Interactive file dialog to choose videos on the fly.
- **Auto-Recovery:** Automatic D3D device-loss (`DXGI_ERROR_DEVICE_REMOVED`) and Media Foundation decode stall recovery.
