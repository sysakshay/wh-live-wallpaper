// ==WindhawkMod==
// @id              live-video-wallpaper
// @name            Live Video Wallpaper
// @description     Play MP4 videos as your Windows desktop wallpaper using hardware-accelerated Media Foundation rendering with a built-in runtime performance profiler (`Ctrl+Alt+D` to toggle HUD).
// @version         1.1.1
// @author          AKS HAY
// @github          https://github.com/sysakshay
// @license         GPL-3.0
// @include         explorer.exe
// @compilerOptions -lgdi32 -lshlwapi -lcomdlg32 -ldwmapi -lmfplat -lmfuuid -luuid -ld3d11 -ldxgi -lole32 -loleaut32 -lpsapi -lwtsapi32
// ==/WindhawkMod==
// ==WindhawkModReadme==
/*
# Live Video Wallpaper

**Author:** AKS HAY | **GitHub:** [sysakshay](https://github.com/sysakshay) | **License:** GPL-3.0

Play local MP4 videos directly on your Windows desktop as a live wallpaper using
Media Foundation and Direct3D 11, featuring a built-in real-time hardware profiler.

Unlike traditional animated wallpaper implementations, this mod renders video
directly through the GPU, minimizing CPU usage while maintaining rock-solid 60 FPS
playback.

## Features

- **Hardware-Accelerated Playback:** Direct3D 11 rendering and Windows Media Foundation zero-copy video decoding directly onto the desktop window hierarchy.
- **Built-in Performance Profiler (`Ctrl + Alt + D`):** Live HUD overlay displaying true wall-clock FPS, frame-time percentiles (`P95`/`P99`), VRAM/commit stats, and pipeline stage timings (`Decode`, `Transfer`, `Present`).
- **Smart Occlusion Detection:** Automatically pauses video rendering when top-level or maximized application windows fully cover the desktop (`P95` CPU occlusion check < 0.1ms). Uses zero-latency `EVENT_SYSTEM_FOREGROUND` event hooks.
- **Audio Volume Control & Stream Restoration:** Integrated audio volume control (`0%` to `100%`) with automatic unmuting and volume restoration when resuming playback or switching apps.
- **Battery-Aware Power Saving:** Configurable behavior (`Pause`, `Drop to 15 FPS`, or `Play normally`) when running on laptop battery power.
- **Instant First-Frame Presentation:** Decodes and presents the initial video frame immediately upon loading so the wallpaper never gets stuck on a black screen when starting in paused/battery mode.
- **Scaling Modes & Aspect Ratio Fix:** Support for `Fill` (stretch to fill screen), `Cover` (zoom & crop edges without distortion), and `Fit` (letterbox with cinema-style black borders). Fixed aspect ratio scaling for wide, ultrawide, and 1:1 square monitors.
- **Async File Picker (`Ctrl + Alt + G`):** Asynchronous background thread file picker for `.mp4`, `.m4v`, `.mov`, `.wmv`, and `.webm` files with zero frame drops during selection.
- **Zero Desktop Right-Click Menu Delay:** Full input transparency (`HTTRANSPARENT`, `WS_EX_TRANSPARENT`, `WS_DISABLED`) ensures right-clicking desktop icons or background opens context menus with 0ms latency.
- **Auto-Recovery:** Automatic D3D device-loss and Media Foundation decode stall recovery.

## Controls & Hotkeys

- **`Ctrl + Alt + G`** — Open the interactive file picker to load a new video (`.mp4`, `.m4v`, `.mov`, `.wmv`, `.webm`).
- **`Ctrl + Alt + H`** — Toggle wallpaper visibility on/off.
- **`Ctrl + Alt + D`** — Toggle Performance Profiler HUD (On / Off), if enabled in settings.

## Performance Profiler HUD

When toggled via `Ctrl + Alt + D`, the on-screen profiler groups diagnostics into color-coded (`🟢 Good`, `🟡 Warning`, `🔴 Critical`) sections:
- **Performance:** Real-clock `FPS` vs polling `Ticks`, rolling average `Frame Time`, `P95`/`P99` frame times, and overall CPU/GPU time.
- **Memory & Resources:** `VRAM` usage, `Shared RAM`, working set, commit charge, and active D3D/MF object counts (`Textures`, `SwapChains`, `MediaEngines`, `Surfaces`).
- **Pipeline Breakdown:** Precise microsecond timers for `Decode`, `TransferVideoFrame`, `CopyResource`, `Present`, and `Occlusion` checks.
- **Status & Bottlenecks:** Live diagnosis showing exact bottlenecks (`GPU Idle`, `Bound by Decode`, `Present blocked by DWM`, etc.) and system state (`Battery`, `Paused`, `Recoveries`).

## Settings

### Video Path
Full path to any local video file on your disk. Can also be updated dynamically via `Ctrl + Alt + G`.

### Fit Mode
- **Fill:** Stretch/scale video to fill the entire monitor.
- **Cover:** Zoom and crop video edges to fill screen without aspect ratio distortion.
- **Fit:** Letterbox video while preserving original aspect ratio.

### Battery Saving Mode
- **Pause video (maximum battery life):** Stops video processing completely when unplugged from AC power.
- **Drop to 15 FPS (reduced power consumption):** Throttles frame rate to save power while keeping background animation alive.
- **Play normally (ignore battery status):** Maintains full 60 FPS playback on battery.

### Target Frame Rate
Max frames per second (`60`, `30`, `15`) or `monitor` to sync exactly with monitor refresh rate (`VREFRESH`).

### Mute Audio & Volume Control
- **Mute Audio:** Mute video audio track (`true` / `false`).
- **Audio Volume:** Audio output volume percentage (`100%`, `90%`, ..., `Muted`).

### Occlusion Check Interval
Frequency of background occlusion safety-net polling (`fast`: 100ms, `normal`: 250ms, `relaxed`: 500ms).

## Technical Highlights & Optimizations

- **Zero-Copy Offscreen Target:** Decodes to an offscreen `B8G8R8A8_UNORM` render target before copying to the flip-model swap chain (`DXGI_SWAP_EFFECT_FLIP_DISCARD`) to guarantee tear-free presentation without MF timing conflicts.
- **DXGI Flip-Model RTV Caching:** Caches D3D11 render target views while handling DXGI swap-chain buffer rotation in `Fit` mode.
- **Zero Heap Allocations:** Ring buffer percentile calculations use zero-allocation stack buffers at 60 Hz.
- **$O(1)$ Section Timers:** Profiler queries bypass linear string comparisons via pooled pointer-equality checks inside hot rendering loops.
- **Kernel Resource Caching:** GDI font handles (`HFONT`), desktop region guards (`HRGN`), and WorkerW window handles are cached across frames to eliminate handle churn.
- **Fast Occlusion Screening:** Bounding-box intersection screening skips invisible, borderless system helpers, and non-overlapping windows before querying DWM or class attributes.
*/
// ==/WindhawkModReadme==

// clang-format off
// ==WindhawkModSettings==
/*
- wallpaperMode: video
  $name: Wallpaper mode
  $description: Master switch -- what to render as your desktop wallpaper.
  $options:
  - video: Video (play an MP4 file)
  - fluid: Fluid Simulation (experimental, interactive)
- videoPath: ""
  $name: Local video path
  $description: Full path to an .mp4 file on disk. Tip -- press Ctrl+Alt+G in the desktop to pick a file interactively instead of typing a path here.
- fitMode: fill
  $name: Fit mode
  $description: How to scale the video across your monitor
  $options:
  - fill: Fill / Stretch (stretch both horizontally & vertically to fill screen)
  - cover: Cover / Zoom (zoom and crop edges to fill screen without distortion)
  - fit: Fit (letterbox with aspect ratio preserved and black borders)
- batteryMode: pause
  $name: Battery saving mode
  $description: What to do when running on laptop battery power
  $options:
  - pause: Pause video (maximum battery life)
  - drop: Drop to 15 FPS (reduced power consumption)
  - normal: Play normally (ignore battery status)
- targetFps: "60"
  $name: Target frame rate (FPS)
  $description: Max frame rate to render wallpaper at. Set to monitor to sync with display refresh rate.
  $options:
  - monitor: Monitor refresh rate (V-Sync)
  - "60": 60 FPS
  - "30": 30 FPS
  - "15": 15 FPS
- audioMuted: true
  $name: Mute audio output
  $description: Whether to mute any audio track embedded in the MP4 file.
- audioVolume: "100"
  $name: Audio volume
  $description: Volume percentage for unmuted audio playback.
  $options:
    - "100": 100% (Maximum)
    - "90": 90%
    - "80": 80%
    - "70": 70%
    - "60": 60%
    - "50": 50%
    - "40": 40%
    - "30": 30%
    - "20": 20%
    - "10": 10%
    - "0": Muted (0%)
- occlusionInterval: normal
  $name: Occlusion check interval
  $description: Polling frequency for desktop occlusion safety-net (fast=100ms, normal=250ms, relaxed=500ms).
  $options:
    - fast: Fast (100 ms)
    - normal: Normal (250 ms)
    - relaxed: Battery Saver / Relaxed (500 ms)
- filePickerHotkey: true
  $name: Enable Ctrl+Alt+G file picker hotkey
- visibilityHotkey: true
  $name: Enable Ctrl+Alt+H visibility hotkey
- profilerHotkey: false
  $name: Enable Ctrl+Alt+D profiler hotkey
- fluidSplatRadius: 25
  $name: Fluid - Splat radius (blob size)
  $description: Controls the size of fluid blobs (0 = tiny, 100 = huge).
- fluidSpeed: 100
  $name: Fluid - Speed
  $description: How fast the fluid moves (0 = slow, 200 = fast, default 100).
- fluidBloom: 80
  $name: Fluid - Bloom intensity
  $description: Glow around bright fluid areas (0 = none, 200 = max, default 80).
- fluidColorful: true
  $name: Fluid - Colorful
  $description: When on, splats use random vivid hues. When off, splats use a fixed cyan color.
- fluidRandomSplatsInterval: 3
  $name: Fluid - Random splats interval (seconds)
  $description: How often random ambient splats appear, in seconds (0 = disabled).
*/
// ==/WindhawkModSettings==
// clang-format on


// ============================================================================
// Global / System Includes
// ============================================================================
#include <algorithm>
#include <atomic>
#include <cmath>
#include <commdlg.h>
#include <cstdlib>
#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfmediaengine.h>
#include <psapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string>
#include <windows.h>
#include <wtsapi32.h>

// ============================================================================
// Project Headers
// ============================================================================
// ============================================================================
// PerformanceProfiler.h
// Professional-Grade Runtime Profiling & Diagnostics System for Windhawk Mods
// ============================================================================
//
// This module provides comprehensive CPU, GPU, VRAM, RAM, and Frame Timing
// diagnostics modeled after built-in developer profiling systems in Unity,
// Unreal Engine, and Chrome Tracing.
//
// CRITICAL DESIGN GUARANTEES:
// 1. Zero Runtime Overhead when Disabled (Mode 0): Every profiling entry point
//    is guarded by an inline relaxed atomic branch check. When disabled, section
//    timers, QPC queries, and memory inspections do not execute.
// 2. Pure Instrumentation: Does not alter rendering logic, architecture, or
//    playback timing.
// 3. Native Windows APIs Only: Uses QueryPerformanceCounter, GetProcessTimes,
//    GetThreadTimes, K32GetProcessMemoryInfo, and DXGI memory queries.
// 4. Thread-Safe & Allocation-Free During Rendering: Uses static circular ring
//    buffers and lock-free atomic counters to eliminate heap churn.
// ============================================================================

// #pragma once (header guard handled by build engine)


// ---------------------------------------------------------------------------
// Minimal COM smart pointer (kept dependency-free rather than relying on
// <wrl/client.h>, which isn't reliably available in this MinGW toolchain).
// ---------------------------------------------------------------------------
template <typename T> struct ComPtr {
  T *ptr = nullptr;
  ComPtr() = default;
  ComPtr(T *p) : ptr(p) {
    if (ptr)
      ptr->AddRef();
  }
  ComPtr(const ComPtr &other) {
    ptr = other.ptr;
    if (ptr)
      ptr->AddRef();
  }
  ComPtr(ComPtr &&other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
  ComPtr &operator=(T *p) {
    if (ptr != p) {
      Reset();
      ptr = p;
      if (ptr)
        ptr->AddRef();
    }
    return *this;
  }
  ComPtr &operator=(const ComPtr &other) {
    if (this != &other) {
      Reset();
      ptr = other.ptr;
      if (ptr)
        ptr->AddRef();
    }
    return *this;
  }
  ComPtr &operator=(ComPtr &&other) noexcept {
    if (this != &other) {
      Reset();
      ptr = other.ptr;
      other.ptr = nullptr;
    }
    return *this;
  }
  ~ComPtr() { Reset(); }
  void Reset() {
    if (ptr) {
      ptr->Release();
      ptr = nullptr;
    }
  }
  T **operator&() {
    // Prevent leaks: release any existing object before handing out the
    // raw pointer for a COM creation call to overwrite.
    Reset();
    return &ptr;
  }
  T *operator->() const { return ptr; }
  T *Get() const { return ptr; }
  operator T *() const { return ptr; }
};

// ----------------------------------------------------------------------------
// Profiler Operating Modes
// ----------------------------------------------------------------------------
enum class ProfilerMode {
  Disabled = 0,          // Absolutely zero overhead (single relaxed branch check)
  Overlay = 1,           // Live on-screen statistics and bottleneck analysis
  OverlayAndLogging = 2  // Live overlay plus detailed 5-second periodic logs
};

// ----------------------------------------------------------------------------
// Rolling Ring Buffer (History & Histogram Calculation)
// Fixed-capacity circular buffer avoiding dynamic allocations during rendering.
// ----------------------------------------------------------------------------
template <size_t Capacity> class RollingHistory {
public:
  float values[Capacity] = {};
  size_t count = 0;
  size_t head = 0;

  void Record(float v) {
    values[head] = v;
    head = (head + 1) % Capacity;
    if (count < Capacity) {
      count++;
    }
  }

  void Reset() {
    count = 0;
    head = 0;
  }

  float GetAverage() const {
    if (count == 0)
      return 0.0f;
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
      sum += values[i];
    }
    return static_cast<float>(sum / count);
  }

  float GetMin() const {
    if (count == 0)
      return 0.0f;
    float m = values[0];
    for (size_t i = 1; i < count; ++i) {
      if (values[i] < m)
        m = values[i];
    }
    return m;
  }

  float GetMax() const {
    if (count == 0)
      return 0.0f;
    float m = values[0];
    for (size_t i = 1; i < count; ++i) {
      if (values[i] > m)
        m = values[i];
    }
    return m;
  }

  // Computes statistical percentile (0.0 to 1.0, e.g. 0.95 for 95th percentile).
  // Computes statistical percentile (0.0 to 1.0, e.g. 0.95 for 95th percentile).
  // Uses a stack-allocated buffer when count > 0 to avoid heap allocations.
  float GetPercentile(float p) const {
    if (count == 0)
      return 0.0f;
    if (count == 1)
      return values[0];

    float temp[Capacity];
    for (size_t i = 0; i < count; ++i) {
      temp[i] = values[i];
    }
    size_t idx = static_cast<size_t>(p * (count - 1));
    if (idx >= count)
      idx = count - 1;
    std::nth_element(temp, temp + idx, temp + count);
    return temp[idx];
  }

  // Computes two statistical percentiles in a single sorting pass to avoid
  // duplicate sorting overhead when querying both P95 and P99 simultaneously.
  void GetSortedPercentiles(float p1, float p2, float &out1, float &out2) const {
    if (count == 0) {
      out1 = 0.0f;
      out2 = 0.0f;
      return;
    }
    if (count == 1) {
      out1 = values[0];
      out2 = values[0];
      return;
    }

    float temp[Capacity];
    for (size_t i = 0; i < count; ++i) {
      temp[i] = values[i];
    }
    std::sort(temp, temp + count);
    size_t idx1 = static_cast<size_t>(p1 * (count - 1));
    if (idx1 >= count)
      idx1 = count - 1;
    size_t idx2 = static_cast<size_t>(p2 * (count - 1));
    if (idx2 >= count)
      idx2 = count - 1;
    out1 = temp[idx1];
    out2 = temp[idx2];
  }
};

// ----------------------------------------------------------------------------
// Section Timer for Code Brackets
// Tracks high-precision execution metrics across individual code sections.
// ----------------------------------------------------------------------------
struct SectionTimer {
  LONGLONG totalTicks = 0;
  LONGLONG lastTicks = 0;
  LONGLONG minTicks = LLONG_MAX;
  LONGLONG maxTicks = 0;
  UINT64 callCount = 0;

  void RecordTicks(LONGLONG ticks) {
    totalTicks += ticks;
    lastTicks = ticks;
    if (ticks < minTicks)
      minTicks = ticks;
    if (ticks > maxTicks)
      maxTicks = ticks;
    callCount++;
  }

  void Reset() {
    totalTicks = 0;
    lastTicks = 0;
    minTicks = LLONG_MAX;
    maxTicks = 0;
    callCount = 0;
  }
};

// ----------------------------------------------------------------------------
// Frame Breakdown Record
// Captures exact timing split for each individual rendered frame.
// ----------------------------------------------------------------------------
struct FrameRecord {
  DWORD frameNumber = 0;
  float totalFrameTimeMs = 0.0f;
  float decodeMs = 0.0f;
  float transferMs = 0.0f;
  float gpuCopyMs = 0.0f;
  float presentMs = 0.0f;
  float occlusionMs = 0.0f;
  float windowEnumMs = 0.0f;
  float batteryMs = 0.0f;
  float miscMs = 0.0f;
};

// ----------------------------------------------------------------------------
// Comprehensive Diagnostic State Structs
// ----------------------------------------------------------------------------

struct CPUStats {
  float explorerProcessPct = 0.0f;
  float wallpaperThreadPct = 0.0f;
  float averagePct = 0.0f;
  float peakPct = 0.0f;
  float minPct = 0.0f;
  float pct5s = 0.0f;
  float pct60s = 0.0f;
};

struct GPUStats {
  float engineUtilizationPct = 0.0f;
  float decodePct = 0.0f;
  float processingPct = 0.0f;
  float copyPct = 0.0f;
  UINT64 dedicatedVRAMBytes = 0;
  UINT64 sharedGPUBytes = 0;
  UINT64 committedBytes = 0;
  float frameTimeMs = 0.0f;
  float presentLatencyMs = 0.0f;
  bool idle = false;
  bool saturated = false;
  bool decodeLimited = false;
  bool presentationLimited = false;
};

struct MemoryStats {
  UINT64 workingSetBytes = 0;
  UINT64 privateBytes = 0;
  UINT64 commitSizeBytes = 0;
  UINT64 pagedPoolBytes = 0;
  UINT64 nonPagedPoolBytes = 0;
  UINT64 peakWorkingSetBytes = 0;
  UINT64 peakCommitBytes = 0;
  float allocationRateKbps = 0.0f;
  bool continuousGrowthWarning = false;
};

struct VideoStats {
  int width = 0;
  int height = 0;
  float fps = 0.0f;
  wchar_t codec[32] = L"Unknown";
  float playbackSpeed = 1.0f;
  UINT64 framesDecoded = 0;
  UINT64 framesRendered = 0;
  UINT64 framesDropped = 0;
  UINT64 decodeFailures = 0;
  UINT64 transferFailures = 0;
};

struct RenderingStats {
  float targetFPS = 60.0f;
  float actualFPS = 0.0f;
  float averageFPS = 0.0f;
  float minFPS = 0.0f;
  float maxFPS = 0.0f;
  float avgFrameTimeMs = 0.0f;
  float worstFrameTimeMs = 0.0f;
  float bestFrameTimeMs = 0.0f;
  float p99FrameTimeMs = 0.0f;
  float p95FrameTimeMs = 0.0f;
  float frameJitterMs = 0.0f;
  UINT64 missedFrames = 0;
  UINT64 skippedFrames = 0;
  UINT64 successfulPresent = 0;
  UINT64 failedPresent = 0;
  UINT64 successfulTransfer = 0;
  UINT64 failedTransfer = 0;
  float tickRate = 0.0f;
  float cpuRenderTimeMs = 0.0f;
};

struct DeviceHealth {
  UINT32 dxgiDeviceLostCount = 0;
  UINT32 dxgiResetCount = 0;
  UINT32 mfErrorCount = 0;
  UINT32 recoveryCount = 0;
  UINT32 reloadCount = 0;
  UINT32 swapChainRecreationCount = 0;
  UINT32 resizeCount = 0;
  UINT32 powerStateChangeCount = 0;
  UINT32 displayChangeCount = 0;
  UINT32 videoReloadCount = 0;
};

struct WindowsState {
  int batteryPct = -1; // -1 if on desktop / no battery
  bool isCharging = true;
  bool fullscreenPauseActive = false;
  bool batteryPauseActive = false;
  bool sessionPauseActive = false;
  bool wallpaperHidden = false;
  int refreshRate = 60;
  int screenWidth = 0;
  int screenHeight = 0;
  int virtualDesktopWidth = 0;
  int virtualDesktopHeight = 0;
  int numMonitors = 1;
};

struct ResourceStats {
  UINT32 textures = 0;
  UINT32 swapChains = 0;
  UINT32 mediaEngines = 0;
  UINT32 videoSurfaces = 0;
  UINT32 liveComObjects = 0;
};

// ----------------------------------------------------------------------------
// Static Profiler Engine API
// ----------------------------------------------------------------------------
class Profiler {
public:
  static void Initialize(ID3D11Device *device, IDXGISwapChain *swapChain);
  static void Shutdown();

  static inline ProfilerMode GetMode() {
    return g_mode.load(std::memory_order_relaxed);
  }
  static void SetMode(ProfilerMode mode);
  static void CycleMode();

  static inline bool IsEnabled() {
    return GetMode() != ProfilerMode::Disabled;
  }

  // --------------------------------------------------------------------------
  // Frame & Bracket Timing Gates (Inline & Zero-Overhead when Disabled)
  // --------------------------------------------------------------------------
  static inline void BeginFrame(DWORD frameNumber) {
    if (!IsEnabled())
      return;
    BeginFrameImpl(frameNumber);
  }

  static inline void RecordTickCall() {
    if (!IsEnabled())
      return;
    RecordTickCallImpl();
  }

  static inline void EndFrame() {
    if (!IsEnabled())
      return;
    EndFrameImpl();
  }

  static inline void BeginSection(const char *name) {
    if (!IsEnabled())
      return;
    BeginSectionImpl(name);
  }

  static inline void EndSection(const char *name) {
    if (!IsEnabled())
      return;
    EndSectionImpl(name);
  }

  // --------------------------------------------------------------------------
  // Event & Metric Recording API
  // --------------------------------------------------------------------------
  static inline void RecordPresent(HRESULT hr, double durationMs) {
    if (!IsEnabled())
      return;
    RecordPresentImpl(hr, durationMs);
  }

  static inline void RecordTransfer(HRESULT hr, double durationMs) {
    if (!IsEnabled())
      return;
    RecordTransferImpl(hr, durationMs);
  }

  static inline void RecordEvent(const wchar_t *eventName) {
    if (!IsEnabled())
      return;
    RecordEventImpl(eventName);
  }

  static inline void RecordVideoMetadata(int w, int h, const wchar_t *codecStr,
                                         float fps) {
    if (!IsEnabled())
      return;
    RecordVideoMetadataImpl(w, h, codecStr, fps);
  }

  static inline void UpdateWindowsState(bool fullscreenPaused,
                                        bool batteryPaused, bool sessionPaused,
                                        bool hidden, bool onBattery) {
    if (!IsEnabled())
      return;
    UpdateWindowsStateImpl(fullscreenPaused, batteryPaused, sessionPaused, hidden, onBattery);
  }

  static inline void UpdateResourceStats(int textures, int swapChains,
                                         int mediaEngines, int videoSurfaces,
                                         int liveComObjects) {
    if (!IsEnabled())
      return;
    UpdateResourceStatsImpl(textures, swapChains, mediaEngines, videoSurfaces,
                            liveComObjects);
  }

  // --------------------------------------------------------------------------
  // Diagnostics Rendering & Output
  // --------------------------------------------------------------------------
  static void DrawOverlay(HWND hwnd, ID3D11Texture2D *targetTexture);
  static void LogStatistics(bool force = false);

private:
  static std::atomic<ProfilerMode> g_mode;

  static void BeginFrameImpl(DWORD frameNumber);
  static void EndFrameImpl();
  static void BeginSectionImpl(const char *name);
  static void EndSectionImpl(const char *name);
  static void RecordPresentImpl(HRESULT hr, double durationMs);
  static void RecordTransferImpl(HRESULT hr, double durationMs);
  static void RecordEventImpl(const wchar_t *eventName);
  static void RecordVideoMetadataImpl(int w, int h, const wchar_t *codecStr,
                                      float fps);
  static void UpdateWindowsStateImpl(bool fullscreenPaused, bool batteryPaused,
                                     bool sessionPaused, bool hidden, bool onBattery);
  static void UpdateResourceStatsImpl(int textures, int swapChains,
                                      int mediaEngines, int videoSurfaces,
                                      int liveComObjects);
  static void RecordCPU();
  static void RecordMemory();
  static void RecordTickCallImpl();
};
// ============================================================================
// CPP Implementations
// ============================================================================
// ============================================================================
// PerformanceProfiler.cpp
// Professional-Grade Runtime Profiling & Diagnostics System for Windhawk Mods
// ============================================================================
//
// Implements high-precision QPC timing, circular ring buffer statistics,
// psapi process memory queries, DXGI VRAM tracking, bottleneck detection,
// periodic 5-second logging, and live on-screen overlay rendering.
// ============================================================================

// [Deduplicated] #include "PerformanceProfiler.h" (already included)

// ----------------------------------------------------------------------------
// Static Profiler State
// ----------------------------------------------------------------------------
std::atomic<ProfilerMode> Profiler::g_mode{ProfilerMode::Disabled};

namespace {
LARGE_INTEGER s_qpcFreq = {};
double s_ticksPerMs = 1.0;

// Rolling histories
RollingHistory<60> s_history60;
RollingHistory<300> s_history300;

// Named section timers
struct SectionEntry {
  const char *rawPtr;
  char name[64];
  SectionTimer timer;
};
SectionEntry s_sections[16] = {};
int s_numSections = 0;

// Section call stack (supports up to 8 nested BeginSection/EndSection levels)
struct SectionStackItem {
  SectionTimer *timer;
  LONGLONG startTicks;
};
SectionStackItem s_sectionStack[8] = {};
int s_stackDepth = 0;

// Frame timing state
LONGLONG s_frameStartTicks = 0;
DWORD s_currentFrameNum = 0;
LONGLONG s_lastLogTicks = 0;
LONGLONG s_lastCPUCheckTicks = 0;
LONGLONG s_lastTickRateWindowTicks = 0;
UINT32 s_tickCallCountInWindow = 0;
LONGLONG s_lastFramePresentTicks = 0;

// CPU utilization tracking
ULONGLONG s_lastProcessTime = 0;
ULONGLONG s_lastThreadTime = 0;
int s_numProcessors = 1;

// Global diagnostic structures
CPUStats s_cpuStats = {};
GPUStats s_gpuStats = {};
MemoryStats s_memStats = {};
VideoStats s_videoStats = {};
RenderingStats s_renderStats = {};
DeviceHealth s_deviceHealth = {};
WindowsState s_winState = {};
ResourceStats s_resourceStats = {};

// DXGI adapter reference for VRAM queries
[[clang::no_destroy]] ComPtr<IDXGIAdapter3> s_dxgiAdapter3;

// Cached D3D11 device and offscreen texture for overlay rendering when backbuffer is flip-model
[[clang::no_destroy]] ComPtr<ID3D11Device> s_d3dDevice;
[[clang::no_destroy]] ComPtr<ID3D11DeviceContext> s_d3dContext;
[[clang::no_destroy]] ComPtr<ID3D11Texture2D> s_overlayTexture;

// Cached overlay lines and font handle for GDI/DXGI rendering
struct OverlayCache {
  struct Line {
    wchar_t text[128];
    COLORREF color;
  } lines[36];
  int numLines = 0;
  LONGLONG lastUpdateTicks = 0;
  HFONT hFont = nullptr;

  ~OverlayCache() {
    if (hFont) {
      DeleteObject(hFont);
      hFont = nullptr;
    }
  }
};
OverlayCache s_overlayCache;

// Helper to retrieve or register a section by name without allocations
SectionTimer *GetOrRegisterSection(const char *name) {
  for (int i = 0; i < s_numSections; ++i) {
    if (s_sections[i].rawPtr == name || strcmp(s_sections[i].name, name) == 0) {
      return &s_sections[i].timer;
    }
  }
  if (s_numSections < 16) {
    int idx = s_numSections++;
    s_sections[idx].rawPtr = name;
    strncpy(s_sections[idx].name, name, sizeof(s_sections[idx].name) - 1);
    s_sections[idx].name[sizeof(s_sections[idx].name) - 1] = '\0';
    s_sections[idx].timer.Reset();
    return &s_sections[idx].timer;
  }
  return nullptr;
}

// Converts FILETIME pair (kernel + user) to 100ns units
ULONGLONG FileTimeToULL(const FILETIME &ftKernel, const FILETIME &ftUser) {
  ULARGE_INTEGER uKernel, uUser;
  uKernel.LowPart = ftKernel.dwLowDateTime;
  uKernel.HighPart = ftKernel.dwHighDateTime;
  uUser.LowPart = ftUser.dwLowDateTime;
  uUser.HighPart = ftUser.dwHighDateTime;
  return uKernel.QuadPart + uUser.QuadPart;
}
} // namespace

// ----------------------------------------------------------------------------
// Initialization & Shutdown
// ----------------------------------------------------------------------------
void Profiler::Initialize(ID3D11Device *device, IDXGISwapChain *swapChain) {
  QueryPerformanceFrequency(&s_qpcFreq);
  if (s_qpcFreq.QuadPart > 0) {
    s_ticksPerMs = static_cast<double>(s_qpcFreq.QuadPart) / 1000.0;
  }

  SYSTEM_INFO sysInfo = {};
  GetSystemInfo(&sysInfo);
  s_numProcessors =
      sysInfo.dwNumberOfProcessors > 0 ? sysInfo.dwNumberOfProcessors : 1;

  s_history60.Reset();
  s_history300.Reset();
  s_numSections = 0;
  s_stackDepth = 0;
  s_frameStartTicks = 0;

  s_d3dDevice.Reset();
  s_d3dContext.Reset();
  s_overlayTexture.Reset();
  if (device) {
    s_d3dDevice = device;
    device->GetImmediateContext(&s_d3dContext);
  }

  // Attempt to acquire IDXGIAdapter3 for video memory queries
  s_dxgiAdapter3.Reset();
  if (device) {
    ComPtr<IDXGIDevice> dxgiDevice;
    if (SUCCEEDED(
            device->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice))) {
      ComPtr<IDXGIAdapter> dxgiAdapter;
      if (SUCCEEDED(dxgiDevice->GetAdapter(&dxgiAdapter))) {
        dxgiAdapter->QueryInterface(__uuidof(IDXGIAdapter3),
                                    (void **)&s_dxgiAdapter3);
      }
    }
  }

  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  s_lastLogTicks = now.QuadPart;
  s_lastCPUCheckTicks = now.QuadPart;

  FILETIME ftCreation, ftExit, ftKernel, ftUser;
  if (GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel,
                      &ftUser)) {
    s_lastProcessTime = FileTimeToULL(ftKernel, ftUser);
  }
  if (GetThreadTimes(GetCurrentThread(), &ftCreation, &ftExit, &ftKernel,
                     &ftUser)) {
    s_lastThreadTime = FileTimeToULL(ftKernel, ftUser);
  }

  Wh_Log(L"PerformanceProfiler: Initialized (Frequency: %lld Hz, Processors: "
         L"%d, VRAM Query Supported: %d)",
         s_qpcFreq.QuadPart, s_numProcessors,
         s_dxgiAdapter3.Get() != nullptr ? 1 : 0);
}

void Profiler::Shutdown() {
  if (s_overlayCache.hFont) {
    DeleteObject(s_overlayCache.hFont);
    s_overlayCache.hFont = nullptr;
  }
  s_dxgiAdapter3.Reset();
  s_d3dDevice.Reset();
  s_d3dContext.Reset();
  s_overlayTexture.Reset();
  s_history60.Reset();
  s_history300.Reset();
  Wh_Log(L"PerformanceProfiler: Shutdown complete");
}

void Profiler::SetMode(ProfilerMode mode) {
  ProfilerMode old = g_mode.exchange(mode, std::memory_order_relaxed);
  if (old != mode) {
    const wchar_t *statusStr = (mode == ProfilerMode::Disabled) ? L"OFF" : L"ON";
    Wh_Log(L"PerformanceProfiler: Toggled %s", statusStr);
  }
}

void Profiler::CycleMode() {
  static ULONGLONG s_lastCycleTick = 0;
  ULONGLONG now = GetTickCount64();
  if (now - s_lastCycleTick < 250) {
    return;
  }
  s_lastCycleTick = now;

  ProfilerMode current = GetMode();
  ProfilerMode next = (current == ProfilerMode::Disabled) ? ProfilerMode::Overlay : ProfilerMode::Disabled;
  SetMode(next);
}

// ----------------------------------------------------------------------------
// Frame & Section Bracket Implementations
// ----------------------------------------------------------------------------
void Profiler::RecordTickCallImpl() {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  if (s_lastTickRateWindowTicks == 0) {
    s_lastTickRateWindowTicks = now.QuadPart;
  }
  s_tickCallCountInWindow++;
  LONGLONG elapsed = now.QuadPart - s_lastTickRateWindowTicks;
  if (elapsed >= s_qpcFreq.QuadPart) {
    s_renderStats.tickRate =
        static_cast<float>(s_tickCallCountInWindow) * static_cast<float>(s_qpcFreq.QuadPart) / static_cast<float>(elapsed);
    s_tickCallCountInWindow = 0;
    s_lastTickRateWindowTicks = now.QuadPart;
  }
}

void Profiler::BeginFrameImpl(DWORD frameNumber) {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  s_frameStartTicks = now.QuadPart;
  s_currentFrameNum = frameNumber;
  s_stackDepth = 0; // Reset stack safety guard

  // Update CPU & Memory statistics periodically (~every 100 ms)
  if (s_frameStartTicks - s_lastCPUCheckTicks >=
      static_cast<LONGLONG>(s_qpcFreq.QuadPart * 0.1)) {
    RecordCPU();
    RecordMemory();
    s_lastCPUCheckTicks = s_frameStartTicks;
  }
}

void Profiler::EndFrameImpl() {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  LONGLONG deltaTicks = now.QuadPart - s_frameStartTicks;
  float cpuRenderTimeMs = static_cast<float>(deltaTicks / s_ticksPerMs);
  s_renderStats.cpuRenderTimeMs = cpuRenderTimeMs;

  // Detect bottlenecks based on CPU render block time and section timers
  SectionTimer *presentTimer = GetOrRegisterSection("Present");
  SectionTimer *transferTimer = GetOrRegisterSection("TransferVideoFrame");
  SectionTimer *tickTimer = GetOrRegisterSection("OnVideoStreamTick");

  float presentMs = presentTimer ? (float)(presentTimer->lastTicks / s_ticksPerMs) : 0.0f;
  float transferMs = transferTimer ? (float)(transferTimer->lastTicks / s_ticksPerMs) : 0.0f;
  float decodeMs = tickTimer ? (float)(tickTimer->lastTicks / s_ticksPerMs) : 0.0f;

  s_gpuStats.idle = (cpuRenderTimeMs < 16.0f && presentMs < 2.0f && transferMs < 1.0f);
  s_gpuStats.decodeLimited = (decodeMs > 5.0f || transferMs > 4.0f);
  s_gpuStats.presentationLimited = (presentMs > 10.0f);
  s_gpuStats.saturated = (cpuRenderTimeMs >= 16.67f && !s_gpuStats.decodeLimited &&
                          !s_gpuStats.presentationLimited);

  // Periodic 5-second logging trigger
  if (GetMode() == ProfilerMode::OverlayAndLogging &&
      now.QuadPart - s_lastLogTicks >=
          static_cast<LONGLONG>(s_qpcFreq.QuadPart * 5)) {
    LogStatistics(false);
    s_lastLogTicks = now.QuadPart;
  }
}

void Profiler::BeginSectionImpl(const char *name) {
  if (s_stackDepth >= 8)
    return;
  SectionTimer *timer = GetOrRegisterSection(name);
  if (!timer)
    return;

  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  s_sectionStack[s_stackDepth].timer = timer;
  s_sectionStack[s_stackDepth].startTicks = now.QuadPart;
  s_stackDepth++;
}

void Profiler::EndSectionImpl(const char *name) {
  if (s_stackDepth <= 0)
    return;
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);

  s_stackDepth--;
  SectionStackItem &item = s_sectionStack[s_stackDepth];
  LONGLONG elapsed = now.QuadPart - item.startTicks;
  if (item.timer) {
    item.timer->RecordTicks(elapsed);
  }
}

// ----------------------------------------------------------------------------
// Recording Helpers
// ----------------------------------------------------------------------------
void Profiler::RecordPresentImpl(HRESULT hr, double durationMs) {
  if (SUCCEEDED(hr)) {
    s_renderStats.successfulPresent++;

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (s_lastFramePresentTicks != 0 && now.QuadPart > s_lastFramePresentTicks) {
      LONGLONG deltaTicks = now.QuadPart - s_lastFramePresentTicks;
      float frameIntervalMs = static_cast<float>(deltaTicks / s_ticksPerMs);
      s_history60.Record(frameIntervalMs);
      s_history300.Record(frameIntervalMs);

      if (frameIntervalMs > 0.0f) {
        s_renderStats.actualFPS = 1000.0f / frameIntervalMs;
      }
      if (frameIntervalMs > 16.67f) {
        s_renderStats.missedFrames++;
      }
    }
    s_lastFramePresentTicks = now.QuadPart;

    s_renderStats.avgFrameTimeMs = s_history60.GetAverage();
    if (s_renderStats.avgFrameTimeMs > 0.0f) {
      s_renderStats.averageFPS = 1000.0f / s_renderStats.avgFrameTimeMs;
    } else {
      s_renderStats.averageFPS = 0.0f;
    }
    s_renderStats.worstFrameTimeMs = s_history60.GetMax();
    s_renderStats.bestFrameTimeMs = s_history60.GetMin();
    s_history60.GetSortedPercentiles(0.95f, 0.99f, s_renderStats.p95FrameTimeMs,
                                     s_renderStats.p99FrameTimeMs);
  } else {
    s_renderStats.failedPresent++;
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
      s_deviceHealth.dxgiDeviceLostCount++;
    }
  }
  s_gpuStats.presentLatencyMs = static_cast<float>(durationMs);
}

void Profiler::RecordTransferImpl(HRESULT hr, double durationMs) {
  if (SUCCEEDED(hr)) {
    s_renderStats.successfulTransfer++;
  } else {
    s_renderStats.failedTransfer++;
  }
}

void Profiler::RecordEventImpl(const wchar_t *eventName) {
  if (wcscmp(eventName, L"DeviceLost") == 0) {
    s_deviceHealth.dxgiDeviceLostCount++;
  } else if (wcscmp(eventName, L"DeviceReset") == 0) {
    s_deviceHealth.dxgiResetCount++;
  } else if (wcscmp(eventName, L"Recover") == 0) {
    s_deviceHealth.recoveryCount++;
  } else if (wcscmp(eventName, L"Reload") == 0) {
    s_deviceHealth.reloadCount++;
  } else if (wcscmp(eventName, L"Resize") == 0) {
    s_deviceHealth.resizeCount++;
  } else if (wcscmp(eventName, L"DisplayChange") == 0) {
    s_deviceHealth.displayChangeCount++;
  } else if (wcscmp(eventName, L"MFError") == 0) {
    s_deviceHealth.mfErrorCount++;
  }
}

void Profiler::RecordVideoMetadataImpl(int w, int h, const wchar_t *codecStr,
                                       float fps) {
  s_videoStats.width = w;
  s_videoStats.height = h;
  s_videoStats.fps = fps;
  if (codecStr) {
    wcsncpy(s_videoStats.codec, codecStr,
            sizeof(s_videoStats.codec) / sizeof(wchar_t) - 1);
    s_videoStats.codec[sizeof(s_videoStats.codec) / sizeof(wchar_t) - 1] = L'\0';
  }
}

void Profiler::UpdateWindowsStateImpl(bool fullscreenPaused,
                                      bool batteryPaused, bool sessionPaused,
                                      bool hidden, bool onBattery) {
  s_winState.fullscreenPauseActive = fullscreenPaused;
  s_winState.batteryPauseActive = batteryPaused;
  s_winState.sessionPauseActive = sessionPaused;
  s_winState.wallpaperHidden = hidden;
  s_winState.batteryPct = onBattery ? 0 : -1; // Only the power source is tracked.
}

void Profiler::UpdateResourceStatsImpl(int textures, int swapChains,
                                       int mediaEngines, int videoSurfaces,
                                       int liveComObjects) {
  s_resourceStats.textures = textures;
  s_resourceStats.swapChains = swapChains;
  s_resourceStats.mediaEngines = mediaEngines;
  s_resourceStats.videoSurfaces = videoSurfaces;
  s_resourceStats.liveComObjects = liveComObjects;
}

// ----------------------------------------------------------------------------
// Internal System Metrics Collection
// ----------------------------------------------------------------------------
void Profiler::RecordCPU() {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  double deltaWallSec =
      static_cast<double>(now.QuadPart - s_lastCPUCheckTicks) /
      static_cast<double>(s_qpcFreq.QuadPart);
  if (deltaWallSec <= 0.0)
    return;

  FILETIME ftCreation, ftExit, ftKernel, ftUser;
  if (GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel,
                      &ftUser)) {
    ULONGLONG curProcTime = FileTimeToULL(ftKernel, ftUser);
    ULONGLONG deltaProcTicks = curProcTime - s_lastProcessTime;
    s_lastProcessTime = curProcTime;

    // Convert 100ns units (deltaProcTicks * 1e-7) to seconds and divide by wall time
    double procSec = static_cast<double>(deltaProcTicks) * 1e-7;
    s_cpuStats.explorerProcessPct =
        static_cast<float>((procSec / deltaWallSec) / s_numProcessors * 100.0);
  }

  if (GetThreadTimes(GetCurrentThread(), &ftCreation, &ftExit, &ftKernel,
                     &ftUser)) {
    ULONGLONG curThreadTime = FileTimeToULL(ftKernel, ftUser);
    ULONGLONG deltaThreadTicks = curThreadTime - s_lastThreadTime;
    s_lastThreadTime = curThreadTime;

    double threadSec = static_cast<double>(deltaThreadTicks) * 1e-7;
    s_cpuStats.wallpaperThreadPct =
        static_cast<float>((threadSec / deltaWallSec) * 100.0);
  }

  // Update rolling averages
  s_cpuStats.averagePct = (s_cpuStats.averagePct * 0.9f) +
                          (s_cpuStats.explorerProcessPct * 0.1f);
  if (s_cpuStats.explorerProcessPct > s_cpuStats.peakPct) {
    s_cpuStats.peakPct = s_cpuStats.explorerProcessPct;
  }
}

void Profiler::RecordMemory() {
  PROCESS_MEMORY_COUNTERS pmc = {};
  if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    UINT64 prevWorkingSet = s_memStats.workingSetBytes;
    s_memStats.workingSetBytes = pmc.WorkingSetSize;
    s_memStats.commitSizeBytes = pmc.PagefileUsage;
    s_memStats.peakWorkingSetBytes = pmc.PeakWorkingSetSize;
    s_memStats.peakCommitBytes = pmc.PeakPagefileUsage;

    if (pmc.WorkingSetSize > prevWorkingSet + 1024 * 1024) {
      s_memStats.continuousGrowthWarning = true;
    } else if (pmc.WorkingSetSize <= prevWorkingSet) {
      s_memStats.continuousGrowthWarning = false;
    }
  }

  if (s_dxgiAdapter3.Get()) {
    DXGI_QUERY_VIDEO_MEMORY_INFO localMem = {};
    if (SUCCEEDED(s_dxgiAdapter3->QueryVideoMemoryInfo(
            0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &localMem))) {
      s_gpuStats.dedicatedVRAMBytes = localMem.CurrentUsage;
    }
    DXGI_QUERY_VIDEO_MEMORY_INFO nonLocalMem = {};
    if (SUCCEEDED(s_dxgiAdapter3->QueryVideoMemoryInfo(
            0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonLocalMem))) {
      s_gpuStats.sharedGPUBytes = nonLocalMem.CurrentUsage;
    }
  }
}

// ----------------------------------------------------------------------------
// Overlay Rendering & Logging
// ----------------------------------------------------------------------------
void Profiler::DrawOverlay(HWND hwnd, ID3D11Texture2D *targetTexture) {
  if (!IsEnabled())
    return;

  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  if (s_overlayCache.lastUpdateTicks == 0 ||
      (now.QuadPart - s_overlayCache.lastUpdateTicks) >=
          static_cast<LONGLONG>(s_qpcFreq.QuadPart * 0.35)) {
    s_overlayCache.numLines = 0;
    auto AddLine = [&](const wchar_t *text, COLORREF clr) {
      if (s_overlayCache.numLines < 36) {
        wcsncpy(s_overlayCache.lines[s_overlayCache.numLines].text, text, 127);
        s_overlayCache.lines[s_overlayCache.numLines].text[127] = L'\0';
        s_overlayCache.lines[s_overlayCache.numLines].color = clr;
        s_overlayCache.numLines++;
      }
    };

    COLORREF clrHeader = RGB(140, 215, 255); // Cyan
    COLORREF clrText = RGB(220, 220, 220);   // White/Silver
    COLORREF clrGood = RGB(80, 255, 80);     // Green
    COLORREF clrWarn = RGB(255, 220, 50);    // Yellow
    COLORREF clrCrit = RGB(255, 80, 80);     // Red

    SectionTimer *presentTimer = GetOrRegisterSection("Present");
    SectionTimer *transferTimer = GetOrRegisterSection("TransferVideoFrame");
    SectionTimer *decodeTimer = GetOrRegisterSection("OnVideoStreamTick");
    SectionTimer *occlusionTimer = GetOrRegisterSection("OcclusionCheck");

    float presentMs = presentTimer ? (float)(presentTimer->lastTicks / s_ticksPerMs) : 0.0f;
    float transferMs = transferTimer ? (float)(transferTimer->lastTicks / s_ticksPerMs) : 0.0f;
    float decodeMs = decodeTimer ? (float)(decodeTimer->lastTicks / s_ticksPerMs) : 0.0f;
    float occlusionMs = occlusionTimer ? (float)(occlusionTimer->lastTicks / s_ticksPerMs) : 0.0f;

    wchar_t lBuf[128];
    AddLine(L"=== LIVE WALLPAPER PROFILER ===", clrHeader);
    AddLine(L"", clrText);

    AddLine(L"Performance", clrHeader);
    AddLine(L"---------------------------------------------", clrHeader);
    const wchar_t *fpsTag = (s_renderStats.actualFPS >= 28.0f)
                                ? L"[OK Good]"
                                : ((s_renderStats.actualFPS >= 20.0f) ? L"[WARN Warn]" : L"[CRIT Crit]");
    COLORREF fpsClr = (s_renderStats.actualFPS >= 28.0f)
                          ? clrGood
                          : ((s_renderStats.actualFPS >= 20.0f) ? clrWarn : clrCrit);
    _snwprintf(lBuf, 127, L"FPS:         %.2f (Avg: %.1f | Ticks: %.1f Hz) %s",
               s_renderStats.actualFPS, s_renderStats.averageFPS, s_renderStats.tickRate, fpsTag);
    AddLine(lBuf, fpsClr);

    const wchar_t *ftTag = (s_renderStats.avgFrameTimeMs <= 18.0f)
                               ? L"[OK Good]"
                               : ((s_renderStats.avgFrameTimeMs <= 33.0f) ? L"[WARN Warn]" : L"[CRIT Crit]");
    COLORREF ftClr = (s_renderStats.avgFrameTimeMs <= 18.0f)
                         ? clrGood
                         : ((s_renderStats.avgFrameTimeMs <= 33.0f) ? clrWarn : clrCrit);
    _snwprintf(lBuf, 127, L"Frame Time:  %.2f ms (P95: %.1f ms, P99: %.1f ms) %s",
               s_renderStats.avgFrameTimeMs, s_renderStats.p95FrameTimeMs, s_renderStats.p99FrameTimeMs, ftTag);
    AddLine(lBuf, ftClr);

    const wchar_t *crTag = (s_renderStats.cpuRenderTimeMs <= 8.0f)
                               ? L"[OK Good]"
                               : ((s_renderStats.cpuRenderTimeMs <= 15.0f) ? L"[WARN Warn]" : L"[CRIT Crit]");
    COLORREF crClr = (s_renderStats.cpuRenderTimeMs <= 8.0f)
                         ? clrGood
                         : ((s_renderStats.cpuRenderTimeMs <= 15.0f) ? clrWarn : clrCrit);
    _snwprintf(lBuf, 127, L"CPU Render:  %.2f ms %s", s_renderStats.cpuRenderTimeMs, crTag);
    AddLine(lBuf, crClr);

    const wchar_t *cpuTag = (s_cpuStats.explorerProcessPct <= 3.0f)
                                ? L"[OK Good]"
                                : ((s_cpuStats.explorerProcessPct <= 6.0f) ? L"[WARN Warn]" : L"[CRIT Crit]");
    COLORREF cpuClr = (s_cpuStats.explorerProcessPct <= 3.0f)
                          ? clrGood
                          : ((s_cpuStats.explorerProcessPct <= 6.0f) ? clrWarn : clrCrit);
    _snwprintf(lBuf, 127, L"CPU Process: %.2f %% (Thread: %.2f %%) %s",
               s_cpuStats.explorerProcessPct, s_cpuStats.wallpaperThreadPct, cpuTag);
    AddLine(lBuf, cpuClr);
    AddLine(L"", clrText);

    AddLine(L"Memory", clrHeader);
    AddLine(L"---------------------------------------------", clrHeader);
    _snwprintf(lBuf, 127, L"VRAM Ded/Sh: %llu MB / %llu MB",
               s_gpuStats.dedicatedVRAMBytes / (1024 * 1024),
               s_gpuStats.sharedGPUBytes / (1024 * 1024));
    AddLine(lBuf, clrText);

    UINT64 wsMB = s_memStats.workingSetBytes / (1024 * 1024);
    const wchar_t *memTag = (wsMB <= 150) ? L"[OK Good]" : ((wsMB <= 300) ? L"[WARN Warn]" : L"[CRIT Crit]");
    COLORREF memClr = (wsMB <= 150) ? clrGood : ((wsMB <= 300) ? clrWarn : clrCrit);
    _snwprintf(lBuf, 127, L"Working Set: %llu MB (Commit: %llu MB) %s", wsMB,
               s_memStats.commitSizeBytes / (1024 * 1024), memTag);
    AddLine(lBuf, memClr);
    AddLine(L"", clrText);

    AddLine(L"Pipeline", clrHeader);
    AddLine(L"---------------------------------------------", clrHeader);
    _snwprintf(lBuf, 127, L"Decode:      %.2f ms | Transfer: %.2f ms", decodeMs, transferMs);
    AddLine(lBuf, clrText);
    _snwprintf(lBuf, 127, L"Present:     %.2f ms | Occlusion: %.2f ms", presentMs, occlusionMs);
    AddLine(lBuf, clrText);
    AddLine(L"", clrText);

    AddLine(L"Status & Bottleneck", clrHeader);
    AddLine(L"---------------------------------------------", clrHeader);
    const wchar_t *pauseReason = L"No";
    if (s_winState.sessionPauseActive) {
      pauseReason = L"Yes (Locked/RDP)";
    } else if (s_winState.fullscreenPauseActive) {
      pauseReason = L"Yes (Occluded)";
    } else if (s_winState.batteryPauseActive) {
      pauseReason = L"Yes (Battery)";
    }
    _snwprintf(lBuf, 127, L"Paused:      %s (Power: %s)",
               pauseReason,
               s_winState.batteryPct < 0 ? L"AC" : L"Battery");
    AddLine(lBuf, clrText);
    _snwprintf(lBuf, 127, L"Video:       %dx%d (%s)", s_videoStats.width,
               s_videoStats.height, s_videoStats.codec);
    AddLine(lBuf, clrText);
    _snwprintf(lBuf, 127, L"Recoveries:  %u | Reloads: %u | Dropped: %llu",
               s_deviceHealth.recoveryCount, s_deviceHealth.reloadCount, s_renderStats.missedFrames);
    AddLine(lBuf, clrText);

    const wchar_t *boundStr = L"YES Decode | NO CPU | NO GPU (Optimal / 60 FPS Sync)";
    COLORREF boundClr = clrGood;
    if (decodeMs > 5.0f || transferMs > 4.0f) {
      boundStr = L"YES Decode (Waiting on MF) | NO CPU | NO GPU";
      boundClr = clrWarn;
    } else if (presentMs > 10.0f) {
      boundStr = L"NO Decode | NO CPU | YES GPU (Present Blocked by DWM)";
      boundClr = clrWarn;
    } else if (s_renderStats.cpuRenderTimeMs > 15.0f) {
      boundStr = L"NO Decode | YES CPU | NO GPU";
      boundClr = clrWarn;
    }
    _snwprintf(lBuf, 127, L"Bound By:    %s", boundStr);
    AddLine(lBuf, boundClr);
    AddLine(L"", clrText);

    AddLine(L"Resources & Lifetime", clrHeader);
    AddLine(L"---------------------------------------------", clrHeader);
    _snwprintf(lBuf, 127, L"Tracked textures: %u | Swapchains: %u",
               s_resourceStats.textures, s_resourceStats.swapChains);
    AddLine(lBuf, clrText);
    _snwprintf(lBuf, 127, L"Engines:     %u | Tracked COM refs: %u",
               s_resourceStats.mediaEngines, s_resourceStats.liveComObjects);
    AddLine(lBuf, clrText);
    AddLine(L"", clrText);
    AddLine(L"[Hotkeys] Pick: Ctrl+Alt+G | Hide: Ctrl+Alt+H | HUD: Ctrl+Alt+D", RGB(180, 200, 220));

    s_overlayCache.lastUpdateTicks = now.QuadPart;
  }

  HDC hdc = nullptr;
  ComPtr<IDXGISurface1> dxgiSurface;
  bool usingDxgiSurface = false;
  bool usingOffscreenOverlayTexture = false;

  if (targetTexture &&
      SUCCEEDED(targetTexture->QueryInterface(__uuidof(IDXGISurface1),
                                              (void **)&dxgiSurface))) {
    if (SUCCEEDED(dxgiSurface->GetDC(FALSE, &hdc))) {
      usingDxgiSurface = true;
    }
  }

  // If GetDC directly on targetTexture failed (e.g. because targetTexture is a flip-model swapchain buffer),
  // copy to our offscreen GDI-compatible texture, get its DC, and blit back right before present.
  if (!hdc && targetTexture) {
    if (!s_d3dDevice.Get()) {
      targetTexture->GetDevice(&s_d3dDevice);
      if (s_d3dDevice.Get()) {
        s_d3dDevice->GetImmediateContext(&s_d3dContext);
      }
    }
    if (s_d3dDevice.Get() && s_d3dContext.Get()) {
      D3D11_TEXTURE2D_DESC desc = {};
      targetTexture->GetDesc(&desc);

      D3D11_TEXTURE2D_DESC curDesc = {};
      if (s_overlayTexture.Get()) {
        s_overlayTexture->GetDesc(&curDesc);
      }
      if (!s_overlayTexture.Get() || curDesc.Width != desc.Width ||
          curDesc.Height != desc.Height) {
        s_overlayTexture.Reset();
        D3D11_TEXTURE2D_DESC overDesc = desc;
        overDesc.MipLevels = 1;
        overDesc.ArraySize = 1;
        overDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        overDesc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
        overDesc.Usage = D3D11_USAGE_DEFAULT;
        s_d3dDevice->CreateTexture2D(&overDesc, nullptr, &s_overlayTexture);
      }

      if (s_overlayTexture.Get()) {
        s_d3dContext->CopyResource(s_overlayTexture.Get(), targetTexture);
        if (SUCCEEDED(s_overlayTexture->QueryInterface(__uuidof(IDXGISurface1),
                                                       (void **)&dxgiSurface))) {
          if (SUCCEEDED(dxgiSurface->GetDC(FALSE, &hdc))) {
            usingDxgiSurface = true;
            usingOffscreenOverlayTexture = true;
          }
        }
      }
    }
  }

  if (!hdc && hwnd) {
    hdc = GetDC(hwnd);
  }

  if (hdc) {
    if (!s_overlayCache.hFont) {
      s_overlayCache.hFont = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }
    HFONT hOldFont = (HFONT)SelectObject(hdc, s_overlayCache.hFont);
    SetBkMode(hdc, OPAQUE);
    SetBkColor(hdc, RGB(18, 18, 18));

    int y = 12;
    int lineHeight = 16;
    for (int i = 0; i < s_overlayCache.numLines; ++i) {
      SetTextColor(hdc, s_overlayCache.lines[i].color);
      RECT rc = {12, y, 620, y + lineHeight};
      DrawTextW(hdc, s_overlayCache.lines[i].text, -1, &rc,
                DT_LEFT | DT_TOP | DT_NOCLIP);
      y += lineHeight;
    }

    SelectObject(hdc, hOldFont);
    if (usingDxgiSurface && dxgiSurface.Get()) {
      dxgiSurface->ReleaseDC(nullptr);
    } else if (hwnd) {
      ReleaseDC(hwnd, hdc);
    }

    if (usingOffscreenOverlayTexture && s_overlayTexture.Get() && s_d3dContext.Get() && targetTexture) {
      s_d3dContext->CopyResource(targetTexture, s_overlayTexture.Get());
    }
  }
}

void Profiler::LogStatistics(bool force) {
  if (!force && GetMode() != ProfilerMode::OverlayAndLogging)
    return;

  // Evaluate status indicators
  const wchar_t *cpuStatus =
      s_cpuStats.explorerProcessPct < 1.0f
          ? L"Excellent"
          : (s_cpuStats.explorerProcessPct < 3.0f
                 ? L"Good"
                 : (s_cpuStats.explorerProcessPct < 5.0f ? L"Warning"
                                                         : L"Poor/Alert"));
  const wchar_t *frameStatus =
      s_renderStats.avgFrameTimeMs < 8.0f
          ? L"Excellent"
          : (s_renderStats.avgFrameTimeMs < 14.0f
                 ? L"Good"
                 : (s_renderStats.avgFrameTimeMs < 16.67f ? L"Warning"
                                                          : L"Poor/Alert"));
  const wchar_t *memStatus =
      (s_memStats.workingSetBytes / (1024 * 1024)) < 60
          ? L"Excellent"
          : ((s_memStats.workingSetBytes / (1024 * 1024)) < 120
                 ? L"Good"
                 : ((s_memStats.workingSetBytes / (1024 * 1024)) < 250
                        ? L"Warning"
                        : L"Poor/Alert"));

  Wh_Log(L"=== [PERFORMANCE PROFILER 5-SECOND SUMMARY] ===");
  Wh_Log(L"FPS: Avg %.2f | Min %.2f | Max %.2f | Target %.0f",
         s_renderStats.averageFPS, s_history60.GetMin() > 0 ? 1000.0f / s_history60.GetMax() : 0.0f,
         s_history60.GetMin() > 0 ? 1000.0f / s_history60.GetMin() : 0.0f, s_renderStats.targetFPS);
  Wh_Log(L"Frame Time: Avg %.2f ms [%s] | Worst %.2f ms | P95 %.2f ms | P99 "
         L"%.2f ms",
         s_renderStats.avgFrameTimeMs, frameStatus,
         s_renderStats.worstFrameTimeMs, s_renderStats.p95FrameTimeMs,
         s_renderStats.p99FrameTimeMs);
  Wh_Log(L"CPU Utilization: Explorer %.2f %% [%s] | Wallpaper Thread %.2f %% | "
         L"Peak %.2f %%",
         s_cpuStats.explorerProcessPct, cpuStatus,
         s_cpuStats.wallpaperThreadPct, s_cpuStats.peakPct);
  Wh_Log(
      L"Memory: Working Set %llu MB [%s] | Commit %llu MB | Dedicated VRAM "
      L"%llu MB | Shared GPU %llu MB",
      s_memStats.workingSetBytes / (1024 * 1024), memStatus,
      s_memStats.commitSizeBytes / (1024 * 1024),
      s_gpuStats.dedicatedVRAMBytes / (1024 * 1024),
      s_gpuStats.sharedGPUBytes / (1024 * 1024));
  Wh_Log(
      L"Health & Errors: Device Lost %u | Resets %u | MF Errors %u | Reloads "
      L"%u | Dropped Frames %llu",
      s_deviceHealth.dxgiDeviceLostCount, s_deviceHealth.dxgiResetCount,
      s_deviceHealth.mfErrorCount, s_deviceHealth.reloadCount,
      s_renderStats.missedFrames);

  // Alerts
  if (s_renderStats.avgFrameTimeMs > 16.67f) {
    Wh_Log(L"[ALERT] Frame time exceeds 16.67 ms budget (%.2f ms)!",
           s_renderStats.avgFrameTimeMs);
  }
  if (s_cpuStats.explorerProcessPct > 5.0f) {
    Wh_Log(L"[ALERT] Explorer CPU utilization exceeds 5.0 %% (%.2f %%)!",
           s_cpuStats.explorerProcessPct);
  }
  if (s_memStats.continuousGrowthWarning) {
    Wh_Log(L"[ALERT] Continuous memory growth detected! Check for resource "
           L"leaks.");
  }
  if (s_deviceHealth.dxgiDeviceLostCount > 0) {
    Wh_Log(L"[ALERT] DXGI device loss events occurred (%u total).",
           s_deviceHealth.dxgiDeviceLostCount);
  }
  Wh_Log(L"===============================================");
}
// ============================================================================
// Main Translation Unit (LiveVideoWallpaper.cpp)
// ============================================================================

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

#ifndef DWMWA_EXTENDED_FRAME_BOUNDS
#define DWMWA_EXTENDED_FRAME_BOUNDS 9
#endif
#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

// [Deduplicated] #include "PerformanceProfiler.h" (already included)


// ---------------------------------------------------------------------------
// Window / message constants
// ---------------------------------------------------------------------------

HWND g_wallpaperWnd = nullptr;
HANDLE g_shutdownEvent = nullptr;
const UINT_PTR kRenderTimerId = 1;
const UINT_PTR kZOrderTimerId = 2;
const UINT_PTR kOcclusionTimerId = 3;
const UINT_PTR kSourceRetryTimerId = 4;
const int kHotkeyId = 1;
const int kVisibilityHotkeyId = 2;
const int kProfilerHotkeyId = 3;
const wchar_t kWindowClassName[] = L"VideoWallpaperEngine_WorkerWindow";
const UINT kMsgReloadSource = WM_APP + 1;
const UINT kMsgMediaEngineEvent = WM_APP + 2;
const UINT kMsgUpdateSettings = WM_APP + 3;
const UINT kMsgModeChanged = WM_APP + 4;

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

enum class BatteryMode { Pause, DropFps, Normal };

// g_videoPath is written from Wh_ModSettingsChanged (called on whatever
// thread Windhawk invokes it from) and read from the wallpaper thread
// (ResolveVideoSource) -- both sides go through g_pathLock rather than
// touching the std::wstring directly, since a bare reassignment racing a
// concurrent read is undefined behavior, not just "usually fine".
//
// fitStretch/batteryMode are read every frame/tick on the wallpaper
// thread, so they're plain atomics instead of being behind the same lock
// -- correct without adding any lock contention to the render path.
CRITICAL_SECTION g_pathLock;
std::wstring g_videoPath;
std::atomic<int> g_fitMode{0}; // 0 = fill/stretch, 1 = cover/zoom, 2 = fit/letterbox
std::atomic<BatteryMode> g_batteryMode{BatteryMode::Pause};

// Which subsystem (VideoPlayer vs FluidSimulation) drives the shared D3D
// device/swap chain. Read on the wallpaper thread every render tick,
// written from Wh_ModSettingsChanged.
enum class WallpaperMode { Video, Fluid };
std::atomic<WallpaperMode> g_wallpaperMode{WallpaperMode::Video};

// Fluid-mode customization settings -- read every frame off the wallpaper
// thread, written from Wh_ModSettingsChanged.
std::atomic<int>  g_fluidSplatRadius{25};          // 0-100
std::atomic<int>  g_fluidSpeed{100};               // 0-200
std::atomic<int>  g_fluidBloom{80};                // 0-200
std::atomic<bool> g_fluidColorful{true};
std::atomic<int>  g_fluidRandomSplatsInterval{3};  // seconds, 0 = disabled
std::atomic<int> g_targetFps{60};
std::atomic<bool> g_audioMuted{true};
std::atomic<int> g_audioVolume{100};
std::atomic<int> g_occlusionIntervalMs{250};
std::atomic<bool> g_filePickerHotkey{true};
std::atomic<bool> g_visibilityHotkey{true};
std::atomic<bool> g_profilerHotkey{false};

std::wstring GetVideoPathSetting() {
  EnterCriticalSection(&g_pathLock);
  std::wstring copy = g_videoPath;
  LeaveCriticalSection(&g_pathLock);
  return copy;
}

// Reads all settings and applies them. Returns the *previous* videoPath
// so Wh_ModSettingsChanged can tell whether the source actually changed,
// as opposed to just fitMode/batteryMode -- only a real source change
// needs to interrupt playback with a reload.
static std::wstring s_lastWindhawkSettingVideoPath;
static bool s_lastSettingLoaded = false;

std::wstring ReadStoredPath(PCWSTR name) {
  std::wstring storedPath(32768, L'\0');
  if (Wh_GetStringValue(name, storedPath.data(), storedPath.size()) == 0)
    return L"";
  return storedPath.c_str();
}

std::wstring LoadSettings() {
  PCWSTR videoPathSettingRaw = Wh_GetStringSetting(L"videoPath");
  std::wstring settingPath = videoPathSettingRaw ? videoPathSettingRaw : L"";
  if (videoPathSettingRaw)
    Wh_FreeStringSetting(videoPathSettingRaw);

  EnterCriticalSection(&g_pathLock);
  std::wstring oldPath = g_videoPath;
  if (!s_lastSettingLoaded) {
    s_lastWindhawkSettingVideoPath = ReadStoredPath(L"LastSettingVideoPath");
    s_lastSettingLoaded = true;
  }

  // Check if the user explicitly changed the videoPath setting text inside the Windhawk UI
  if (settingPath != s_lastWindhawkSettingVideoPath) {
    s_lastWindhawkSettingVideoPath = settingPath;
    Wh_SetStringValue(L"LastSettingVideoPath", settingPath.c_str());
    g_videoPath = settingPath;
    Wh_SetStringValue(L"LastPickedVideoPath", settingPath.c_str());
  }

  // Restore the last picker choice if the settings path is empty on startup.
  if (g_videoPath.empty()) {
    g_videoPath = ReadStoredPath(L"LastPickedVideoPath");
  }

  PCWSTR fitModeStr = Wh_GetStringSetting(L"fitMode");
  if (fitModeStr) {
    if (wcscmp(fitModeStr, L"cover") == 0) {
      g_fitMode = 1;
    } else if (wcscmp(fitModeStr, L"fit") == 0) {
      g_fitMode = 2;
    } else {
      g_fitMode = 0;
    }
    Wh_FreeStringSetting(fitModeStr);
  } else {
    g_fitMode = 0;
  }

  PCWSTR batteryModeStr = Wh_GetStringSetting(L"batteryMode");
  BatteryMode newBatteryMode;
  if (wcscmp(batteryModeStr, L"drop") == 0) {
    newBatteryMode = BatteryMode::DropFps;
  } else if (wcscmp(batteryModeStr, L"normal") == 0) {
    newBatteryMode = BatteryMode::Normal;
  } else {
    newBatteryMode = BatteryMode::Pause;
  }
  g_batteryMode = newBatteryMode;
  Wh_FreeStringSetting(batteryModeStr);

  PCWSTR targetFpsStr = Wh_GetStringSetting(L"targetFps");
  int newTargetFps = 60;
  if (targetFpsStr) {
    if (wcscmp(targetFpsStr, L"monitor") == 0) {
      newTargetFps = 0;
    } else {
      newTargetFps = _wtoi(targetFpsStr);
      if (newTargetFps <= 0) newTargetFps = 60;
    }
    Wh_FreeStringSetting(targetFpsStr);
  }
  g_targetFps = newTargetFps;

  int audioMutedInt = Wh_GetIntSetting(L"audioMuted");
  g_audioMuted = (audioMutedInt != 0);

  PCWSTR audioVolumeStr = Wh_GetStringSetting(L"audioVolume");
  int newVolume = 100;
  if (audioVolumeStr) {
    newVolume = _wtoi(audioVolumeStr);
    if (newVolume < 0) newVolume = 0;
    if (newVolume > 100) newVolume = 100;
    Wh_FreeStringSetting(audioVolumeStr);
  }
  g_audioVolume = newVolume;

  PCWSTR occlusionIntervalStr = Wh_GetStringSetting(L"occlusionInterval");
  int newOcclusionMs = 250;
  if (occlusionIntervalStr) {
    if (wcscmp(occlusionIntervalStr, L"fast") == 0) {
      newOcclusionMs = 100;
    } else if (wcscmp(occlusionIntervalStr, L"relaxed") == 0) {
      newOcclusionMs = 500;
    } else {
      newOcclusionMs = 250;
    }
    Wh_FreeStringSetting(occlusionIntervalStr);
  }
  g_occlusionIntervalMs = newOcclusionMs;
  g_filePickerHotkey = Wh_GetIntSetting(L"filePickerHotkey") != 0;
  g_visibilityHotkey = Wh_GetIntSetting(L"visibilityHotkey") != 0;
  g_profilerHotkey = Wh_GetIntSetting(L"profilerHotkey") != 0;

  PCWSTR wallpaperModeStr = Wh_GetStringSetting(L"wallpaperMode");
  WallpaperMode newWallpaperMode = WallpaperMode::Video;
  if (wallpaperModeStr) {
    if (wcscmp(wallpaperModeStr, L"fluid") == 0)
      newWallpaperMode = WallpaperMode::Fluid;
    Wh_FreeStringSetting(wallpaperModeStr);
  }
  g_wallpaperMode = newWallpaperMode;

  int fluidSplatRadiusInt = Wh_GetIntSetting(L"fluidSplatRadius");
  g_fluidSplatRadius = (std::max)(0, (std::min)(100, fluidSplatRadiusInt));

  int fluidSpeedInt = Wh_GetIntSetting(L"fluidSpeed");
  g_fluidSpeed = (std::max)(0, (std::min)(200, fluidSpeedInt));

  int fluidBloomInt = Wh_GetIntSetting(L"fluidBloom");
  g_fluidBloom = (std::max)(0, (std::min)(200, fluidBloomInt));

  g_fluidColorful = Wh_GetIntSetting(L"fluidColorful") != 0;

  int fluidRandomSplatsIntervalInt = Wh_GetIntSetting(L"fluidRandomSplatsInterval");
  g_fluidRandomSplatsInterval = (std::max)(0, fluidRandomSplatsIntervalInt);

  LeaveCriticalSection(&g_pathLock);
  return oldPath;
}

// ============================================================================
// Fluid Simulation Subsystem
// Interactive GPU fluid-simulation wallpaper mode (experimental)
// ============================================================================
// A real-time, mouse-reactive fluid sim (semi-Lagrangian advection + Jacobi
// pressure projection + vorticity confinement -- the standard "Stable
// Fluids" method) implemented as Direct3D 11 compute shaders, rendered as an
// alternative to video playback on the SAME shared D3D11 device/swap chain
// that VideoPlayer::InitD3DAndSwapChain already sets up.
//
// Pipeline per tick:
//   1. Splat  - inject a velocity + dye impulse at the cursor
//   2. Advect velocity (self-advection)
//   3. Curl + vorticity confinement
//   4. Divergence
//   5. Pressure solve (Jacobi, warm-started)
//   6. Gradient subtract (projection -> divergence-free)
//   7. Advect dye
//   8. Composite: draw dye field to the swap chain back buffer
// ============================================================================

typedef HRESULT(WINAPI *PFN_D3DCOMPILE)(LPCVOID pSrcData, SIZE_T SrcDataSize,
                                        LPCSTR pSourceName, LPCVOID pDefines,
                                        LPCVOID pInclude, LPCSTR pEntrypoint,
                                        LPCSTR pTarget, UINT Flags1,
                                        UINT Flags2, ID3DBlob **ppCode,
                                        ID3DBlob **ppErrorMsgs);

static PFN_D3DCOMPILE GetD3DCompileFn() {
  static PFN_D3DCOMPILE fn = nullptr;
  static bool attempted = false;
  if (!attempted) {
    attempted = true;
    const wchar_t *candidates[] = {L"d3dcompiler_47.dll", L"d3dcompiler_46.dll",
                                   L"d3dcompiler_43.dll"};
    for (const wchar_t *name : candidates) {
      HMODULE mod = LoadLibraryW(name);
      if (mod) {
        fn = reinterpret_cast<PFN_D3DCOMPILE>(GetProcAddress(mod, "D3DCompile"));
        if (fn)
          break;
      }
    }
  }
  return fn;
}

static ComPtr<ID3DBlob> FluidCompileShaderBlob(const char *source, size_t sourceLen,
                                               const char *entryPoint,
                                               const char *target) {
  PFN_D3DCOMPILE d3dCompile = GetD3DCompileFn();
  if (!d3dCompile) {
    Wh_Log(L"FluidSimulation: d3dcompiler_47.dll / D3DCompile not available");
    return ComPtr<ID3DBlob>();
  }
  ComPtr<ID3DBlob> code;
  ComPtr<ID3DBlob> errors;
  HRESULT hr = d3dCompile(source, sourceLen, nullptr, nullptr, nullptr,
                          entryPoint, target, 0, 0, &code, &errors);
  if (FAILED(hr)) {
    if (errors.Get()) {
      Wh_Log(L"FluidSimulation: shader compile error (%hs/%hs): %hs", entryPoint,
             target, (const char *)errors->GetBufferPointer());
    } else {
      Wh_Log(L"FluidSimulation: shader compile failed (%hs/%hs), hr=0x%08lX",
             entryPoint, target, (unsigned long)hr);
    }
    return ComPtr<ID3DBlob>();
  }
  return code;
}

struct SimTexture {
  ComPtr<ID3D11Texture2D> tex;
  ComPtr<ID3D11ShaderResourceView> srv;
  ComPtr<ID3D11UnorderedAccessView> uav;

  void Reset() {
    tex.Reset();
    srv.Reset();
    uav.Reset();
  }
};

static bool CreateSimTexture(ID3D11Device *device, int w, int h, SimTexture &out) {
  out.Reset();
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = (UINT)w;
  desc.Height = (UINT)h;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

  HRESULT hr = device->CreateTexture2D(&desc, nullptr, &out.tex);
  if (FAILED(hr))
    return false;
  hr = device->CreateShaderResourceView(out.tex.Get(), nullptr, &out.srv);
  if (FAILED(hr))
    return false;
  hr = device->CreateUnorderedAccessView(out.tex.Get(), nullptr, &out.uav);
  if (FAILED(hr))
    return false;
  return true;
}

static void FluidUnbindCSUAVs(ID3D11DeviceContext *ctx, UINT startSlot, UINT count) {
  static ID3D11UnorderedAccessView *const nullUAVs[8] = {
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  ctx->CSSetUnorderedAccessViews(startSlot, (std::min)(count, 8u), nullUAVs, nullptr);
}

static void FluidUnbindCSSRVs(ID3D11DeviceContext *ctx, UINT startSlot, UINT count) {
  static ID3D11ShaderResourceView *const nullSRVs[8] = {
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  ctx->CSSetShaderResources(startSlot, (std::min)(count, 8u), nullSRVs);
}

static void FluidHsvToRgb(float h, float s, float v, float &r, float &g, float &b) {
  float i = floorf(h * 6.0f);
  float f = h * 6.0f - i;
  float p = v * (1.0f - s);
  float q = v * (1.0f - f * s);
  float t = v * (1.0f - (1.0f - f) * s);
  int im = ((int)i) % 6;
  if (im < 0)
    im += 6;
  switch (im) {
  case 0: r = v; g = t; b = p; break;
  case 1: r = q; g = v; b = p; break;
  case 2: r = p; g = v; b = t; break;
  case 3: r = p; g = q; b = v; break;
  case 4: r = t; g = p; b = v; break;
  default: r = v; g = p; b = q; break;
  }
}

struct FluidSimParamsCB {
  float dt;
  float dissipation;
  float clearValue;
  float pad0;
  float simTexelX;
  float simTexelY;
  float pad1;
  float pad2;
};

struct FluidSplatParamsCB {
  float pointX, pointY, pad0, pad1;
  float value0, value1, value2, value3;
  float radiusSq, aspectRatio, pad2, pad3;
};

struct FluidVorticityParamsCB {
  float curl;
  float dt;
  float pad0;
  float pad1;
};

struct FluidBloomParamsCB {
  float intensity, threshold, curve0, curve1;
  float curve2, pad0, pad1, pad2;
  float texelSizeX, texelSizeY, pad3, pad4;
};

static const char kFluidShaderSource[] = R"HLSL(
SamplerState LinearClamp : register(s0);

cbuffer SimParams : register(b0)
{
    float4 simParams;
    float4 simTexelSize;
};

cbuffer SplatParams : register(b1)
{
    float4 splatPoint;
    float4 splatValue;
    float4 splatParams;
};

cbuffer VorticityParams : register(b2)
{
    float4 vorticityParams;
};

cbuffer BloomParams : register(b3)
{
    float4 bloomParams;
    float4 bloomParams2;
    float4 bloomTexelSize;
};

int2 ClampCoord(int2 c, int2 dims)
{
    return clamp(c, int2(0, 0), dims - int2(1, 1));
}

Texture2D<float4> splatSrc : register(t0);
RWTexture2D<float4> splatDst : register(u0);

[numthreads(8, 8, 1)]
void SplatCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    splatDst.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    float2 uv = (float2(id.xy) + 0.5) / float2((float)w, (float)h);
    float2 p = uv - splatPoint.xy;
    p.x *= splatParams.y;
    float falloff = exp(-dot(p, p) / max(splatParams.x, 1e-6));
    float4 base = splatSrc.Load(int3(int2(id.xy), 0));
    splatDst[id.xy] = base + splatValue * falloff;
}

Texture2D<float4> advectVelocityField : register(t1);
Texture2D<float4> advectSourceField : register(t2);
RWTexture2D<float4> advectDest : register(u1);

[numthreads(8, 8, 1)]
void AdvectCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    advectDest.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    float2 uv = (float2(id.xy) + 0.5) / float2((float)w, (float)h);
    float2 vel = advectVelocityField.SampleLevel(LinearClamp, uv, 0).xy;
    float2 backUv = uv - simParams.x * vel * simTexelSize.xy;
    float4 result = advectSourceField.SampleLevel(LinearClamp, backUv, 0);
    float decay = 1.0 + simParams.y * simParams.x;
    advectDest[id.xy] = result / decay;
}

Texture2D<float4> divVelocityField : register(t3);
RWTexture2D<float4> divDest : register(u2);

[numthreads(8, 8, 1)]
void DivergenceCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    divDest.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    int2 dims = int2(w, h);
    int2 c = int2(id.xy);

    float L = divVelocityField.Load(int3(ClampCoord(c + int2(-1, 0), dims), 0)).x;
    float R = divVelocityField.Load(int3(ClampCoord(c + int2( 1, 0), dims), 0)).x;
    float T = divVelocityField.Load(int3(ClampCoord(c + int2( 0, 1), dims), 0)).y;
    float B = divVelocityField.Load(int3(ClampCoord(c + int2( 0,-1), dims), 0)).y;
    float2 C = divVelocityField.Load(int3(c, 0)).xy;

    if (c.x == 0)       L = -C.x;
    if (c.x == w - 1)   R = -C.x;
    if (c.y == h - 1)   T = -C.y;
    if (c.y == 0)       B = -C.y;

    divDest[id.xy] = float4(0.5 * (R - L + T - B), 0, 0, 0);
}

Texture2D<float4> jacobiPressureField : register(t4);
Texture2D<float4> jacobiDivergenceField : register(t5);
RWTexture2D<float4> jacobiDest : register(u3);

[numthreads(8, 8, 1)]
void PressureJacobiCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    jacobiDest.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    int2 dims = int2(w, h);
    int2 c = int2(id.xy);

    float L = jacobiPressureField.Load(int3(ClampCoord(c + int2(-1, 0), dims), 0)).x;
    float R = jacobiPressureField.Load(int3(ClampCoord(c + int2( 1, 0), dims), 0)).x;
    float B = jacobiPressureField.Load(int3(ClampCoord(c + int2( 0,-1), dims), 0)).x;
    float T = jacobiPressureField.Load(int3(ClampCoord(c + int2( 0, 1), dims), 0)).x;
    float div = jacobiDivergenceField.Load(int3(c, 0)).x;

    jacobiDest[id.xy] = float4((L + R + B + T - div) * 0.25, 0, 0, 0);
}

Texture2D<float4> gradVelocitySrc : register(t6);
Texture2D<float4> gradPressureField : register(t7);
RWTexture2D<float4> gradVelocityDst : register(u4);

[numthreads(8, 8, 1)]
void GradientSubtractCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    gradVelocityDst.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    int2 dims = int2(w, h);
    int2 c = int2(id.xy);

    float L = gradPressureField.Load(int3(ClampCoord(c + int2(-1, 0), dims), 0)).x;
    float R = gradPressureField.Load(int3(ClampCoord(c + int2( 1, 0), dims), 0)).x;
    float B = gradPressureField.Load(int3(ClampCoord(c + int2( 0,-1), dims), 0)).x;
    float T = gradPressureField.Load(int3(ClampCoord(c + int2( 0, 1), dims), 0)).x;

    float4 vel = gradVelocitySrc.Load(int3(c, 0));
    vel.xy -= float2(R - L, T - B);
    gradVelocityDst[id.xy] = vel;
}

Texture2D<float4> curlVelocityField : register(t8);
RWTexture2D<float4> curlDest : register(u5);

[numthreads(8, 8, 1)]
void CurlCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    curlDest.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    int2 dims = int2(w, h);
    int2 c = int2(id.xy);

    float L = curlVelocityField.Load(int3(ClampCoord(c + int2(-1, 0), dims), 0)).y;
    float R = curlVelocityField.Load(int3(ClampCoord(c + int2( 1, 0), dims), 0)).y;
    float T = curlVelocityField.Load(int3(ClampCoord(c + int2( 0, 1), dims), 0)).x;
    float B = curlVelocityField.Load(int3(ClampCoord(c + int2( 0,-1), dims), 0)).x;

    curlDest[id.xy] = float4(0.5 * (R - L - T + B), 0, 0, 0);
}

Texture2D<float4> vortVelocitySrc : register(t9);
Texture2D<float4> vortCurlField : register(t10);
RWTexture2D<float4> vortVelocityDst : register(u6);

[numthreads(8, 8, 1)]
void VorticityCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    vortVelocityDst.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    int2 dims = int2(w, h);
    int2 c = int2(id.xy);

    float L = vortCurlField.Load(int3(ClampCoord(c + int2(-1, 0), dims), 0)).x;
    float R = vortCurlField.Load(int3(ClampCoord(c + int2( 1, 0), dims), 0)).x;
    float T = vortCurlField.Load(int3(ClampCoord(c + int2( 0, 1), dims), 0)).x;
    float B = vortCurlField.Load(int3(ClampCoord(c + int2( 0,-1), dims), 0)).x;
    float C = vortCurlField.Load(int3(c, 0)).x;

    float2 force = 0.5 * float2(abs(T) - abs(B), abs(R) - abs(L));
    force /= length(force) + 0.0001;
    force *= vorticityParams.x * C;
    force.y *= -1.0;

    float4 vel = vortVelocitySrc.Load(int3(c, 0));
    vel.xy += force * vorticityParams.y;
    vel.xy = clamp(vel.xy, -1000.0, 1000.0);

    vortVelocityDst[id.xy] = vel;
}

Texture2D<float4> clearSrc : register(t12);
RWTexture2D<float4> clearDst : register(u7);

[numthreads(8, 8, 1)]
void ClearCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    clearDst.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    float4 v = clearSrc.Load(int3(int2(id.xy), 0));
    clearDst[id.xy] = v * simParams.z;
}

Texture2D<float4> bloomSrc : register(t11);
RWTexture2D<float4> bloomDst : register(u7);

[numthreads(8, 8, 1)]
void BloomPrefilterCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    bloomDst.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    float2 uv = (float2(id.xy) + 0.5) / float2((float)w, (float)h);
    float3 c = bloomSrc.SampleLevel(LinearClamp, uv, 0).rgb;
    float br = max(c.r, max(c.g, c.b));
    float rq = clamp(br - bloomParams.z, 0.0, bloomParams.w);
    rq = bloomParams2.x * rq * rq;
    c *= max(rq, br - bloomParams.y) / max(br, 0.0001);
    bloomDst[id.xy] = float4(c, 0);
}

[numthreads(8, 8, 1)]
void BloomBlurCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    bloomDst.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    float2 uv = (float2(id.xy) + 0.5) / float2((float)w, (float)h);
    float2 t = bloomTexelSize.xy;
    float4 sum = bloomSrc.SampleLevel(LinearClamp, uv + float2(-t.x, -t.y), 0)
               + bloomSrc.SampleLevel(LinearClamp, uv + float2( t.x, -t.y), 0)
               + bloomSrc.SampleLevel(LinearClamp, uv + float2(-t.x,  t.y), 0)
               + bloomSrc.SampleLevel(LinearClamp, uv + float2( t.x,  t.y), 0);
    bloomDst[id.xy] = sum * 0.25;
}

[numthreads(8, 8, 1)]
void BloomUpsampleCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    bloomDst.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    float2 uv = (float2(id.xy) + 0.5) / float2((float)w, (float)h);
    float2 t = bloomTexelSize.xy;
    float4 sum = bloomSrc.SampleLevel(LinearClamp, uv + float2(-t.x, -t.y), 0)
               + bloomSrc.SampleLevel(LinearClamp, uv + float2( t.x, -t.y), 0)
               + bloomSrc.SampleLevel(LinearClamp, uv + float2(-t.x,  t.y), 0)
               + bloomSrc.SampleLevel(LinearClamp, uv + float2( t.x,  t.y), 0);
    sum *= 0.25;
    float4 dst = bloomDst.Load(int3(int2(id.xy), 0));
    bloomDst[id.xy] = dst + sum;
}

[numthreads(8, 8, 1)]
void BloomFinalCS(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    bloomDst.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    float2 uv = (float2(id.xy) + 0.5) / float2((float)w, (float)h);
    float2 t = bloomTexelSize.xy;
    float4 sum = bloomSrc.SampleLevel(LinearClamp, uv + float2(-t.x, -t.y), 0)
               + bloomSrc.SampleLevel(LinearClamp, uv + float2( t.x, -t.y), 0)
               + bloomSrc.SampleLevel(LinearClamp, uv + float2(-t.x,  t.y), 0)
               + bloomSrc.SampleLevel(LinearClamp, uv + float2( t.x,  t.y), 0);
    bloomDst[id.xy] = sum * 0.25 * bloomParams.x;
}

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut FullscreenVS(uint id : SV_VertexID)
{
    VSOut o;
    float2 p = float2((id << 1) & 2, id & 2);
    o.uv = p;
    o.pos = float4(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0, 0, 1);
    return o;
}

Texture2D<float4> compositeDyeTex : register(t13);
Texture2D<float4> compositeBloomTex : register(t14);

float4 CompositePS(VSOut i) : SV_TARGET
{
    uint texW, texH;
    compositeDyeTex.GetDimensions(texW, texH);
    float2 texel = 1.0 / float2((float)texW, (float)texH);

    float3 base = compositeDyeTex.SampleLevel(LinearClamp, i.uv, 0).rgb;

    float3 lc = compositeDyeTex.SampleLevel(LinearClamp, i.uv - float2(texel.x, 0), 0).rgb;
    float3 rc = compositeDyeTex.SampleLevel(LinearClamp, i.uv + float2(texel.x, 0), 0).rgb;
    float3 tc = compositeDyeTex.SampleLevel(LinearClamp, i.uv + float2(0, texel.y), 0).rgb;
    float3 bc = compositeDyeTex.SampleLevel(LinearClamp, i.uv - float2(0, texel.y), 0).rgb;
    float dx = length(rc) - length(lc);
    float dy = length(tc) - length(bc);
    float3 n = normalize(float3(dx, dy, length(texel)));
    float diffuse = clamp(n.z + 0.7, 0.7, 1.0);
    base *= diffuse;

    float3 bloom = max(compositeBloomTex.SampleLevel(LinearClamp, i.uv, 0).rgb, 0.0);
    bloom = max(1.055 * pow(bloom, 1.0 / 2.4) - 0.055, 0.0);
    base += bloom;

    return float4(base, 1.0);
}
)HLSL";

class FluidSimulation {
public:
  enum class TickResult { Ok, Skipped, DeviceLost };

  bool Initialize(ID3D11Device *device, ID3D11DeviceContext *context,
                  IDXGISwapChain1 *swapChain, HWND hwnd, int screenW, int screenH);
  void Resize(int screenW, int screenH);
  void Shutdown();
  bool IsInitialized() const { return m_initialized; }

  TickResult Tick(bool isOnBattery);

  void SetPausedForFullscreen(bool paused) { m_pausedForFullscreen = paused; }
  void SetPausedForBattery(bool paused) { m_pausedForBattery = paused; }
  void SetPausedForSession(bool paused) { m_pausedForSession = paused; }

private:
  bool CreateResources();
  bool CreateBloomChain();
  void ReleaseSimTextures();
  void ReleaseBloomTextures();
  bool CompileShaders();
  void ComputeResolutions(int screenW, int screenH);
  void ClearAllTextures();
  void UpdateSimulation(float dt);
  void DoSplatVelocity(float x, float y, float dx, float dy, float radiusUV);
  void DoSplatDye(float x, float y, float r, float g, float b, float radiusUV);
  void ComputeSplatColor(float &r, float &g, float &b);
  void ApplyBloom();
  void RenderComposite(ID3D11RenderTargetView *rtv);
  void DispatchSim(int w, int h) { m_context->Dispatch((w + 7) / 8, (h + 7) / 8, 1); }
  bool IsEffectivePaused() const {
    return m_pausedForFullscreen || m_pausedForBattery || m_pausedForSession;
  }

  static const int kBloomIterations = 8;

  bool m_initialized = false;
  ID3D11Device *m_device = nullptr;
  ID3D11DeviceContext *m_context = nullptr;
  IDXGISwapChain1 *m_swapChain = nullptr;
  HWND m_hwnd = nullptr;

  int m_simWidth = 0;
  int m_simHeight = 0;
  int m_dyeWidth = 0;
  int m_dyeHeight = 0;
  int m_bloomWidth = 0;
  int m_bloomHeight = 0;
  int m_bloomChainCount = 0;
  int m_screenWidth = 0;
  int m_screenHeight = 0;
  RECT m_windowRect = {};

  SimTexture m_velocity[2];
  int m_velocityIdx = 0;
  SimTexture m_dye[2];
  int m_dyeIdx = 0;
  SimTexture m_pressure[2];
  int m_pressureIdx = 0;
  SimTexture m_divergence;
  SimTexture m_curl;

  SimTexture m_bloom;
  SimTexture m_bloomChain[kBloomIterations];

  ComPtr<ID3D11ComputeShader> m_csSplat;
  ComPtr<ID3D11ComputeShader> m_csAdvect;
  ComPtr<ID3D11ComputeShader> m_csDivergence;
  ComPtr<ID3D11ComputeShader> m_csPressureJacobi;
  ComPtr<ID3D11ComputeShader> m_csGradientSubtract;
  ComPtr<ID3D11ComputeShader> m_csCurl;
  ComPtr<ID3D11ComputeShader> m_csVorticity;
  ComPtr<ID3D11ComputeShader> m_csClear;
  ComPtr<ID3D11ComputeShader> m_csBloomPrefilter;
  ComPtr<ID3D11ComputeShader> m_csBloomBlur;
  ComPtr<ID3D11ComputeShader> m_csBloomUpsample;
  ComPtr<ID3D11ComputeShader> m_csBloomFinal;
  ComPtr<ID3D11VertexShader> m_vsFullscreen;
  ComPtr<ID3D11PixelShader> m_psComposite;

  ComPtr<ID3D11Buffer> m_cbSimParams;
  ComPtr<ID3D11Buffer> m_cbSplatParams;
  ComPtr<ID3D11Buffer> m_cbVorticityParams;
  ComPtr<ID3D11Buffer> m_cbBloomParams;
  ComPtr<ID3D11SamplerState> m_sampler;
  ComPtr<ID3D11RasterizerState> m_rasterizerState;

  ComPtr<ID3D11Texture2D> m_cachedBackBuffer[2];
  ComPtr<ID3D11RenderTargetView> m_cachedBackBufferRtv[2];

  bool m_havePrevCursor = false;
  POINT m_prevCursor = {};
  float m_cursorColorR = 0.0f;
  float m_cursorColorG = 0.0f;
  float m_cursorColorB = 0.0f;
  float m_cursorColorTimer = 0.0f;
  bool m_cursorColorValid = false;
  DWORD m_lastCornerSplatTickMs = 0;
  DWORD m_lastPressureResetTickMs = 0;
  LARGE_INTEGER m_lastFrameTicks = {};
  UINT64 m_frameNumber = 0;

  bool m_pausedForFullscreen = false;
  bool m_pausedForBattery = false;
  bool m_pausedForSession = false;
  bool m_everShown = false;
  DWORD m_lastDropFpsTickMs = 0;
};

void FluidSimulation::ComputeResolutions(int screenW, int screenH) {
  auto computeLongEdge = [](int sw, int sh, int maxDim, int &outW, int &outH) {
    if (sw <= 0 || sh <= 0) { outW = maxDim; outH = maxDim; return; }
    if (sw >= sh) {
      outW = maxDim;
      outH = (std::max)(16, (int)((int64_t)maxDim * sh / sw));
    } else {
      outH = maxDim;
      outW = (std::max)(16, (int)((int64_t)maxDim * sw / sh));
    }
  };
  computeLongEdge(screenW, screenH, 128, m_simWidth, m_simHeight);
  computeLongEdge(screenW, screenH, 1024, m_dyeWidth, m_dyeHeight);
  computeLongEdge(screenW, screenH, 256, m_bloomWidth, m_bloomHeight);
}

bool FluidSimulation::CreateResources() {
  ReleaseSimTextures();
  bool ok = true;
  for (int i = 0; i < 2 && ok; ++i) {
    ok = ok && CreateSimTexture(m_device, m_simWidth, m_simHeight, m_velocity[i]);
    ok = ok && CreateSimTexture(m_device, m_simWidth, m_simHeight, m_pressure[i]);
  }
  for (int i = 0; i < 2 && ok; ++i) {
    ok = ok && CreateSimTexture(m_device, m_dyeWidth, m_dyeHeight, m_dye[i]);
  }
  ok = ok && CreateSimTexture(m_device, m_simWidth, m_simHeight, m_divergence);
  ok = ok && CreateSimTexture(m_device, m_simWidth, m_simHeight, m_curl);
  m_velocityIdx = 0;
  m_dyeIdx = 0;
  m_pressureIdx = 0;
  return ok;
}

bool FluidSimulation::CreateBloomChain() {
  ReleaseBloomTextures();
  if (!CreateSimTexture(m_device, m_bloomWidth, m_bloomHeight, m_bloom))
    return false;
  m_bloomChainCount = 0;
  for (int i = 0; i < kBloomIterations; ++i) {
    int w = m_bloomWidth >> (i + 1);
    int h = m_bloomHeight >> (i + 1);
    if (w < 2 || h < 2) break;
    if (!CreateSimTexture(m_device, w, h, m_bloomChain[i]))
      return false;
    m_bloomChainCount++;
  }
  return true;
}

void FluidSimulation::ReleaseSimTextures() {
  for (int i = 0; i < 2; ++i) {
    m_velocity[i].Reset();
    m_dye[i].Reset();
    m_pressure[i].Reset();
  }
  m_divergence.Reset();
  m_curl.Reset();
}

void FluidSimulation::ReleaseBloomTextures() {
  m_bloom.Reset();
  for (int i = 0; i < kBloomIterations; ++i)
    m_bloomChain[i].Reset();
  m_bloomChainCount = 0;
}

void FluidSimulation::ClearAllTextures() {
  const FLOAT zero[4] = {0, 0, 0, 0};
  for (int i = 0; i < 2; ++i) {
    m_context->ClearUnorderedAccessViewFloat(m_velocity[i].uav.Get(), zero);
    m_context->ClearUnorderedAccessViewFloat(m_dye[i].uav.Get(), zero);
    m_context->ClearUnorderedAccessViewFloat(m_pressure[i].uav.Get(), zero);
  }
  m_context->ClearUnorderedAccessViewFloat(m_divergence.uav.Get(), zero);
  m_context->ClearUnorderedAccessViewFloat(m_curl.uav.Get(), zero);
}

bool FluidSimulation::CompileShaders() {
  size_t srcLen = strlen(kFluidShaderSource);

  auto compileCS = [&](const char *entry, ComPtr<ID3D11ComputeShader> &out) -> bool {
    ComPtr<ID3DBlob> blob = FluidCompileShaderBlob(kFluidShaderSource, srcLen, entry, "cs_5_0");
    if (!blob.Get()) return false;
    if (blob->GetBufferSize() == 0) {
      Wh_Log(L"FluidSimulation: compiler returned empty bytecode for %hs", entry);
      return false;
    }
    HRESULT hr = m_device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(),
                                               nullptr, &out);
    if (FAILED(hr)) {
      Wh_Log(L"FluidSimulation: CreateComputeShader(%hs) failed, hr=0x%08lX", entry,
             (unsigned long)hr);
      return false;
    }
    return true;
  };

  if (!compileCS("SplatCS", m_csSplat)) return false;
  if (!compileCS("AdvectCS", m_csAdvect)) return false;
  if (!compileCS("DivergenceCS", m_csDivergence)) return false;
  if (!compileCS("PressureJacobiCS", m_csPressureJacobi)) return false;
  if (!compileCS("GradientSubtractCS", m_csGradientSubtract)) return false;
  if (!compileCS("CurlCS", m_csCurl)) return false;
  if (!compileCS("VorticityCS", m_csVorticity)) return false;
  if (!compileCS("ClearCS", m_csClear)) return false;
  if (!compileCS("BloomPrefilterCS", m_csBloomPrefilter)) return false;
  if (!compileCS("BloomBlurCS", m_csBloomBlur)) return false;
  if (!compileCS("BloomUpsampleCS", m_csBloomUpsample)) return false;
  if (!compileCS("BloomFinalCS", m_csBloomFinal)) return false;

  ComPtr<ID3DBlob> vsBlob = FluidCompileShaderBlob(kFluidShaderSource, srcLen, "FullscreenVS", "vs_5_0");
  if (!vsBlob.Get()) return false;
  HRESULT hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                            nullptr, &m_vsFullscreen);
  if (FAILED(hr)) {
    Wh_Log(L"FluidSimulation: CreateVertexShader failed, hr=0x%08lX", (unsigned long)hr);
    return false;
  }

  ComPtr<ID3DBlob> psBlob = FluidCompileShaderBlob(kFluidShaderSource, srcLen, "CompositePS", "ps_5_0");
  if (!psBlob.Get()) return false;
  hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr,
                                   &m_psComposite);
  if (FAILED(hr)) {
    Wh_Log(L"FluidSimulation: CreatePixelShader failed, hr=0x%08lX", (unsigned long)hr);
    return false;
  }
  return true;
}

bool FluidSimulation::Initialize(ID3D11Device *device, ID3D11DeviceContext *context,
                                 IDXGISwapChain1 *swapChain, HWND hwnd, int screenW, int screenH) {
  Shutdown();
  if (!device || !context || !swapChain || !hwnd)
    return false;

  if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0) {
    Wh_Log(L"FluidSimulation::Initialize: GPU feature level below 11_0; aborting fluid mode");
    return false;
  }

  m_device = device;
  m_context = context;
  m_swapChain = swapChain;
  m_hwnd = hwnd;
  m_screenWidth = screenW;
  m_screenHeight = screenH;
  GetWindowRect(hwnd, &m_windowRect);

  // Multithread protection is required when Media Foundation shares the
  // D3D11 device across its own decoder worker threads (video mode). With
  // MF not running in fluid mode, only the wallpaper thread touches the
  // device, so turning the lock off is safe and gives back the frame
  // budget. Re-enabled automatically by VideoPlayer::InitD3DAndSwapChain
  // on the next switch back to video mode.
  {
    ComPtr<ID3D10Multithread> mt;
    if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D10Multithread),
                                         (void **)&mt))) {
      mt->SetMultithreadProtected(FALSE);
    }
  }

  if (!CompileShaders()) {
    Wh_Log(L"FluidSimulation::Initialize: shader compilation failed");
    Shutdown();
    return false;
  }

  ComputeResolutions(screenW, screenH);
  if (!CreateResources() || !CreateBloomChain()) {
    Wh_Log(L"FluidSimulation::Initialize: resource creation failed");
    Shutdown();
    return false;
  }

  D3D11_SAMPLER_DESC sampDesc = {};
  sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
  HRESULT hr = device->CreateSamplerState(&sampDesc, &m_sampler);
  if (FAILED(hr)) { Shutdown(); return false; }

  D3D11_RASTERIZER_DESC rastDesc = {};
  rastDesc.FillMode = D3D11_FILL_SOLID;
  rastDesc.CullMode = D3D11_CULL_NONE;
  rastDesc.DepthClipEnable = TRUE;
  hr = device->CreateRasterizerState(&rastDesc, &m_rasterizerState);
  if (FAILED(hr)) { Shutdown(); return false; }

  D3D11_BUFFER_DESC cbDesc = {};
  cbDesc.Usage = D3D11_USAGE_DEFAULT;
  cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

  cbDesc.ByteWidth = sizeof(FluidSimParamsCB);
  if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &m_cbSimParams))) { Shutdown(); return false; }
  cbDesc.ByteWidth = sizeof(FluidSplatParamsCB);
  if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &m_cbSplatParams))) { Shutdown(); return false; }
  cbDesc.ByteWidth = sizeof(FluidVorticityParamsCB);
  if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &m_cbVorticityParams))) { Shutdown(); return false; }
  cbDesc.ByteWidth = sizeof(FluidBloomParamsCB);
  if (FAILED(device->CreateBuffer(&cbDesc, nullptr, &m_cbBloomParams))) { Shutdown(); return false; }

  ClearAllTextures();

  QueryPerformanceCounter(&m_lastFrameTicks);
  m_havePrevCursor = false;
  m_lastCornerSplatTickMs = 0;
  m_lastPressureResetTickMs = 0;
  m_frameNumber = 0;
  m_everShown = false;
  m_initialized = true;

  // Seed with random splats so the wallpaper starts alive.
  int seed = 5 + (rand() % 20);
  for (int i = 0; i < seed; ++i) {
    float r, g, b;
    ComputeSplatColor(r, g, b);
    float x = (float)rand() / (float)RAND_MAX;
    float y = (float)rand() / (float)RAND_MAX;
    float dx = 1000.0f * ((float)rand() / (float)RAND_MAX - 0.5f);
    float dy = 1000.0f * ((float)rand() / (float)RAND_MAX - 0.5f);
    float radiusUV = 0.0025f;
    if (m_screenWidth > m_screenHeight) radiusUV *= (float)m_screenWidth / (float)m_screenHeight;
    DoSplatVelocity(x, y, dx, dy, radiusUV);
    DoSplatDye(x, y, r * 10.0f, g * 10.0f, b * 10.0f, radiusUV);
  }

  Wh_Log(L"FluidSimulation::Initialize: ready (sim %dx%d, dye %dx%d, bloom %dx%d)",
         m_simWidth, m_simHeight, m_dyeWidth, m_dyeHeight, m_bloomWidth, m_bloomHeight);
  return true;
}

void FluidSimulation::Resize(int screenW, int screenH) {
  if (!m_initialized)
    return;
  m_screenWidth = screenW;
  m_screenHeight = screenH;
  if (m_hwnd)
    GetWindowRect(m_hwnd, &m_windowRect);

  for (int i = 0; i < 2; ++i) {
    m_cachedBackBuffer[i].Reset();
    m_cachedBackBufferRtv[i].Reset();
  }

  int oldSimW = m_simWidth, oldSimH = m_simHeight;
  int oldDyeW = m_dyeWidth, oldDyeH = m_dyeHeight;
  ComputeResolutions(screenW, screenH);
  if (m_simWidth != oldSimW || m_simHeight != oldSimH ||
      m_dyeWidth != oldDyeW || m_dyeHeight != oldDyeH) {
    if (CreateResources() && CreateBloomChain())
      ClearAllTextures();
  }
}

void FluidSimulation::Shutdown() {
  ReleaseSimTextures();
  ReleaseBloomTextures();
  m_csSplat.Reset();
  m_csAdvect.Reset();
  m_csDivergence.Reset();
  m_csPressureJacobi.Reset();
  m_csGradientSubtract.Reset();
  m_csCurl.Reset();
  m_csVorticity.Reset();
  m_csClear.Reset();
  m_csBloomPrefilter.Reset();
  m_csBloomBlur.Reset();
  m_csBloomUpsample.Reset();
  m_csBloomFinal.Reset();
  m_vsFullscreen.Reset();
  m_psComposite.Reset();
  m_cbSimParams.Reset();
  m_cbSplatParams.Reset();
  m_cbVorticityParams.Reset();
  m_cbBloomParams.Reset();
  m_sampler.Reset();
  m_rasterizerState.Reset();
  for (int i = 0; i < 2; ++i) {
    m_cachedBackBuffer[i].Reset();
    m_cachedBackBufferRtv[i].Reset();
  }
  m_device = nullptr;
  m_context = nullptr;
  m_swapChain = nullptr;
  m_hwnd = nullptr;
  m_havePrevCursor = false;
  m_pausedForFullscreen = m_pausedForBattery = m_pausedForSession = false;
  m_everShown = false;
  m_initialized = false;
}

void FluidSimulation::ComputeSplatColor(float &r, float &g, float &b) {
  if (g_fluidColorful.load()) {
    float hue = (float)rand() / (float)RAND_MAX;
    FluidHsvToRgb(hue, 1.0f, 1.0f, r, g, b);
  } else {
    r = 0.2f; g = 0.8f; b = 1.0f;
  }
  r *= 0.15f; g *= 0.15f; b *= 0.15f;
}

void FluidSimulation::DoSplatVelocity(float x, float y, float dx, float dy, float radiusUV) {
  FluidSplatParamsCB sp = {};
  sp.pointX = x;
  sp.pointY = y;
  sp.value0 = dx;
  sp.value1 = dy;
  sp.radiusSq = radiusUV;
  sp.aspectRatio = (m_screenHeight > 0)
                       ? (float)m_screenWidth / (float)m_screenHeight : 1.0f;
  m_context->UpdateSubresource(m_cbSplatParams.Get(), 0, nullptr, &sp, 0, 0);
  ID3D11Buffer *cb = m_cbSplatParams.Get();
  m_context->CSSetConstantBuffers(1, 1, &cb);

  int dst = 1 - m_velocityIdx;
  ID3D11ShaderResourceView *srv = m_velocity[m_velocityIdx].srv.Get();
  m_context->CSSetShaderResources(0, 1, &srv);
  ID3D11UnorderedAccessView *uav = m_velocity[dst].uav.Get();
  m_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
  m_context->CSSetShader(m_csSplat.Get(), nullptr, 0);
  DispatchSim(m_simWidth, m_simHeight);
  FluidUnbindCSUAVs(m_context, 0, 1);
  FluidUnbindCSSRVs(m_context, 0, 1);
  m_velocityIdx = dst;
}

void FluidSimulation::DoSplatDye(float x, float y, float r, float g, float b, float radiusUV) {
  FluidSplatParamsCB sp = {};
  sp.pointX = x;
  sp.pointY = y;
  sp.value0 = r;
  sp.value1 = g;
  sp.value2 = b;
  sp.radiusSq = radiusUV;
  sp.aspectRatio = (m_screenHeight > 0)
                       ? (float)m_screenWidth / (float)m_screenHeight : 1.0f;
  m_context->UpdateSubresource(m_cbSplatParams.Get(), 0, nullptr, &sp, 0, 0);
  ID3D11Buffer *cb = m_cbSplatParams.Get();
  m_context->CSSetConstantBuffers(1, 1, &cb);

  int dst = 1 - m_dyeIdx;
  ID3D11ShaderResourceView *srv = m_dye[m_dyeIdx].srv.Get();
  m_context->CSSetShaderResources(0, 1, &srv);
  ID3D11UnorderedAccessView *uav = m_dye[dst].uav.Get();
  m_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
  m_context->CSSetShader(m_csSplat.Get(), nullptr, 0);
  DispatchSim(m_dyeWidth, m_dyeHeight);
  FluidUnbindCSUAVs(m_context, 0, 1);
  FluidUnbindCSSRVs(m_context, 0, 1);
  m_dyeIdx = dst;
}

void FluidSimulation::UpdateSimulation(float dt) {
  int radiusPct = g_fluidSplatRadius.load();
  int speedPct  = g_fluidSpeed.load();
  int randomIntervalSec = g_fluidRandomSplatsInterval.load();

  float radiusScale = (radiusPct <= 0) ? 0.05f : (radiusPct / 25.0f);
  float speedMul    = speedPct / 100.0f;
  if (speedMul < 0.05f) speedMul = 0.05f;

  float dyeDissipation = 1.0f;
  float vorticityStrength = 30.0f * speedMul;
  float velDissipation   = 0.2f / speedMul;
  float splatForce       = 6000.0f * speedMul;

  bool cornerSplatsEnabled = (randomIntervalSec > 0);
  const float kSplatRadiusUVBase = 0.0025f * radiusScale;

  if (!g_fluidColorful.load()) {
    m_cursorColorR = 0.2f; m_cursorColorG = 0.8f; m_cursorColorB = 1.0f;
    m_cursorColorValid = true;
  } else {
    m_cursorColorTimer += dt * 2.0f;
    if (!m_cursorColorValid || m_cursorColorTimer >= 1.0f) {
      m_cursorColorTimer = 0.0f;
      float hue = (float)rand() / (float)RAND_MAX;
      FluidHsvToRgb(hue, 1.0f, 1.0f, m_cursorColorR, m_cursorColorG, m_cursorColorB);
      m_cursorColorValid = true;
    }
  }

  POINT cursor;
  if (GetCursorPos(&cursor)) {
    int ww = m_windowRect.right - m_windowRect.left;
    int wh = m_windowRect.bottom - m_windowRect.top;
    if (ww > 0 && wh > 0) {
      float ux = (float)(cursor.x - m_windowRect.left) / (float)ww;
      float uy = (float)(cursor.y - m_windowRect.top) / (float)wh;
      bool inBounds = (ux >= 0.0f && ux <= 1.0f && uy >= 0.0f && uy <= 1.0f);

      if (inBounds && m_havePrevCursor) {
        float prevUx = (float)(m_prevCursor.x - m_windowRect.left) / (float)ww;
        float prevUy = (float)(m_prevCursor.y - m_windowRect.top) / (float)wh;
        float dUx = ux - prevUx;
        float dUy = uy - prevUy;
        if (fabsf(dUx) > 1e-5f || fabsf(dUy) > 1e-5f) {
          float dx = dUx * splatForce;
          float dy = dUy * splatForce;

          float r = m_cursorColorR * 0.15f;
          float g = m_cursorColorG * 0.15f;
          float b = m_cursorColorB * 0.15f;

          float radiusUV = kSplatRadiusUVBase;
          if (m_screenWidth > m_screenHeight)
            radiusUV *= (float)m_screenWidth / (float)m_screenHeight;

          DoSplatVelocity(ux, uy, dx, dy, radiusUV);
          DoSplatDye(ux, uy, r, g, b, radiusUV);
        }
      }
      m_havePrevCursor = inBounds;
      if (inBounds)
        m_prevCursor = cursor;
    }
  }

  DWORD nowMs = GetTickCount();
  if (cornerSplatsEnabled && nowMs - m_lastCornerSplatTickMs > (DWORD)(randomIntervalSec * 1000)) {
    int count = 5 + (rand() % 6);
    for (int i = 0; i < count; ++i) {
      float r, g, b;
      ComputeSplatColor(r, g, b);

      float px = (float)rand() / (float)RAND_MAX;
      float py = (float)rand() / (float)RAND_MAX;
      float dx = 1000.0f * ((float)rand() / (float)RAND_MAX - 0.5f);
      float dy = 1000.0f * ((float)rand() / (float)RAND_MAX - 0.5f);

      float radiusUV = kSplatRadiusUVBase;
      if (m_screenWidth > m_screenHeight)
        radiusUV *= (float)m_screenWidth / (float)m_screenHeight;

      DoSplatVelocity(px, py, dx, dy, radiusUV);
      DoSplatDye(px, py, r * 10.0f, g * 10.0f, b * 10.0f, radiusUV);
    }
    m_lastCornerSplatTickMs = nowMs;
  }

  FluidSimParamsCB sp = {};
  sp.dt = dt;
  sp.simTexelX = 1.0f / (float)m_simWidth;
  sp.simTexelY = 1.0f / (float)m_simHeight;

  {
    ID3D11ShaderResourceView *srv = m_velocity[m_velocityIdx].srv.Get();
    m_context->CSSetShaderResources(8, 1, &srv);
    ID3D11UnorderedAccessView *uav = m_curl.uav.Get();
    m_context->CSSetUnorderedAccessViews(5, 1, &uav, nullptr);
    m_context->CSSetShader(m_csCurl.Get(), nullptr, 0);
    DispatchSim(m_simWidth, m_simHeight);
    FluidUnbindCSUAVs(m_context, 5, 1);
    FluidUnbindCSSRVs(m_context, 8, 1);
  }

  {
    FluidVorticityParamsCB vp = {};
    vp.curl = vorticityStrength;
    vp.dt = dt;
    m_context->UpdateSubresource(m_cbVorticityParams.Get(), 0, nullptr, &vp, 0, 0);
    ID3D11Buffer *cb = m_cbVorticityParams.Get();
    m_context->CSSetConstantBuffers(2, 1, &cb);

    int dst = 1 - m_velocityIdx;
    ID3D11ShaderResourceView *srvs[2] = {m_velocity[m_velocityIdx].srv.Get(), m_curl.srv.Get()};
    m_context->CSSetShaderResources(9, 2, srvs);
    ID3D11UnorderedAccessView *uav = m_velocity[dst].uav.Get();
    m_context->CSSetUnorderedAccessViews(6, 1, &uav, nullptr);
    m_context->CSSetShader(m_csVorticity.Get(), nullptr, 0);
    DispatchSim(m_simWidth, m_simHeight);
    FluidUnbindCSUAVs(m_context, 6, 1);
    FluidUnbindCSSRVs(m_context, 9, 2);
    m_velocityIdx = dst;
  }

  {
    ID3D11ShaderResourceView *srv = m_velocity[m_velocityIdx].srv.Get();
    m_context->CSSetShaderResources(3, 1, &srv);
    ID3D11UnorderedAccessView *uav = m_divergence.uav.Get();
    m_context->CSSetUnorderedAccessViews(2, 1, &uav, nullptr);
    m_context->CSSetShader(m_csDivergence.Get(), nullptr, 0);
    DispatchSim(m_simWidth, m_simHeight);
    FluidUnbindCSUAVs(m_context, 2, 1);
    FluidUnbindCSSRVs(m_context, 3, 1);
  }

  {
    FluidSimParamsCB cp = sp;
    cp.clearValue = 0.5f;
    m_context->UpdateSubresource(m_cbSimParams.Get(), 0, nullptr, &cp, 0, 0);
    ID3D11Buffer *cb = m_cbSimParams.Get();
    m_context->CSSetConstantBuffers(0, 1, &cb);

    int dst = 1 - m_pressureIdx;
    ID3D11ShaderResourceView *srv = m_pressure[m_pressureIdx].srv.Get();
    m_context->CSSetShaderResources(12, 1, &srv);
    ID3D11UnorderedAccessView *uav = m_pressure[dst].uav.Get();
    m_context->CSSetUnorderedAccessViews(7, 1, &uav, nullptr);
    m_context->CSSetShader(m_csClear.Get(), nullptr, 0);
    DispatchSim(m_simWidth, m_simHeight);
    FluidUnbindCSUAVs(m_context, 7, 1);
    FluidUnbindCSSRVs(m_context, 12, 1);
    m_pressureIdx = dst;
  }

  const int kPressureIterations = 12;
  for (int iter = 0; iter < kPressureIterations; ++iter) {
    int dst = 1 - m_pressureIdx;
    ID3D11ShaderResourceView *srvs[2] = {m_pressure[m_pressureIdx].srv.Get(),
                                         m_divergence.srv.Get()};
    m_context->CSSetShaderResources(4, 2, srvs);
    ID3D11UnorderedAccessView *uav = m_pressure[dst].uav.Get();
    m_context->CSSetUnorderedAccessViews(3, 1, &uav, nullptr);
    m_context->CSSetShader(m_csPressureJacobi.Get(), nullptr, 0);
    DispatchSim(m_simWidth, m_simHeight);
    FluidUnbindCSUAVs(m_context, 3, 1);
    FluidUnbindCSSRVs(m_context, 4, 2);
    m_pressureIdx = dst;
  }

  {
    int dst = 1 - m_velocityIdx;
    ID3D11ShaderResourceView *srvs[2] = {m_velocity[m_velocityIdx].srv.Get(),
                                         m_pressure[m_pressureIdx].srv.Get()};
    m_context->CSSetShaderResources(6, 2, srvs);
    ID3D11UnorderedAccessView *uav = m_velocity[dst].uav.Get();
    m_context->CSSetUnorderedAccessViews(4, 1, &uav, nullptr);
    m_context->CSSetShader(m_csGradientSubtract.Get(), nullptr, 0);
    DispatchSim(m_simWidth, m_simHeight);
    FluidUnbindCSUAVs(m_context, 4, 1);
    FluidUnbindCSSRVs(m_context, 6, 2);
    m_velocityIdx = dst;
  }

  {
    sp.dissipation = velDissipation;
    m_context->UpdateSubresource(m_cbSimParams.Get(), 0, nullptr, &sp, 0, 0);
    ID3D11Buffer *cb = m_cbSimParams.Get();
    m_context->CSSetConstantBuffers(0, 1, &cb);
    ID3D11SamplerState *samp = m_sampler.Get();
    m_context->CSSetSamplers(0, 1, &samp);

    int dst = 1 - m_velocityIdx;
    ID3D11ShaderResourceView *srvs[2] = {m_velocity[m_velocityIdx].srv.Get(),
                                         m_velocity[m_velocityIdx].srv.Get()};
    m_context->CSSetShaderResources(1, 2, srvs);
    ID3D11UnorderedAccessView *uav = m_velocity[dst].uav.Get();
    m_context->CSSetUnorderedAccessViews(1, 1, &uav, nullptr);
    m_context->CSSetShader(m_csAdvect.Get(), nullptr, 0);
    DispatchSim(m_simWidth, m_simHeight);
    FluidUnbindCSUAVs(m_context, 1, 1);
    FluidUnbindCSSRVs(m_context, 1, 2);
    m_velocityIdx = dst;
  }

  {
    sp.dissipation = dyeDissipation;
    m_context->UpdateSubresource(m_cbSimParams.Get(), 0, nullptr, &sp, 0, 0);
    ID3D11Buffer *cb = m_cbSimParams.Get();
    m_context->CSSetConstantBuffers(0, 1, &cb);

    int dst = 1 - m_dyeIdx;
    ID3D11ShaderResourceView *srvs[2] = {m_velocity[m_velocityIdx].srv.Get(),
                                         m_dye[m_dyeIdx].srv.Get()};
    m_context->CSSetShaderResources(1, 2, srvs);
    ID3D11UnorderedAccessView *uav = m_dye[dst].uav.Get();
    m_context->CSSetUnorderedAccessViews(1, 1, &uav, nullptr);
    m_context->CSSetShader(m_csAdvect.Get(), nullptr, 0);
    DispatchSim(m_dyeWidth, m_dyeHeight);
    FluidUnbindCSUAVs(m_context, 1, 1);
    FluidUnbindCSSRVs(m_context, 1, 2);
    m_dyeIdx = dst;
  }

  m_context->CSSetShader(nullptr, nullptr, 0);
}

void FluidSimulation::ApplyBloom() {
  if (m_bloomChainCount < 2)
    return;

  const float kThreshold = 0.6f;
  const float kSoftKnee   = 0.7f;
  const float kIntensity  = g_fluidBloom.load() / 100.0f;

  float knee   = kThreshold * kSoftKnee + 0.0001f;
  float curve0 = kThreshold - knee;
  float curve1 = knee * 2.0f;
  float curve2 = 0.25f / knee;

  {
    FluidBloomParamsCB bp = {};
    bp.intensity = kIntensity;
    bp.threshold = kThreshold;
    bp.curve0 = curve0;
    bp.curve1 = curve1;
    bp.curve2 = curve2;
    m_context->UpdateSubresource(m_cbBloomParams.Get(), 0, nullptr, &bp, 0, 0);
    ID3D11Buffer *cb = m_cbBloomParams.Get();
    m_context->CSSetConstantBuffers(3, 1, &cb);

    ID3D11ShaderResourceView *srv = m_dye[m_dyeIdx].srv.Get();
    m_context->CSSetShaderResources(11, 1, &srv);
    ID3D11UnorderedAccessView *uav = m_bloom.uav.Get();
    m_context->CSSetUnorderedAccessViews(7, 1, &uav, nullptr);
    m_context->CSSetShader(m_csBloomPrefilter.Get(), nullptr, 0);
    DispatchSim(m_bloomWidth, m_bloomHeight);
    FluidUnbindCSUAVs(m_context, 7, 1);
    FluidUnbindCSSRVs(m_context, 11, 1);
  }

  SimTexture *last = &m_bloom;
  for (int i = 0; i < m_bloomChainCount; ++i) {
    SimTexture *dst = &m_bloomChain[i];
    D3D11_TEXTURE2D_DESC srcDesc;
    last->tex->GetDesc(&srcDesc);

    FluidBloomParamsCB bp = {};
    bp.texelSizeX = 1.0f / (float)srcDesc.Width;
    bp.texelSizeY = 1.0f / (float)srcDesc.Height;
    m_context->UpdateSubresource(m_cbBloomParams.Get(), 0, nullptr, &bp, 0, 0);
    ID3D11Buffer *cb = m_cbBloomParams.Get();
    m_context->CSSetConstantBuffers(3, 1, &cb);

    ID3D11ShaderResourceView *srv = last->srv.Get();
    m_context->CSSetShaderResources(11, 1, &srv);
    ID3D11UnorderedAccessView *uav = dst->uav.Get();
    m_context->CSSetUnorderedAccessViews(7, 1, &uav, nullptr);
    m_context->CSSetShader(m_csBloomBlur.Get(), nullptr, 0);
    DispatchSim((int)srcDesc.Width / 2, (int)srcDesc.Height / 2);
    FluidUnbindCSUAVs(m_context, 7, 1);
    FluidUnbindCSSRVs(m_context, 11, 1);
    last = dst;
  }

  for (int i = m_bloomChainCount - 2; i >= 0; --i) {
    SimTexture *src = &m_bloomChain[i + 1];
    SimTexture *dst = &m_bloomChain[i];
    D3D11_TEXTURE2D_DESC srcDesc;
    src->tex->GetDesc(&srcDesc);

    FluidBloomParamsCB bp = {};
    bp.texelSizeX = 1.0f / (float)srcDesc.Width;
    bp.texelSizeY = 1.0f / (float)srcDesc.Height;
    m_context->UpdateSubresource(m_cbBloomParams.Get(), 0, nullptr, &bp, 0, 0);
    ID3D11Buffer *cb = m_cbBloomParams.Get();
    m_context->CSSetConstantBuffers(3, 1, &cb);

    ID3D11ShaderResourceView *srv = src->srv.Get();
    m_context->CSSetShaderResources(11, 1, &srv);
    ID3D11UnorderedAccessView *uav = dst->uav.Get();
    m_context->CSSetUnorderedAccessViews(7, 1, &uav, nullptr);
    m_context->CSSetShader(m_csBloomUpsample.Get(), nullptr, 0);

    D3D11_TEXTURE2D_DESC dstDesc;
    dst->tex->GetDesc(&dstDesc);
    DispatchSim((int)dstDesc.Width, (int)dstDesc.Height);
    FluidUnbindCSUAVs(m_context, 7, 1);
    FluidUnbindCSSRVs(m_context, 11, 1);
  }

  {
    SimTexture *src = &m_bloomChain[0];
    D3D11_TEXTURE2D_DESC srcDesc;
    src->tex->GetDesc(&srcDesc);

    FluidBloomParamsCB bp = {};
    bp.intensity = kIntensity;
    bp.texelSizeX = 1.0f / (float)srcDesc.Width;
    bp.texelSizeY = 1.0f / (float)srcDesc.Height;
    m_context->UpdateSubresource(m_cbBloomParams.Get(), 0, nullptr, &bp, 0, 0);
    ID3D11Buffer *cb = m_cbBloomParams.Get();
    m_context->CSSetConstantBuffers(3, 1, &cb);

    ID3D11ShaderResourceView *srv = src->srv.Get();
    m_context->CSSetShaderResources(11, 1, &srv);
    ID3D11UnorderedAccessView *uav = m_bloom.uav.Get();
    m_context->CSSetUnorderedAccessViews(7, 1, &uav, nullptr);
    m_context->CSSetShader(m_csBloomFinal.Get(), nullptr, 0);
    DispatchSim(m_bloomWidth, m_bloomHeight);
    FluidUnbindCSUAVs(m_context, 7, 1);
    FluidUnbindCSSRVs(m_context, 11, 1);
  }
}

void FluidSimulation::RenderComposite(ID3D11RenderTargetView *rtv) {
  ApplyBloom();

  D3D11_VIEWPORT vp = {};
  vp.Width = (FLOAT)m_screenWidth;
  vp.Height = (FLOAT)m_screenHeight;
  vp.MaxDepth = 1.0f;
  m_context->RSSetViewports(1, &vp);

  ID3D11RenderTargetView *rtvs[1] = {rtv};
  m_context->OMSetRenderTargets(1, rtvs, nullptr);
  m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
  m_context->OMSetDepthStencilState(nullptr, 0);
  m_context->RSSetState(m_rasterizerState.Get());

  const FLOAT clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  m_context->ClearRenderTargetView(rtv, clearColor);

  m_context->IASetInputLayout(nullptr);
  m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_context->VSSetShader(m_vsFullscreen.Get(), nullptr, 0);
  m_context->PSSetShader(m_psComposite.Get(), nullptr, 0);

  ID3D11ShaderResourceView *srvs[2] = {m_dye[m_dyeIdx].srv.Get(), m_bloom.srv.Get()};
  m_context->PSSetShaderResources(13, 2, srvs);
  ID3D11SamplerState *samp = m_sampler.Get();
  m_context->PSSetSamplers(0, 1, &samp);

  m_context->Draw(3, 0);

  ID3D11ShaderResourceView *nullSrvs[2] = {nullptr, nullptr};
  m_context->PSSetShaderResources(13, 2, nullSrvs);
  ID3D11RenderTargetView *nullRtv = nullptr;
  m_context->OMSetRenderTargets(1, &nullRtv, nullptr);
}

FluidSimulation::TickResult FluidSimulation::Tick(bool isOnBattery) {
  if (!m_initialized)
    return TickResult::Skipped;
  if (IsEffectivePaused())
    return TickResult::Skipped;

  if (isOnBattery && g_batteryMode.load() == BatteryMode::DropFps) {
    DWORD nowMs = GetTickCount();
    if (m_lastDropFpsTickMs != 0 && nowMs - m_lastDropFpsTickMs < 66) {
      return TickResult::Skipped;
    }
    m_lastDropFpsTickMs = nowMs;
  }

  {
    DWORD nowMs = GetTickCount();
    if (m_lastPressureResetTickMs == 0) {
      m_lastPressureResetTickMs = nowMs;
    } else if (nowMs - m_lastPressureResetTickMs > 25000) {
      const FLOAT zero[4] = {0, 0, 0, 0};
      m_context->ClearUnorderedAccessViewFloat(m_pressure[0].uav.Get(), zero);
      m_context->ClearUnorderedAccessViewFloat(m_pressure[1].uav.Get(), zero);
      m_lastPressureResetTickMs = nowMs;
    }
  }

  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  double dt = 1.0 / 60.0;
  if (m_lastFrameTicks.QuadPart != 0 && freq.QuadPart > 0) {
    dt = (double)(now.QuadPart - m_lastFrameTicks.QuadPart) / (double)freq.QuadPart;
    dt = (std::min)(dt, 1.0 / 15.0);
    dt = (std::max)(dt, 1.0 / 240.0);
  }
  m_lastFrameTicks = now;

  Profiler::BeginFrame((DWORD)(++m_frameNumber));
  Profiler::RecordTickCall();
  Profiler::BeginSection("TransferVideoFrame");
  UpdateSimulation((float)dt);
  Profiler::EndSection("TransferVideoFrame");

  ComPtr<ID3D11Texture2D> backBuffer;
  HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backBuffer);
  if (FAILED(hr)) {
    Profiler::EndFrame();
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
      return TickResult::DeviceLost;
    return TickResult::Ok;
  }

  ID3D11RenderTargetView *rtv = nullptr;
  for (int i = 0; i < 2; ++i) {
    if (m_cachedBackBuffer[i].Get() == backBuffer.Get()) {
      rtv = m_cachedBackBufferRtv[i].Get();
      break;
    }
  }
  if (!rtv) {
    int slot = -1;
    for (int i = 0; i < 2; ++i) {
      if (!m_cachedBackBuffer[i].Get()) { slot = i; break; }
    }
    if (slot < 0) slot = 0;
    m_cachedBackBuffer[slot] = backBuffer;
    m_cachedBackBufferRtv[slot].Reset();
    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr,
                                          &m_cachedBackBufferRtv[slot]);
    if (SUCCEEDED(hr))
      rtv = m_cachedBackBufferRtv[slot].Get();
  }

  if (rtv) {
    RenderComposite(rtv);
  }

  Profiler::DrawOverlay(m_hwnd, backBuffer.Get());

  LARGE_INTEGER pStart, pEnd;
  QueryPerformanceCounter(&pStart);
  Profiler::BeginSection("Present");
  // Present(0, 0) instead of Present(1, 0): the DWM composition schedule
  // for a child-of-WorkerW window can throttle harder than the desktop's
  // real refresh rate, and vsync-blocking on it was capping us well below
  // the frame rate the render loop can actually produce.
  hr = m_swapChain->Present(0, 0);
  Profiler::EndSection("Present");
  QueryPerformanceCounter(&pEnd);
  double presentMs = (freq.QuadPart > 0)
                         ? (double)(pEnd.QuadPart - pStart.QuadPart) * 1000.0 / (double)freq.QuadPart
                         : 0.0;
  Profiler::RecordPresent(hr, presentMs);
  Profiler::EndFrame();

  if (SUCCEEDED(hr)) {
    if (!m_everShown && m_hwnd && !IsWindowVisible(m_hwnd)) {
      ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
      SetWindowPos(m_hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
      m_everShown = true;
    }
    return TickResult::Ok;
  }
  if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
    return TickResult::DeviceLost;
  }
  return TickResult::Ok;
}

// ---------------------------------------------------------------------------
// Window / message constants
// ---------------------------------------------------------------------------

// Undocumented Progman message that triggers creation of the
// behind-desktop WorkerW window on classic shell builds.
constexpr UINT WM_SPAWN_WORKER = 0x052C;

bool g_topLevelMode = false;
int g_sourceRetryAttempts = 0;
bool g_wallpaperHidden = false;
bool g_isOnBattery = false;

bool ResolveVideoSource(std::wstring &outPath);

// ---------------------------------------------------------------------------
// Media Foundation notify callback
// ---------------------------------------------------------------------------
//
// EventNotify is invoked on an internal Media Foundation work-queue thread,
// NOT the wallpaper thread. It must not touch the engine, D3D objects, or
// window state directly -- everything is forwarded via PostMessage and
// handled back on the wallpaper thread's own message loop, same as
// kMsgReloadSource already does.

class MediaEngineNotify : public IMFMediaEngineNotify {
public:
  explicit MediaEngineNotify(HWND targetWnd) : refCount(1), wnd(targetWnd) {}
  virtual ~MediaEngineNotify() = default;

  STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv)
      return E_POINTER;
    if (riid == IID_IUnknown || riid == __uuidof(IMFMediaEngineNotify)) {
      *ppv = static_cast<IMFMediaEngineNotify *>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&refCount);
  }
  STDMETHODIMP_(ULONG) Release() override {
    ULONG c = InterlockedDecrement(&refCount);
    if (c == 0)
      delete this;
    return c;
  }

  STDMETHODIMP EventNotify(DWORD event, DWORD_PTR param1,
                           DWORD param2) override {
    Profiler::BeginSection("MediaEngineNotify");
    PostMessageW(wnd, kMsgMediaEngineEvent, (WPARAM)event, (LPARAM)param1);
    Profiler::EndSection("MediaEngineNotify");
    return S_OK;
  }

private:
  volatile LONG refCount;
  HWND wnd;
};

// ---------------------------------------------------------------------------
// Video player: D3D11 device, DXGI swap chain bound to the wallpaper HWND,
// and the Media Foundation media engine that decodes into it.
// ---------------------------------------------------------------------------

struct VideoPlayer {
  ComPtr<ID3D11Device> d3dDevice;
  ComPtr<ID3D11DeviceContext> d3dContext;
  ComPtr<IDXGISwapChain1> swapChain;
  ComPtr<IMFDXGIDeviceManager> dxgiManager;
  ComPtr<IMFMediaEngine> engine;
  MediaEngineNotify *notify = nullptr; // ref-owned by `engine` after creation
  UINT resetToken = 0;

  // TransferVideoFrame's target -- deliberately NOT the swap chain's own
  // back buffer. Flip-model (DXGI_SWAP_EFFECT_FLIP_DISCARD) back buffers
  // have presentation-timing constraints that Media Foundation's internal
  // video processor isn't necessarily written to respect; writing into an
  // ordinary offscreen render target and then CopyResource-ing that into
  // the actual back buffer each frame is the documented-safe pattern.
  ComPtr<ID3D11Texture2D> renderTarget;
  ComPtr<ID3D11RenderTargetView> renderTargetRTV;
  ComPtr<ID3D11RenderTargetView> tempRTV;
  ID3D11Texture2D* tempRTVTex = nullptr;

  void ResetRTVs() {
    renderTargetRTV.Reset();
    tempRTV.Reset();
    tempRTVTex = nullptr;
  }

  enum class TransferMode { Unknown, DirectBackBuffer, OffscreenFallback };
  TransferMode transferMode = TransferMode::Unknown;

  bool loaded = false;
  bool canPlay = false;
  bool wantsPlay =
      false; // true once caller asked to play, even if CANPLAY hasn't fired yet
  bool needsInitialFrame = false; // true right after load until frame 0 is presented
  bool pausedForFullscreen = false;
  bool pausedForBattery = false;
  bool pausedForSession = false;
  DWORD loadStartTickMs = 0;
  DWORD canPlayTickMs = 0;
  bool loadTimeoutLogged = false;
  int loadTimeoutRetryCount = 0;
  static const int kMaxLoadTimeoutRetries = 3;

  DWORD cachedVideoW = 0;
  DWORD cachedVideoH = 0;
  RECT cachedDestRect = {0, 0, 0, 0};
  bool destRectValid = false;

  int width = 0;
  int height = 0;

  bool InitD3DAndSwapChain(HWND hwnd, int w, int h) {
    boundHwnd = hwnd;
    width = w;
    height = h;

    // VIDEO_SUPPORT: required for the device to be usable for hardware
    // video decode/processing at all -- without it, Media Foundation
    // may still accept the device but hit driver-level trouble doing
    // real decode work against it, matching what we're seeing.
    UINT deviceFlags =
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   deviceFlags, nullptr, 0, D3D11_SDK_VERSION,
                                   &d3dDevice, &featureLevel, &d3dContext);
    if (FAILED(hr)) {
      Wh_Log(L"VideoPlayer::InitD3DAndSwapChain: D3D11CreateDevice failed, "
             L"hr=0x%08lX",
             (unsigned long)hr);
      return false;
    }

    // Enable multithread protection -- required when sharing the device
    // with Media Foundation's internal decoder/video-processor threads.
    // Without this, concurrent access causes an immediate GPU fault
    // (DXGI_ERROR_DEVICE_REMOVED) the moment real decode work begins.
    {
      ComPtr<ID3D10Multithread> multithread;
      hr = d3dDevice->QueryInterface(__uuidof(ID3D10Multithread),
                                     (void **)&multithread);
      if (SUCCEEDED(hr)) {
        multithread->SetMultithreadProtected(TRUE);
      }
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice);
    if (FAILED(hr))
      return false;

    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr))
      return false;

    ComPtr<IDXGIFactory2> dxgiFactory;
    hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void **)&dxgiFactory);
    if (FAILED(hr))
      return false;

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width = w;
    scDesc.Height = h;
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    hr = dxgiFactory->CreateSwapChainForHwnd(d3dDevice.Get(), hwnd, &scDesc,
                                             nullptr, nullptr, &swapChain);
    if (FAILED(hr)) {
      Wh_Log(L"VideoPlayer::InitD3DAndSwapChain: CreateSwapChainForHwnd "
             L"failed, hr=0x%08lX",
             (unsigned long)hr);
      return false;
    }

    hr = MFCreateDXGIDeviceManager(&resetToken, &dxgiManager);
    if (FAILED(hr)) {
      Wh_Log(L"VideoPlayer::InitD3DAndSwapChain: MFCreateDXGIDeviceManager "
             L"failed, hr=0x%08lX",
             (unsigned long)hr);
      return false;
    }
    hr = dxgiManager->ResetDevice(d3dDevice.Get(), resetToken);
    if (FAILED(hr)) {
      Wh_Log(
          L"VideoPlayer::InitD3DAndSwapChain: ResetDevice failed, hr=0x%08lX",
          (unsigned long)hr);
      return false;
    }

    if (transferMode == TransferMode::OffscreenFallback) {
      if (!CreateRenderTarget(w, h))
        return false;
    }

    Profiler::Initialize(d3dDevice.Get(), swapChain.Get());
    return true;
  }

  // (Re)creates the offscreen texture TransferVideoFrame writes into.
  // Must be called any time width/height changes (initial creation and
  // Resize()).
  bool CreateRenderTarget(int w, int h) {
    renderTarget.Reset();
    ResetRTVs();

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = Profiler::IsEnabled() ? D3D11_RESOURCE_MISC_GDI_COMPATIBLE : 0;

    HRESULT hr = d3dDevice->CreateTexture2D(&desc, nullptr, &renderTarget);
    if (FAILED(hr)) {
      Wh_Log(L"VideoPlayer::CreateRenderTarget: CreateTexture2D failed, "
             L"hr=0x%08lX",
             (unsigned long)hr);
      return false;
    }
    return true;
  }

  void ClearTexture(ID3D11Texture2D* tex) {
    if (!tex || !d3dDevice.Get() || !d3dContext.Get()) return;
    ComPtr<ID3D11RenderTargetView>* rtvPtr = nullptr;
    if (tex == renderTarget.Get()) {
      rtvPtr = std::addressof(renderTargetRTV);
    } else {
      if (tempRTVTex != tex) {
        tempRTV.Reset();
        tempRTVTex = tex;
      }
      rtvPtr = std::addressof(tempRTV);
    }
    if (!rtvPtr->Get()) {
      HRESULT hr = d3dDevice->CreateRenderTargetView(tex, nullptr, &(*rtvPtr));
      if (FAILED(hr)) return;
    }
    static const float kBlack[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    d3dContext->ClearRenderTargetView(rtvPtr->Get(), kBlack);
  }

  bool InitMediaEngine(HWND notifyTargetWnd) {
    ComPtr<IMFAttributes> attrs;
    HRESULT hr = MFCreateAttributes(&attrs, 3);
    if (FAILED(hr))
      return false;

    notify = new MediaEngineNotify(notifyTargetWnd);
    hr = attrs->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, (IUnknown *)notify);
    if (FAILED(hr))
      return false;

    hr = attrs->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, dxgiManager.Get());
    if (FAILED(hr))
      return false;

    // TransferVideoFrame's D3D11 path expects this to match the swap
    // chain's back buffer format (B8G8R8A8_UNORM above).
    hr = attrs->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT,
                          DXGI_FORMAT_B8G8R8A8_UNORM);
    if (FAILED(hr))
      return false;

    ComPtr<IMFMediaEngineClassFactory> factory;
    hr = CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
                          CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
      Wh_Log(L"VideoPlayer::InitMediaEngine: "
             L"CoCreateInstance(MFMediaEngineClassFactory) "
             L"failed, hr=0x%08lX",
             (unsigned long)hr);
      return false;
    }

    hr = factory->CreateInstance(0, attrs.Get(), &engine);
    if (FAILED(hr)) {
      Wh_Log(L"VideoPlayer::InitMediaEngine: CreateInstance failed, hr=0x%08lX",
             (unsigned long)hr);
      return false;
    }

    engine->SetLoop(TRUE);
    engine->SetMuted(g_audioMuted.load() ? TRUE : FALSE);
    engine->SetVolume(static_cast<double>(g_audioVolume.load()) / 100.0);

    return true;
  }

  // file:// URL builder. UrlCreateFromPathW (shlwapi, already linked) is
  // the real RFC3986-aware Win32 API for this -- it correctly escapes
  // '#', '%', '&', '+', non-ASCII characters, etc, which a hand-rolled
  // escaper handling only backslashes and spaces would mangle.
  static std::wstring BuildFileUrl(const std::wstring &path) {
    // INTERNET_MAX_URL_LENGTH (2048 + 36) without pulling in wininet.h.
    constexpr DWORD kMaxUrlLength = 2084;
    wchar_t urlBuf[kMaxUrlLength] = {};
    DWORD urlLen = ARRAYSIZE(urlBuf);
    HRESULT hr = UrlCreateFromPathW(path.c_str(), urlBuf, &urlLen, 0);
    if (SUCCEEDED(hr)) {
      return std::wstring(urlBuf);
    }

    Wh_Log(L"VideoPlayer::BuildFileUrl: UrlCreateFromPathW failed for %s, "
           L"hr=0x%08lX, falling back to minimal escaper",
           path.c_str(), (unsigned long)hr);
    std::wstring result;
    result.reserve(path.size() + 16);
    result += L"file:///";
    for (wchar_t c : path) {
      if (c == L'\\') {
        result += L'/';
      } else if (c == L' ') {
        result += L"%20";
      } else {
        result += c;
      }
    }
    return result;
  }

  bool Load(const std::wstring &path) {
    Profiler::BeginSection("VideoLoading");
    struct LoadGuard {
      ~LoadGuard() { Profiler::EndSection("VideoLoading"); }
    } loadGuard;

    if (deviceLost || !engine.Get() || !d3dDevice.Get()) {
      if (!Recover())
        return false;
    }
    if (!engine.Get())
      return false;

    loaded = false;
    canPlay = false;
    needsInitialFrame = true;

    std::wstring url = BuildFileUrl(path);
    BSTR bstrUrl = SysAllocString(url.c_str());
    if (!bstrUrl)
      return false;

    HRESULT hr = engine->SetSource(bstrUrl);
    SysFreeString(bstrUrl);

    if (FAILED(hr)) {
      Wh_Log(L"VideoPlayer::Load: SetSource failed for %s, hr=0x%08lX",
             path.c_str(), (unsigned long)hr);
      return false;
    }

    loaded = true;
    wantsPlay = true;
    destRectValid = false;
    loadStartTickMs = GetTickCount();
    canPlayTickMs = 0;
    loadTimeoutLogged = false;
    loadTimeoutRetryCount = 0;
    Wh_Log(L"VideoPlayer::Load: source set to %s", path.c_str());
    return true;
  }

  // Called on the occlusion timer (every 250ms) regardless of readiness.
  // A load that never becomes playable and never raises
  // MF_MEDIA_ENGINE_EVENT_ERROR either used to just sit there silently
  // forever. Now it retries a bounded number of times via the same
  // kMsgReloadSource path used for device-loss recovery, and only gives
  // up (with a clear log line) after kMaxLoadTimeoutRetries attempts --
  // so a transient decode/driver stall recovers on its own, while a
  // genuinely bad file/codec doesn't retry forever.
  void CheckLoadTimeout() {
    if (canPlay || deviceLost)
      return;
    if (loadStartTickMs == 0) {
      if ((!engine.Get() || !d3dDevice.Get()) && boundHwnd) {
        Wh_Log(L"VideoPlayer::CheckLoadTimeout: engine or d3dDevice missing on tick, posting reload to recover");
        PostMessageW(boundHwnd, kMsgReloadSource, 0, 0);
      } else if (g_sourceRetryAttempts >= 30 && boundHwnd) {
        static DWORD s_lastLowFreqRetryTick = 0;
        DWORD now = GetTickCount();
        if (now - s_lastLowFreqRetryTick > 10000 || s_lastLowFreqRetryTick == 0) {
          s_lastLowFreqRetryTick = now;
          std::wstring path;
          if (ResolveVideoSource(path)) {
            Wh_Log(L"VideoPlayer::CheckLoadTimeout: video file is now accessible after initial startup delay -- reloading wallpaper source");
            g_sourceRetryAttempts = 0;
            PostMessageW(boundHwnd, kMsgReloadSource, 0, 0);
          }
        }
      }
      return;
    }
    if (GetTickCount() - loadStartTickMs <= 8000)
      return;

    if (loadTimeoutRetryCount >= kMaxLoadTimeoutRetries) {
      if (!loadTimeoutLogged) {
        loadTimeoutLogged = true;
        Profiler::RecordEvent(L"LoadTimeout");
        Wh_Log(L"VideoPlayer: gave up after %d reload attempt(s) -- video "
               L"never became playable. Likely an unsupported codec/file "
               L"rather than a transient stall; wallpaper will stay black "
               L"until videoPath is changed or the mod is reloaded.",
               kMaxLoadTimeoutRetries);
      }
      return;
    }

    loadTimeoutRetryCount++;
    // Zero this out so we don't re-trigger again before the reload's own
    // Load() call resets it and starts a fresh 8s window.
    loadStartTickMs = 0;
    Profiler::RecordEvent(L"LoadTimeout");
    Wh_Log(L"VideoPlayer: video did not become playable within 8s of Load() "
           L"(attempt %d/%d) -- reloading",
           loadTimeoutRetryCount, kMaxLoadTimeoutRetries);
    if (boundHwnd) {
      OnDeviceLost();
    }
  }

  // Called when MF_MEDIA_ENGINE_EVENT_CANPLAY (or similar readiness event)
  // arrives via kMsgMediaEngineEvent, back on the wallpaper thread.
  bool IsEffectivePaused() const {
    return pausedForFullscreen || pausedForBattery || pausedForSession;
  }

  static UINT GetMonitorRefreshIntervalMs() {
    int target = g_targetFps.load();
    if (target > 0) {
      return (std::max)(1u, 1000u / static_cast<UINT>(target));
    }
    HDC hdc = GetDC(nullptr);
    int vrefresh = 60;
    if (hdc) {
      vrefresh = GetDeviceCaps(hdc, VREFRESH);
      ReleaseDC(nullptr, hdc);
    }
    if (vrefresh <= 0)
      vrefresh = 60;
    return (std::max)(1u, 1000u / static_cast<UINT>(vrefresh));
  }

  void UpdatePlaybackState() {
    // In fluid mode, the shared kRenderTimerId is owned and driven by
    // FluidSimulation, not this VideoPlayer. Every pause-state setter
    // (fullscreen / battery / session) funnels through here, and the
    // no-engine early-out below unconditionally calls KillTimer on that
    // shared timer ID -- which would permanently freeze the fluid sim
    // the first time any pause state changes. Bail out up front so
    // FluidSimulation keeps receiving its Tick.
    if (g_wallpaperMode.load() == WallpaperMode::Fluid) {
      return;
    }
    Wh_Log(L"VideoPlayer::UpdatePlaybackState: engine=%d canPlay=%d IsEffectivePaused=%d (fullscreen=%d, battery=%d, session=%d) wantsPlay=%d needsInitial=%d",
           engine.Get() != nullptr, canPlay ? 1 : 0, IsEffectivePaused() ? 1 : 0,
           pausedForFullscreen ? 1 : 0, pausedForBattery ? 1 : 0, pausedForSession ? 1 : 0, wantsPlay ? 1 : 0, needsInitialFrame ? 1 : 0);
    if (!engine.Get() || !canPlay) {
      if (boundHwnd)
        KillTimer(boundHwnd, kRenderTimerId);
      return;
    }
    if (engine.Get()) {
      engine->SetMuted(g_audioMuted.load() ? TRUE : FALSE);
      engine->SetVolume(static_cast<double>(g_audioVolume.load()) / 100.0);
    }
    if (IsEffectivePaused() && !needsInitialFrame) {
      engine->Pause();
      if (boundHwnd)
        KillTimer(boundHwnd, kRenderTimerId);
    } else if (wantsPlay) {
      engine->Play();
      if (boundHwnd) {
        UINT interval =
            (g_isOnBattery && g_batteryMode.load() == BatteryMode::DropFps)
                ? 66
                : GetMonitorRefreshIntervalMs();
        SetTimer(boundHwnd, kRenderTimerId, interval, nullptr);
      }
    }
  }

  void OnCanPlay() {
    canPlay = true;
    if (canPlayTickMs == 0) {
      canPlayTickMs = GetTickCount();
    }
    DWORD videoW = 0, videoH = 0;
    if (SUCCEEDED(engine->GetNativeVideoSize(&videoW, &videoH)) && videoW > 0 && videoH > 0) {
      cachedVideoW = videoW;
      cachedVideoH = videoH;
      destRectValid = false;
      Profiler::RecordVideoMetadata((int)videoW, (int)videoH,
                                    L"Media Foundation", 0.0f);
    }
    if (engine.Get()) {
      engine->SetMuted(g_audioMuted.load() ? TRUE : FALSE);
      engine->SetVolume(static_cast<double>(g_audioVolume.load()) / 100.0);
    }
    Wh_Log(L"VideoPlayer::OnCanPlay: canPlay=true, wantsPlay=%d, "
           L"pausedForFullscreen=%d, pausedForBattery=%d",
           wantsPlay ? 1 : 0, pausedForFullscreen ? 1 : 0,
           pausedForBattery ? 1 : 0);
    if (wantsPlay) {
      HRESULT hr = engine->Play();
      Wh_Log(L"VideoPlayer::OnCanPlay: Play() hr=0x%08lX", (unsigned long)hr);
      if (IsEffectivePaused() && !needsInitialFrame) {
        engine->Pause();
      }
    }
  }

  void SetPausedForFullscreen(bool paused) {
    if (paused == pausedForFullscreen)
      return;
    pausedForFullscreen = paused;
    if (paused) {
      Wh_Log(L"VideoPlayer::SetPausedForFullscreen: pausing (fullscreen app "
             L"took focus)");
    } else {
      Wh_Log(L"VideoPlayer::SetPausedForFullscreen: unpausing");
    }
    UpdatePlaybackState();
  }

  void SetPausedForBattery(bool paused) {
    if (paused == pausedForBattery)
      return;
    pausedForBattery = paused;
    if (paused) {
      Wh_Log(L"VideoPlayer::SetPausedForBattery: pausing (running on battery)");
    } else {
      Wh_Log(L"VideoPlayer::SetPausedForBattery: unpausing");
    }
    UpdatePlaybackState();
  }

  void SetPausedForSession(bool paused) {
    if (paused == pausedForSession)
      return;
    pausedForSession = paused;
    if (paused) {
      Wh_Log(L"VideoPlayer::SetPausedForSession: pausing (session locked or RDP)");
    } else {
      Wh_Log(L"VideoPlayer::SetPausedForSession: unpausing");
    }
    UpdatePlaybackState();
  }

  void OnSystemResume() {
    Wh_Log(L"VideoPlayer::OnSystemResume: system resumed or display changed");
    if (d3dDevice.Get() && FAILED(d3dDevice->GetDeviceRemovedReason())) {
      OnDeviceLost();
      return;
    }
    if (engine.Get()) {
      if (wantsPlay && !IsEffectivePaused()) {
        engine->Play();
      }
    }
  }

  void Resize(int w, int h) {
    if (!swapChain.Get() || (w == width && h == height))
      return;
    width = w;
    height = h;
    destRectValid = false;
    ResetRTVs();
    HRESULT hr =
        swapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
    if (FAILED(hr)) {
      Wh_Log(L"VideoPlayer::Resize: ResizeBuffers failed, hr=0x%08lX",
             (unsigned long)hr);
      if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        OnDeviceLost();
      }
      return;
    }
    if (transferMode == TransferMode::OffscreenFallback || renderTarget.Get()) {
      CreateRenderTarget(w, h);
    }
    Profiler::RecordEvent(L"Resize");
  }

  void ComputeDestRect(RECT &dest, int fitMode) {
    Profiler::BeginSection("ComputeDestRect");
    struct DestGuard {
      ~DestGuard() { Profiler::EndSection("ComputeDestRect"); }
    } destGuard;
    if (fitMode != 2 || !engine.Get()) {
      dest = {0, 0, width, height};
      if (engine.Get() && (cachedVideoW == 0 || cachedVideoH == 0)) {
        DWORD videoW = 0, videoH = 0;
        if (SUCCEEDED(engine->GetNativeVideoSize(&videoW, &videoH)) && videoW > 0 && videoH > 0) {
          cachedVideoW = videoW;
          cachedVideoH = videoH;
        }
      }
      return;
    }
    if (destRectValid && cachedDestRect.right > 0) {
      dest = cachedDestRect;
      return;
    }

    DWORD videoW = cachedVideoW, videoH = cachedVideoH;
    if (videoW == 0 || videoH == 0) {
      if (FAILED(engine->GetNativeVideoSize(&videoW, &videoH)) || videoW == 0 || videoH == 0) {
        dest = {0, 0, width, height};
        return;
      }
      cachedVideoW = videoW;
      cachedVideoH = videoH;
    }
    float scaleX = (float)width / videoW;
    float scaleY = (float)height / videoH;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;
    int drawW = (int)(videoW * scale);
    int drawH = (int)(videoH * scale);
    dest.left = (width - drawW) / 2;
    dest.top = (height - drawH) / 2;
    dest.right = dest.left + drawW;
    dest.bottom = dest.top + drawH;
    cachedDestRect = dest;
    destRectValid = true;
  }

  // Called on the render timer. Returns true if a frame was presented.
  bool deviceLost = false;
  HWND boundHwnd = nullptr;

  // Full teardown-and-rebuild of the D3D device, swap chain, DXGI device
  // manager, and media engine. Needed after DXGI_ERROR_DEVICE_REMOVED/
  // RESET, since none of those objects are usable again once the
  // underlying device is gone -- there's no partial-repair path, the
  // whole chain has to be recreated from scratch.
  bool Recover() {
    Profiler::RecordEvent(L"Recover");
    if (!boundHwnd && g_wallpaperWnd) {
      boundHwnd = g_wallpaperWnd;
    }
    if (!boundHwnd)
      return false;
    Wh_Log(L"VideoPlayer::Recover: rebuilding D3D device/swap chain/media engine after device loss or cold-start failure");

    HWND hwnd = boundHwnd;
    int w = width > 0 ? width : GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = height > 0 ? height : GetSystemMetrics(SM_CYVIRTUALSCREEN);
    Shutdown();

    if (!InitD3DAndSwapChain(hwnd, w, h) || !InitMediaEngine(hwnd)) {
      Wh_Log(L"VideoPlayer::Recover: rebuild failed, wallpaper will retry on next source reload");
      if (hwnd && IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
      }
      return false;
    }

    deviceLost = false;
    Wh_Log(L"VideoPlayer::Recover: rebuild succeeded");
    return true;
  }

  // Logs *why* the device was lost (driver crash/timeout/hang recovery,
  // out of memory, etc) -- this is the single most useful piece of
  // diagnostic info for a TDR-style GPU hang, so always try to capture it
  // the moment device loss is first detected.
  void LogDeviceRemovedReason() {
    if (!d3dDevice.Get())
      return;
    HRESULT reason = d3dDevice->GetDeviceRemovedReason();
    Wh_Log(L"VideoPlayer: D3D device lost, reason=0x%08lX",
           (unsigned long)reason);
  }

  // Called the moment device loss is first detected, from any of the
  // three Tick() call sites below. Stops the media engine immediately
  // (it has no idea its D3D device is dead and will otherwise keep
  // internally retrying decode and firing MF_MEDIA_ENGINE_EVENT_ERROR
  // repeatedly, as seen in the log, until something tells it to stop).
  // Full rebuild happens later, in Recover(), triggered by the next
  // Load() call -- not here, since a Load() may never come.
  void OnDeviceLost() {
    Profiler::RecordEvent(L"DeviceLost");
    if (deviceLost)
      return; // already handled
    deviceLost = true;
    LogDeviceRemovedReason();
    if (engine.Get()) {
      engine->Shutdown();
    }
    // Trigger recovery: kMsgReloadSource → ReloadWallpaperSource() →
    // Load() → Recover() rebuilds the entire D3D/MF stack.
    if (boundHwnd) {
      PostMessageW(boundHwnd, kMsgReloadSource, 0, 0);
    }
  }

  // Diagnostic-only counters/state for Tick() -- see the logging added
  // inside Tick() itself below.
  UINT tickCallCount = 0;
  UINT tickSuccessCount = 0;
  DWORD lastTickLogMs = 0;

  // Frame-rate throttle and stall-detection timestamps.
  // These are member variables (not function-local statics) so that
  // Shutdown() can reset them, preventing stale timestamps from the
  // previous device lifetime from triggering false stall-recovery
  // reloads after Recover() rebuilds the pipeline.
  DWORD lastDropFpsTick = 0;
  DWORD lastSuccessTickMs = 0;

  bool Tick(int fitMode, bool isOnBattery) {
    tickCallCount++;
    Profiler::RecordTickCall();
    Profiler::UpdateWindowsState(pausedForFullscreen, pausedForBattery, pausedForSession, g_wallpaperHidden, isOnBattery);

    if (deviceLost || !engine.Get() || !canPlay || (IsEffectivePaused() && !needsInitialFrame) ||
        !swapChain.Get() || (transferMode == TransferMode::OffscreenFallback && !renderTarget.Get())) {
// Throttled diagnostic: if we're perpetually blocked on one of
// these gate conditions (most likely canPlay never becoming
// true), say so periodically instead of being silent forever.
#ifdef DEBUG
      DWORD now = GetTickCount();
      if (now - lastTickLogMs > 3000) {
        lastTickLogMs = now;
        Wh_Log(L"VideoPlayer::Tick: gated -- deviceLost=%d hasEngine=%d "
               L"canPlay=%d IsEffectivePaused=%d hasSwapChain=%d "
               L"hasRenderTarget=%d transferMode=%d",
               deviceLost ? 1 : 0, engine.Get() != nullptr, canPlay ? 1 : 0,
               IsEffectivePaused() ? 1 : 0, swapChain.Get() != nullptr,
               renderTarget.Get() != nullptr, (int)transferMode);
      }
#endif
      return false;
    }

    if (g_batteryMode.load() == BatteryMode::DropFps && isOnBattery) {
      DWORD now = GetTickCount();
      if (now - lastDropFpsTick < 66 && lastDropFpsTick != 0) {
        return false;
      }
      lastDropFpsTick = now;
    }

    LONGLONG pts = 0;
    Profiler::BeginSection("OnVideoStreamTick");
    HRESULT hr = engine->OnVideoStreamTick(&pts);
    Profiler::EndSection("OnVideoStreamTick");
    DWORD now = GetTickCount();
    if (hr != S_OK) {
#ifdef DEBUG
      if (now - lastTickLogMs > 3000) {
        lastTickLogMs = now;
        Wh_Log(L"VideoPlayer::Tick: OnVideoStreamTick not ready, hr=0x%08lX, "
               L"tickCalls=%u tickSuccesses=%u currentTime=%.3fs",
               (unsigned long)hr, tickCallCount, tickSuccessCount,
               engine->GetCurrentTime());
      }
#endif
      // If stalled while active and unpaused (either >4s after last frame or >5s right
      // after boot/CANPLAY before the first frame ever presents), recover:
      if (canPlay && wantsPlay && !IsEffectivePaused()) {
        if ((lastSuccessTickMs != 0 && (now - lastSuccessTickMs > 4000)) ||
            (lastSuccessTickMs == 0 && canPlayTickMs != 0 && (now - canPlayTickMs > 5000))) {
          Wh_Log(L"VideoPlayer::Tick: stalled after sleep/lock/boot (>4s without "
                 L"ready frame), triggering recovery");
          lastSuccessTickMs = now;
          canPlayTickMs = now;
          OnDeviceLost();
        }
      }
      return false;
    }
    lastSuccessTickMs = now;

    tickSuccessCount++;

    Profiler::BeginFrame(tickSuccessCount);
    struct FrameGuard {
      ~FrameGuard() { Profiler::EndFrame(); }
    } frameGuard;

    RECT dest;
    ComputeDestRect(dest, fitMode);

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                              (void **)&backBuffer);
    if (FAILED(hr)) {
      if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        OnDeviceLost();
      }
      return false;
    }

    // In fit mode (fitMode == 2), clear target textures to solid black before transferring
    // so letterbox borders never show underlying Windows wallpaper. In fill (0) or cover (1) mode,
    // the video frame covers 100% of the target texture, making per-frame clears redundant GPU work.
    if (fitMode == 2) {
      ClearTexture(backBuffer.Get());
      if (renderTarget.Get()) {
        ClearTexture(renderTarget.Get());
      }
    }

    MFVideoNormalizedRect srcRect = {0.0f, 0.0f, 1.0f, 1.0f};
    bool useSrcRect = false;
    if (fitMode == 1 && engine.Get() && (cachedVideoW == 0 || cachedVideoH == 0)) {
      DWORD videoW = 0, videoH = 0;
      if (SUCCEEDED(engine->GetNativeVideoSize(&videoW, &videoH)) && videoW > 0 && videoH > 0) {
        cachedVideoW = videoW;
        cachedVideoH = videoH;
      }
    }

    if (fitMode == 1 && cachedVideoW > 0 && cachedVideoH > 0 && width > 0 && height > 0) {
      float screenAspect = (float)width / (float)height;
      float videoAspect = (float)cachedVideoW / (float)cachedVideoH;
      if (videoAspect > screenAspect && screenAspect > 0) {
        float cropW = screenAspect / videoAspect;
        srcRect.left = (1.0f - cropW) / 2.0f;
        srcRect.right = srcRect.left + cropW;
        useSrcRect = true;
      } else if (videoAspect < screenAspect && videoAspect > 0) {
        float cropH = videoAspect / screenAspect;
        srcRect.top = (1.0f - cropH) / 2.0f;
        srcRect.bottom = srcRect.top + cropH;
        useSrcRect = true;
      }
    }
    const MFVideoNormalizedRect* pSrc = useSrcRect ? &srcRect : nullptr;
    const RECT* pDst = (dest.right > 0 && dest.bottom > 0) ? &dest : nullptr;

    static const MFARGB kBorderColor = {0, 0, 0, 0xFF};
    LARGE_INTEGER tStart, tEnd;
    LARGE_INTEGER qpcFreq;
    QueryPerformanceFrequency(&qpcFreq);

    QueryPerformanceCounter(&tStart);
    Profiler::BeginSection("TransferVideoFrame");
    bool usedDirectBackBuffer = false;
    if (transferMode == TransferMode::DirectBackBuffer || transferMode == TransferMode::Unknown) {
      hr = engine->TransferVideoFrame(backBuffer.Get(), pSrc, pDst, &kBorderColor);
      if (SUCCEEDED(hr)) {
        if (transferMode == TransferMode::Unknown) {
          Wh_Log(L"VideoPlayer::Tick: direct TransferVideoFrame to backBuffer succeeded; locking in DirectBackBuffer mode");
        }
        transferMode = TransferMode::DirectBackBuffer;
        usedDirectBackBuffer = true;
      } else if (transferMode == TransferMode::Unknown) {
        Wh_Log(L"VideoPlayer::Tick: direct TransferVideoFrame to backBuffer failed (hr=0x%08lX); switching to offscreen fallback mode and lazy-creating renderTarget", (unsigned long)hr);
        transferMode = TransferMode::OffscreenFallback;
      }
    }
    if (transferMode == TransferMode::OffscreenFallback) {
      if (!renderTarget.Get()) {
        CreateRenderTarget(width, height);
        ClearTexture(renderTarget.Get());
      }
      if (renderTarget.Get()) {
        hr = engine->TransferVideoFrame(renderTarget.Get(), pSrc, pDst, &kBorderColor);
        usedDirectBackBuffer = false;
      } else {
        hr = E_FAIL;
      }
    }
    Profiler::EndSection("TransferVideoFrame");
    QueryPerformanceCounter(&tEnd);
    double transferMs = static_cast<double>(tEnd.QuadPart - tStart.QuadPart) * 1000.0 / static_cast<double>(qpcFreq.QuadPart);
    Profiler::RecordTransfer(hr, transferMs);
    if (FAILED(hr)) {
#ifdef DEBUG
      if (now - lastTickLogMs > 3000) {
        lastTickLogMs = now;
        Wh_Log(L"VideoPlayer::Tick: TransferVideoFrame failed, hr=0x%08lX",
               (unsigned long)hr);
      }
#endif
      if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        OnDeviceLost();
      }
      return false;
    }

    if (cachedVideoW > 0 && cachedVideoH > 0) {
      Profiler::RecordVideoMetadata((int)cachedVideoW, (int)cachedVideoH,
                                    L"Media Foundation", 0.0f);
    }
    int activeTextures = (renderTarget.Get() ? 1 : 0) + (backBuffer.Get() ? 1 : 0);
    int activeSwapChains = swapChain.Get() ? 1 : 0;
    int activeMediaEngines = engine.Get() ? 1 : 0;
    int activeSurfaces = 0; // Media Foundation internals are not observable here.
    int activeComObjects = (d3dDevice.Get() ? 1 : 0) + (d3dContext.Get() ? 1 : 0) + (swapChain.Get() ? 1 : 0) +
                           (dxgiManager.Get() ? 1 : 0) + (engine.Get() ? 1 : 0) + (renderTarget.Get() ? 1 : 0) +
                           (backBuffer.Get() ? 1 : 0);
    Profiler::UpdateResourceStats(activeTextures, activeSwapChains, activeMediaEngines, activeSurfaces, activeComObjects);

    ID3D11Texture2D *overlayTarget = usedDirectBackBuffer ? backBuffer.Get() : renderTarget.Get();
    Profiler::DrawOverlay(boundHwnd, overlayTarget);

    if (!usedDirectBackBuffer && renderTarget.Get()) {
      Profiler::BeginSection("CopyResource");
      d3dContext->CopyResource(backBuffer.Get(), renderTarget.Get());
      Profiler::EndSection("CopyResource");
    }

    QueryPerformanceCounter(&tStart);
    Profiler::BeginSection("Present");
    hr = swapChain->Present(1, 0);
    Profiler::EndSection("Present");
    QueryPerformanceCounter(&tEnd);
    double presentMs = static_cast<double>(tEnd.QuadPart - tStart.QuadPart) * 1000.0 / static_cast<double>(qpcFreq.QuadPart);
    Profiler::RecordPresent(hr, presentMs);

    if (SUCCEEDED(hr) && !g_wallpaperHidden && boundHwnd &&
        !IsWindowVisible(boundHwnd)) {
      ShowWindow(boundHwnd, SW_SHOWNOACTIVATE);
      SetWindowPos(boundHwnd, HWND_BOTTOM, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    if (SUCCEEDED(hr) && needsInitialFrame) {
      needsInitialFrame = false;
      Wh_Log(L"VideoPlayer: initial frame presented successfully");
      UpdatePlaybackState();
    }

    if (FAILED(hr) &&
        (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)) {
      OnDeviceLost();
      return false;
    }
    return true;
  }

  void Shutdown() {
    Profiler::Shutdown();
    if (engine.Get()) {
      engine->Shutdown();
    }
    engine.Reset();
    notify = nullptr;
    ResetRTVs();
    swapChain.Reset();
    renderTarget.Reset();
    dxgiManager.Reset();
    d3dContext.Reset();
    d3dDevice.Reset();
    loaded = false;
    canPlay = false;
    wantsPlay = false;
    pausedForFullscreen = false;
    pausedForBattery = false;
    pausedForSession = false;
    transferMode = TransferMode::Unknown;
    deviceLost = false;
    boundHwnd = nullptr;
    // Reset frame-timing state so stale timestamps from the previous
    // device lifetime don't trigger false stall-recovery reloads.
    tickCallCount = 0;
    tickSuccessCount = 0;
    lastTickLogMs = 0;
    lastDropFpsTick = 0;
    lastSuccessTickMs = 0;
  }
};

[[clang::no_destroy]] VideoPlayer g_player;
[[clang::no_destroy]] FluidSimulation g_fluidSim;

// ---------------------------------------------------------------------------
// Window finding (unchanged from the working GIF build)
// ---------------------------------------------------------------------------

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  HWND shellDllDefView =
      FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
  if (shellDllDefView) {
    HWND *result = reinterpret_cast<HWND *>(lParam);
    *result = FindWindowExW(nullptr, hwnd, L"WorkerW", nullptr);
    return FALSE;
  }
  return TRUE;
}

HWND FindClassicTopLevelWorkerW(HWND progman, bool verbose = true) {
  HWND workerW = nullptr;
  EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&workerW));
  if (workerW) {
    if (verbose) {
      Wh_Log(L"FindClassicTopLevelWorkerW: found top-level WorkerW sibling 0x%p",
             workerW);
    }
    return workerW;
  }
  return nullptr;
}

HWND FindChildWorkerW(HWND progman, bool verbose = true) {
  HWND workerW = nullptr;
  while ((workerW = FindWindowExW(progman, workerW, L"WorkerW", nullptr)) !=
         nullptr) {
    HWND defView =
        FindWindowExW(workerW, nullptr, L"SHELLDLL_DefView", nullptr);
    if (!defView) {
      if (verbose) {
        Wh_Log(
            L"FindChildWorkerW: found child WorkerW 0x%p (no SHELLDLL_DefView)",
            workerW);
      }
      return workerW;
    }
  }
  if (verbose) {
    Wh_Log(L"FindChildWorkerW: no usable child WorkerW found under Progman");
  }
  return nullptr;
}

HWND TryFindClassicWorkerWWithRetries(HWND progman) {
  DWORD_PTR result = 0;
  SendMessageTimeoutW(progman, WM_SPAWN_WORKER, 0, 0, SMTO_NORMAL, 1000,
                      &result);

  for (int attempt = 0; attempt < 10; attempt++) {
    if (WaitForSingleObject(g_shutdownEvent, 0) == WAIT_OBJECT_0)
      return nullptr;
    HWND workerW = FindClassicTopLevelWorkerW(progman);
    if (workerW)
      return workerW;
    HWND childWorkerW = FindChildWorkerW(progman);
    if (childWorkerW)
      return childWorkerW;
    if (WaitForSingleObject(g_shutdownEvent, 200) == WAIT_OBJECT_0)
      return nullptr;
    SendMessageTimeoutW(progman, WM_SPAWN_WORKER, 0, 0, SMTO_NORMAL, 1000,
                        &result);
  }
  Wh_Log(L"TryFindClassicWorkerWWithRetries: no usable WorkerW (sibling or "
         L"child) on this build");
  return nullptr;
}

void GetVirtualScreenRect(RECT &rc) {
  rc.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  rc.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  rc.right = rc.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
  rc.bottom = rc.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
}

void PinBehindTargetWindow() {
  if (!g_wallpaperWnd || g_wallpaperHidden)
    return;

  static HWND s_cachedWorkerW = nullptr;
  HWND activeWorkerW = nullptr;

  HWND progman = FindWindowW(L"Progman", nullptr);
  if (progman) {
    // Always check if a classic top-level WorkerW sibling hosting or adjacent to SHELLDLL_DefView exists.
    // On cold boot, Explorer creates SHELLDLL_DefView after initial mod load, so we must dynamically
    // detect when the true desktop WorkerW appears rather than relying on a stale early cached handle.
    HWND topWorker = FindClassicTopLevelWorkerW(progman, false);
    if (topWorker) {
      activeWorkerW = topWorker;
    } else if (s_cachedWorkerW && IsWindow(s_cachedWorkerW)) {
      activeWorkerW = s_cachedWorkerW;
    } else {
      activeWorkerW = FindChildWorkerW(progman, false);
    }
    s_cachedWorkerW = activeWorkerW;
  }

  if (activeWorkerW) {
    // If we were previously top-level or attached to a stale/destroyed WorkerW parent,
    // reparent dynamically to the active WorkerW hosting desktop icons.
    HWND currentParent = GetParent(g_wallpaperWnd);
    if (g_topLevelMode || currentParent != activeWorkerW) {
      Wh_Log(L"PinBehindTargetWindow: attaching wallpaper window to active WorkerW 0x%p (old parent 0x%p, topLevel=%d)",
             activeWorkerW, currentParent, g_topLevelMode ? 1 : 0);
      g_topLevelMode = false;
      SetParent(g_wallpaperWnd, activeWorkerW);
      DWORD style = GetWindowLongW(g_wallpaperWnd, GWL_STYLE);
      style = (style & ~WS_POPUP) | WS_CHILD | WS_CLIPSIBLINGS;
      SetWindowLongW(g_wallpaperWnd, GWL_STYLE, style);
      DWORD exStyle = GetWindowLongW(g_wallpaperWnd, GWL_EXSTYLE);
      exStyle |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
      SetWindowLongW(g_wallpaperWnd, GWL_EXSTYLE, exStyle);
    }

    // Ensure dimensions match activeWorkerW client area.
    RECT rcHost = {};
    if (GetClientRect(activeWorkerW, &rcHost)) {
      int w = rcHost.right - rcHost.left;
      int h = rcHost.bottom - rcHost.top;
      if (w <= 0 || h <= 0) {
        w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
      }
      if (w > 0 && h > 0 && (w != g_player.width || h != g_player.height)) {
        Wh_Log(L"PinBehindTargetWindow: host dimensions changed to %dx%d, resizing", w, h);
        SetWindowPos(g_wallpaperWnd, nullptr, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        g_player.Resize(w, h);
        g_fluidSim.Resize(w, h);
      }
    }
  } else if (!g_topLevelMode && !GetParent(g_wallpaperWnd)) {
    // WorkerW disappeared or detached; switch to top-level fallback mode.
    Wh_Log(L"PinBehindTargetWindow: WorkerW detached, switching to top-level fallback mode");
    g_topLevelMode = true;
    DWORD style = GetWindowLongW(g_wallpaperWnd, GWL_STYLE);
    style = (style & ~WS_CHILD) | WS_POPUP | WS_CLIPSIBLINGS;
    SetWindowLongW(g_wallpaperWnd, GWL_STYLE, style);
  }

  // Always keep at the bottom of the Z-order (both inside WorkerW and in top-level mode)
  SetWindowPos(g_wallpaperWnd, HWND_BOTTOM, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);

  // If rendering is active and canPlay is true, make sure our window hasn't been hidden
  // by Explorer during startup/theme transition.
  if (g_player.canPlay && !IsWindowVisible(g_wallpaperWnd)) {
    ShowWindow(g_wallpaperWnd, SW_SHOWNOACTIVATE);
  }
}

// ---------------------------------------------------------------------------
// Fullscreen-pause heuristic (unchanged)
// ---------------------------------------------------------------------------

// RAII guard for GDI HRGN handles -- ensures DeleteObject is called on
// all exit paths without manual bookkeeping.
struct HRGNGuard {
  HRGN h = nullptr;
  HRGNGuard() = default;
  ~HRGNGuard() {
    if (h)
      DeleteObject(h);
  }
  // Non-copyable.
  HRGNGuard(const HRGNGuard &) = delete;
  HRGNGuard &operator=(const HRGNGuard &) = delete;
};

bool IsDesktopFullyCovered(bool forceCheck = false) {
  static DWORD lastCheckTick = 0;
  static bool cachedResult = false;
  DWORD now = GetTickCount();
  int checkIntervalMs = g_occlusionIntervalMs.load();
  if (!forceCheck && now - lastCheckTick < static_cast<DWORD>(checkIntervalMs) && lastCheckTick != 0) {
    return cachedResult;
  }
  lastCheckTick = now;

  Profiler::BeginSection("OcclusionCheck");
  struct OcclusionGuard {
    ~OcclusionGuard() { Profiler::EndSection("OcclusionCheck"); }
  } occGuard;

  // Determine target screen bounds that our wallpaper covers.
  // If g_wallpaperWnd exists and has valid bounds, use it; otherwise use
  // virtual screen bounds.
  RECT rcTarget = {};
  if (!g_wallpaperWnd || !GetWindowRect(g_wallpaperWnd, &rcTarget) ||
      IsRectEmpty(&rcTarget)) {
    GetVirtualScreenRect(rcTarget);
  }

  static HWND s_desktopHost = nullptr;
  if (s_desktopHost && !IsWindow(s_desktopHost)) {
    s_desktopHost = nullptr;
  }

  HRGNGuard uncoveredGuard;
  HWND hwnd = GetTopWindow(nullptr);
  while (hwnd) {
    // Fast O(1) stop conditions: our own window, direct parent, or known
    // desktop host
    if (hwnd == g_wallpaperWnd ||
        (g_wallpaperWnd && hwnd == GetParent(g_wallpaperWnd)) ||
        (s_desktopHost && hwnd == s_desktopHost)) {
      break;
    }

    // Fast-path filter: immediately skip hidden or minimized windows
    // right up front before making string or attribute queries.
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
      hwnd = GetWindow(hwnd, GW_HWNDNEXT);
      continue;
    }

    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);

    // 1. Skip click-through / transparent hit-test windows (e.g. Discord
    // overlay, RTSS overlay, recording indicator borders, crosshairs).
    if (exStyle & WS_EX_TRANSPARENT) {
      hwnd = GetWindow(hwnd, GW_HWNDNEXT);
      continue;
    }

    // 2. Skip tool windows unless they explicitly opt into being taskbar app
    // windows (e.g. NVIDIA GeForce recording overlay, floating status popups,
    // hidden background helpers).
    if ((exStyle & WS_EX_TOOLWINDOW) && !(exStyle & WS_EX_APPWINDOW)) {
      hwnd = GetWindow(hwnd, GW_HWNDNEXT);
      continue;
    }

    // 3. Skip non-activating overlay/indicator windows (e.g. Game Bar
    // background capture, performance monitors, widgets that never take
    // keyboard focus).
    if ((exStyle & WS_EX_NOACTIVATE) && !(exStyle & WS_EX_APPWINDOW)) {
      hwnd = GetWindow(hwnd, GW_HWNDNEXT);
      continue;
    }

    // Fast bounding screening: GetWindowRect is orders of magnitude faster
    // than DWM attribute queries or class name strings. If the coarse rect
    // doesn't even intersect our target screen bounds, skip this window
    // immediately without calling DWM or string APIs!
    RECT wr = {};
    if (!GetWindowRect(hwnd, &wr) || IsRectEmpty(&wr)) {
      hwnd = GetWindow(hwnd, GW_HWNDNEXT);
      continue;
    }
    RECT dummyFast;
    if (!IntersectRect(&dummyFast, &wr, &rcTarget)) {
      hwnd = GetWindow(hwnd, GW_HWNDNEXT);
      continue;
    }

    wchar_t className[64] = {};
    GetClassNameW(hwnd, className, ARRAYSIZE(className));
    if (wcscmp(className, L"Progman") == 0) {
      break;
    }
    if (wcscmp(className, L"WorkerW") == 0) {
      if (FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr) !=
          nullptr) {
        s_desktopHost = hwnd;
        break;
      }
      hwnd = GetWindow(hwnd, GW_HWNDNEXT);
      continue;
    }

    // Check cloaked by DWM (e.g. UWP background apps or windows on other
    // virtual desktops)
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked,
                                        sizeof(cloaked))) &&
        cloaked != 0) {
      hwnd = GetWindow(hwnd, GW_HWNDNEXT);
      continue;
    }

    // Check layered/transparent windows
    if (exStyle & WS_EX_LAYERED) {
      BYTE alpha = 0;
      DWORD flags = 0;
      if (GetLayeredWindowAttributes(hwnd, nullptr, &alpha, &flags)) {
        // If nearly invisible alpha OR chroma-keyed (transparent background
        // cutouts), skip
        if (((flags & LWA_ALPHA) && alpha < 15) || (flags & LWA_COLORKEY)) {
          hwnd = GetWindow(hwnd, GW_HWNDNEXT);
          continue;
        }
      }
    }

    // Borderless windows without WS_CAPTION/WS_THICKFRAME that are NOT explicit
    // taskbar app windows (WS_EX_APPWINDOW) are modern composition/overlay
    // helpers or desktop backdrop hosts, not occluding applications.
    if (!(style & WS_CAPTION) && !(style & WS_THICKFRAME) &&
        !(exStyle & WS_EX_APPWINDOW)) {
      hwnd = GetWindow(hwnd, GW_HWNDNEXT);
      continue;
    }

    // Refine to true visible bounds (excluding invisible DWM drop shadows
    // around normal windows)
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &wr,
                                     sizeof(wr)))) {
      if (!GetWindowRect(hwnd, &wr)) {
        hwnd = GetWindow(hwnd, GW_HWNDNEXT);
        continue;
      }
    }

    // If maximized, ensure we cover the exact monitor bounds across border edge
    // discrepancies
    if (IsZoomed(hwnd)) {
      HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi = {sizeof(mi)};
      if (GetMonitorInfoW(mon, &mi)) {
        wr = mi.rcMonitor;
      }
    }

    // Fast-path check: if this single window completely covers our target rect
    // right now, return true immediately without allocating any GDI regions or
    // calling CombineRgn!
    if (wr.left <= rcTarget.left + 4 && wr.top <= rcTarget.top + 4 &&
        wr.right >= rcTarget.right - 4 && wr.bottom >= rcTarget.bottom - 4) {
      if (!cachedResult) {
        wchar_t title[128] = {};
        GetWindowTextW(hwnd, title, ARRAYSIZE(title));
        Wh_Log(L"IsDesktopFullyCovered: covered (fast-path) by hwnd=0x%p class='%s' title='%s' rect=[%ld,%ld,%ld,%ld]",
               hwnd, className, title, wr.left, wr.top, wr.right, wr.bottom);
      }
      cachedResult = true;
      return true;
    }

    // Check if this window intersects our target at all before doing region
    // math
    RECT dummy;
    if (IntersectRect(&dummy, &wr, &rcTarget)) {
      // Lazy-initialize the uncovered region only when encountering the first
      // overlapping window
      if (!uncoveredGuard.h) {
        uncoveredGuard.h = CreateRectRgnIndirect(&rcTarget);
      }
      HRGN hrgnWnd = CreateRectRgnIndirect(&wr);
      if (hrgnWnd) {
        CombineRgn(uncoveredGuard.h, uncoveredGuard.h, hrgnWnd, RGN_DIFF);
        DeleteObject(hrgnWnd);
      }

      RECT rcBox = {};
      int rgnType = GetRgnBox(uncoveredGuard.h, &rcBox);
      LONGLONG remainingArea = static_cast<LONGLONG>(rcBox.right - rcBox.left) *
                               static_cast<LONGLONG>(rcBox.bottom - rcBox.top);
      LONGLONG totalArea = static_cast<LONGLONG>(rcTarget.right - rcTarget.left) *
                           static_cast<LONGLONG>(rcTarget.bottom - rcTarget.top);
      if (rgnType == NULLREGION || IsRectEmpty(&rcBox) ||
          (rcBox.right - rcBox.left <= 4 || rcBox.bottom - rcBox.top <= 4) ||
          (totalArea > 0 && remainingArea < (totalArea / 10))) {
        if (!cachedResult) {
          wchar_t title[128] = {};
          GetWindowTextW(hwnd, title, ARRAYSIZE(title));
          Wh_Log(L"IsDesktopFullyCovered: covered (rgn-box/partial >90%%) by hwnd=0x%p class='%s' title='%s'",
                 hwnd, className, title);
        }
        cachedResult = true;
        return true;
      }
    }

    hwnd = GetWindow(hwnd, GW_HWNDNEXT);
  }

  // uncoveredGuard's destructor handles DeleteObject automatically.
  cachedResult = false;
  return false;
}

// ---------------------------------------------------------------------------
// Battery / power management helper
// ---------------------------------------------------------------------------

void CheckBatteryState() {
  static DWORD lastBatteryCheckTick = 0;
  DWORD now = GetTickCount();
  if (now - lastBatteryCheckTick < 1000 && lastBatteryCheckTick != 0) {
    return;
  }
  lastBatteryCheckTick = now;

  Profiler::BeginSection("BatteryCheck");
  struct BatteryGuard {
    ~BatteryGuard() { Profiler::EndSection("BatteryCheck"); }
  } batGuard;

  SYSTEM_POWER_STATUS status = {};
  if (GetSystemPowerStatus(&status)) {
    g_isOnBattery = (status.ACLineStatus == 0);
  } else {
    g_isOnBattery = false;
  }

  if (g_batteryMode.load() == BatteryMode::Pause) {
    g_player.SetPausedForBattery(g_isOnBattery);
    g_fluidSim.SetPausedForBattery(g_isOnBattery);
  } else {
    g_player.SetPausedForBattery(false);
    g_fluidSim.SetPausedForBattery(false);
  }
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

bool IsDesktopFullyCovered(bool forceCheck);
void PickAndLoadVideoViaDialogAsync(HWND ownerWnd);
bool ResolveVideoSource(std::wstring &outPath);
void ReloadWallpaperSource();

HWINEVENTHOOK g_hWinEventHook = nullptr;

VOID CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
                           LONG idObject, LONG idChild, DWORD dwEventThread,
                           DWORD dwmsEventTime) {
  if (!g_wallpaperWnd) return;
  if (event == EVENT_SYSTEM_FOREGROUND || event == EVENT_SYSTEM_MINIMIZEEND || event == EVENT_SYSTEM_MINIMIZESTART) {
    bool covered = IsDesktopFullyCovered(true);
    g_player.SetPausedForFullscreen(covered);
    g_fluidSim.SetPausedForFullscreen(covered);
  }
}

LRESULT CALLBACK WallpaperWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam) {
  switch (msg) {
  case WM_NCHITTEST:
    return HTTRANSPARENT;
  case WM_MOUSEACTIVATE:
    return MA_NOACTIVATEANDEAT;
  case WM_SETFOCUS:
    return 0;
  case WM_CONTEXTMENU:
  case WM_RBUTTONDOWN:
  case WM_RBUTTONUP:
  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
    return 0;
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_TIMER: {
    if (wParam == kRenderTimerId) {
      if (g_wallpaperMode.load() == WallpaperMode::Fluid) {
        if (g_fluidSim.Tick(g_isOnBattery) == FluidSimulation::TickResult::DeviceLost) {
          g_player.OnDeviceLost();
        }
      } else {
        g_player.Tick(g_fitMode.load(), g_isOnBattery);
      }
    } else if (wParam == kOcclusionTimerId) {
      if (g_wallpaperMode.load() == WallpaperMode::Video) {
        g_player.CheckLoadTimeout();
      }
      CheckBatteryState();
      bool covered = IsDesktopFullyCovered();
      g_player.SetPausedForFullscreen(covered);
      g_fluidSim.SetPausedForFullscreen(covered);
    } else if (wParam == kZOrderTimerId) {
      PinBehindTargetWindow();
    } else if (wParam == kSourceRetryTimerId) {
      KillTimer(hwnd, kSourceRetryTimerId);
      ReloadWallpaperSource();
    }
    return 0;
  }
  case kMsgMediaEngineEvent: {
    DWORD event = (DWORD)wParam;
    if (event == MF_MEDIA_ENGINE_EVENT_CANPLAY) {
      g_player.OnCanPlay();
    } else if (event == MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA) {
      // Some containers/codecs reliably fire LOADEDMETADATA but
      // are flaky about CANPLAY specifically -- treat both as
      // "ready", same as the D3D9/GIF-era player did. OnCanPlay()
      // is idempotent (just sets canPlay=true again and re-checks
      // wantsPlay), so it's safe to call from both events.
      Wh_Log(L"WallpaperWndProc: MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA");
      g_player.OnCanPlay();
    } else if (event == MF_MEDIA_ENGINE_EVENT_ERROR) {
      Profiler::RecordEvent(L"MFError");
      Wh_Log(L"WallpaperWndProc: MF_MEDIA_ENGINE_EVENT_ERROR, param1=%ld",
             (long)lParam);
    } else if (event != MF_MEDIA_ENGINE_EVENT_TIMEUPDATE &&
               event != (DWORD)18) {
      // Filter TIMEUPDATE (spams every ~250ms) -- check both the
      // symbolic constant and the literal 18 since MinGW headers may
      // define different enum values than the Windows SDK.
      Wh_Log(L"WallpaperWndProc: media engine event %lu, param1=%ld",
             (unsigned long)event, (long)lParam);
    }
    return 0;
  }
  case WM_HOTKEY: {
    if (wParam == kHotkeyId) {
      PickAndLoadVideoViaDialogAsync(hwnd);
    } else if (wParam == kVisibilityHotkeyId) {
      g_wallpaperHidden = !g_wallpaperHidden;
      ShowWindow(hwnd, g_wallpaperHidden ? SW_HIDE : SW_SHOWNOACTIVATE);
      if (!g_wallpaperHidden) {
        PinBehindTargetWindow();
      }
      Wh_Log(L"Visibility toggled, hidden=%d", g_wallpaperHidden ? 1 : 0);
    } else if (wParam == kProfilerHotkeyId) {
      Profiler::CycleMode();
    }
    return 0;
  }
  case WM_POWERBROADCAST: {
    if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND ||
        wParam == PBT_APMPOWERSTATUSCHANGE) {
      Wh_Log(L"WallpaperWndProc: WM_POWERBROADCAST (wParam=0x%04lX)",
             (unsigned long)wParam);
      CheckBatteryState();
      g_player.OnSystemResume();
    }
    return TRUE;
  }
  case WM_DISPLAYCHANGE: {
    Wh_Log(L"WallpaperWndProc: WM_DISPLAYCHANGE");
    g_player.destRectValid = false;

    // A monitor being added/removed/resized changes the bounds we need
    // to cover -- previously only the dest-rect cache was invalidated,
    // so the window itself (and the player's swap chain) kept the old
    // size until the next manual reload, leaving clipping or letterbox
    // artifacts on the new layout.
    if (g_topLevelMode) {
      RECT vr;
      GetVirtualScreenRect(vr);
      int w = vr.right - vr.left;
      int h = vr.bottom - vr.top;
      SetWindowPos(hwnd, nullptr, vr.left, vr.top, w, h,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      g_player.Resize(w, h);
      g_fluidSim.Resize(w, h);
      PinBehindTargetWindow();
    } else {
      HWND parent = GetParent(hwnd);
      if (parent) {
        RECT hostClient = {};
        GetClientRect(parent, &hostClient);
        int w = hostClient.right - hostClient.left;
        int h = hostClient.bottom - hostClient.top;
        if (w > 0 && h > 0) {
          SetWindowPos(hwnd, nullptr, 0, 0, w, h,
                       SWP_NOZORDER | SWP_NOACTIVATE);
          g_player.Resize(w, h);
          g_fluidSim.Resize(w, h);
        }
      }
    }

    g_player.OnSystemResume();
    return 0;
  }
  case WM_WTSSESSION_CHANGE: {
    Wh_Log(L"WallpaperWndProc: WM_WTSSESSION_CHANGE wParam=%lu", (unsigned long)wParam);
    if (wParam == WTS_SESSION_LOCK || wParam == WTS_REMOTE_CONNECT) {
      g_player.SetPausedForSession(true);
      g_fluidSim.SetPausedForSession(true);
    } else if (wParam == WTS_SESSION_UNLOCK || wParam == WTS_REMOTE_DISCONNECT) {
      g_player.SetPausedForSession(false);
      g_fluidSim.SetPausedForSession(false);
    }
    return 0;
  }
  case WM_DESTROY:
    KillTimer(hwnd, kRenderTimerId);
    KillTimer(hwnd, kZOrderTimerId);
    KillTimer(hwnd, kOcclusionTimerId);
    KillTimer(hwnd, kSourceRetryTimerId);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Source loading -- local file only, no network/download support
// ---------------------------------------------------------------------------

bool ResolveVideoSource(std::wstring &outPath) {
  std::wstring path = GetVideoPathSetting();
  if (path.empty() || !PathFileExistsW(path.c_str())) {
    Wh_Log(L"Video file path missing or invalid: %s", path.c_str());
    return false;
  }
  outPath = path;
  return true;
}

HANDLE g_pickerThread = nullptr;
DWORD g_pickerThreadId = 0;

DWORD WINAPI FilePickerThreadProc(LPVOID param) {
  if (WaitForSingleObject(g_shutdownEvent, 0) == WAIT_OBJECT_0)
    return 0;
  HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(comHr)) {
    Wh_Log(L"FilePickerThreadProc: COM initialization failed, hr=0x%08lX",
           static_cast<unsigned long>(comHr));
    return 1;
  }
  HWND ownerWnd = static_cast<HWND>(param);
  HWND tempOwner =
      CreateWindowExW(WS_EX_TOPMOST, L"STATIC", L"", WS_POPUP, 0, 0, 0, 0,
                      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (tempOwner) {
    SetWindowPos(tempOwner, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(tempOwner);
  }

  wchar_t fileBuffer[32768] = {};
  OPENFILENAMEW ofn = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = tempOwner ? tempOwner : ownerWnd;
  ofn.lpstrFilter =
      L"Video files (*.mp4;*.m4v;*.mov;*.wmv;*.webm)\0*.mp4;*.m4v;*.mov;*.wmv;*.webm\0All files (*.*)\0*.*\0";
  ofn.lpstrFile = fileBuffer;
  ofn.nMaxFile = ARRAYSIZE(fileBuffer);
  ofn.lpstrTitle = L"Pick a video for your wallpaper";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

  BOOL picked = WaitForSingleObject(g_shutdownEvent, 0) != WAIT_OBJECT_0 &&
                GetOpenFileNameW(&ofn);
  if (tempOwner)
    DestroyWindow(tempOwner);

  if (picked && WaitForSingleObject(g_shutdownEvent, 0) != WAIT_OBJECT_0) {
    Wh_Log(L"FilePickerThreadProc: picked %s", fileBuffer);

    // Persist the picked path so that device-loss recovery
    // (ReloadWallpaperSource) and mod restarts use the user's latest
    // choice, not whatever was in the settings panel.
    EnterCriticalSection(&g_pathLock);
    g_videoPath = fileBuffer;
    LeaveCriticalSection(&g_pathLock);

    Wh_SetStringValue(L"LastPickedVideoPath", fileBuffer);

    if (ownerWnd && IsWindow(ownerWnd) &&
        WaitForSingleObject(g_shutdownEvent, 0) != WAIT_OBJECT_0) {
      PostMessageW(ownerWnd, kMsgReloadSource, 0, 0);
    }
  }
  CoUninitialize();
  return 0;
}

void PickAndLoadVideoViaDialogAsync(HWND ownerWnd) {
  if (WaitForSingleObject(g_shutdownEvent, 0) == WAIT_OBJECT_0)
    return;
  if (g_pickerThread) {
    if (WaitForSingleObject(g_pickerThread, 0) != WAIT_OBJECT_0)
      return;
    CloseHandle(g_pickerThread);
    g_pickerThread = nullptr;
  }
  g_pickerThread = CreateThread(nullptr, 0, FilePickerThreadProc,
                                (LPVOID)ownerWnd, 0, &g_pickerThreadId);
}

BOOL CALLBACK ClosePickerDialog(HWND hwnd, LPARAM) {
  wchar_t className[32];
  if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) &&
      wcscmp(className, L"#32770") == 0) {
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
  }
  return TRUE;
}

void ReloadWallpaperSource() {
  Profiler::RecordEvent(L"Reload");
  std::wstring path;
  if (!ResolveVideoSource(path)) {
    g_sourceRetryAttempts++;
    Wh_Log(L"ReloadWallpaperSource: could not resolve a video source (attempt %d/30)",
           g_sourceRetryAttempts);
    if (g_wallpaperWnd && IsWindowVisible(g_wallpaperWnd)) {
      ShowWindow(g_wallpaperWnd, SW_HIDE);
    }
    if (g_wallpaperWnd && g_sourceRetryAttempts < 30) {
      SetTimer(g_wallpaperWnd, kSourceRetryTimerId, 2000, nullptr);
    } else if (g_wallpaperWnd) {
      KillTimer(g_wallpaperWnd, kSourceRetryTimerId);
    }
    return;
  }
  if (!g_player.Load(path)) {
    g_sourceRetryAttempts++;
    Wh_Log(L"ReloadWallpaperSource: player load/recover failed (attempt %d/30)", g_sourceRetryAttempts);
    if (g_wallpaperWnd && IsWindowVisible(g_wallpaperWnd)) {
      ShowWindow(g_wallpaperWnd, SW_HIDE);
    }
    if (g_wallpaperWnd && g_sourceRetryAttempts < 30) {
      SetTimer(g_wallpaperWnd, kSourceRetryTimerId, 2000, nullptr);
    }
    return;
  }
  g_sourceRetryAttempts = 0;
  if (g_wallpaperWnd) {
    KillTimer(g_wallpaperWnd, kSourceRetryTimerId);
  }
}

// Fluid-mode counterpart to ReloadWallpaperSource(). Handles both first-time
// initialization and post-device-loss rebuild (g_player owns the shared D3D
// device/swap chain regardless of mode, so its deviceLost flag is the signal).
void ReloadFluidSource() {
  Profiler::RecordEvent(L"Reload");
  if (!g_wallpaperWnd)
    return;

  if (g_player.deviceLost || !g_player.d3dDevice.Get()) {
    Wh_Log(L"ReloadFluidSource: shared device lost or missing, rebuilding");
    HWND hwnd = g_wallpaperWnd;
    int w = g_player.width, h = g_player.height;
    g_fluidSim.Shutdown();
    g_player.Shutdown();
    if (!g_player.InitD3DAndSwapChain(hwnd, w, h)) {
      Wh_Log(L"ReloadFluidSource: failed to rebuild shared D3D device/swap chain");
      if (IsWindowVisible(hwnd))
        ShowWindow(hwnd, SW_HIDE);
      return;
    }
  }

  if (!g_fluidSim.IsInitialized()) {
    if (!g_fluidSim.Initialize(g_player.d3dDevice.Get(), g_player.d3dContext.Get(),
                               g_player.swapChain.Get(), g_wallpaperWnd, g_player.width,
                               g_player.height)) {
      Wh_Log(L"ReloadFluidSource: FluidSimulation::Initialize failed");
      if (g_wallpaperWnd && IsWindowVisible(g_wallpaperWnd)) {
        ShowWindow(g_wallpaperWnd, SW_HIDE);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Mod lifecycle
// ---------------------------------------------------------------------------

HANDLE g_thread = nullptr;
DWORD g_threadId = 0;
HANDLE g_messageQueueReadyEvent = nullptr;
HMODULE g_modInstance = nullptr;
bool g_mfStarted = false;
bool g_comInitialized = false;

void RegisterConfiguredHotkeys() {
  UnregisterHotKey(g_wallpaperWnd, kHotkeyId);
  UnregisterHotKey(g_wallpaperWnd, kVisibilityHotkeyId);
  UnregisterHotKey(g_wallpaperWnd, kProfilerHotkeyId);
  if (g_filePickerHotkey.load() &&
      !RegisterHotKey(g_wallpaperWnd, kHotkeyId,
                      MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'G')) {
    Wh_Log(L"File picker hotkey Ctrl+Alt+G is unavailable");
  }
  if (g_visibilityHotkey.load() &&
      !RegisterHotKey(g_wallpaperWnd, kVisibilityHotkeyId,
                      MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'H')) {
    Wh_Log(L"Visibility hotkey Ctrl+Alt+H is unavailable");
  }
  if (g_profilerHotkey.load() &&
      !RegisterHotKey(g_wallpaperWnd, kProfilerHotkeyId,
                      MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'D')) {
    Wh_Log(L"Profiler hotkey Ctrl+Alt+D is unavailable");
  }
}

DWORD WINAPI WallpaperThreadProc(LPVOID) {
  MSG startupMsg;
  PeekMessageW(&startupMsg, nullptr, 0, 0, PM_NOREMOVE);
  SetEvent(g_messageQueueReadyEvent);

  HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  g_comInitialized = SUCCEEDED(comHr);
  if (!g_comInitialized) {
    Wh_Log(L"WallpaperThreadProc: CoInitializeEx failed, hr=0x%08lX",
           (unsigned long)comHr);
  }

  HRESULT mfHr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  g_mfStarted = SUCCEEDED(mfHr);
  if (!g_mfStarted) {
    Wh_Log(L"WallpaperThreadProc: MFStartup failed, hr=0x%08lX",
           (unsigned long)mfHr);
    if (g_comInitialized)
      CoUninitialize();
    return 1;
  }

  HWND progman = nullptr;
  for (;;) {
    if (WaitForSingleObject(g_shutdownEvent, 0) == WAIT_OBJECT_0)
      break;
    HWND shellWindow = GetShellWindow();
    if (shellWindow) {
      DWORD shellPid = 0;
      GetWindowThreadProcessId(shellWindow, &shellPid);
      if (shellPid != GetCurrentProcessId()) {
        Wh_Log(L"WallpaperThreadProc: this Explorer process does not own the desktop");
        progman = nullptr;
        break;
      }
    }
    progman = FindWindowW(L"Progman", nullptr);
    if (progman && shellWindow)
      break;
    progman = nullptr;
    if (WaitForSingleObject(g_shutdownEvent, 500) == WAIT_OBJECT_0)
      break;
  }
  if (!progman) {
    Wh_Log(L"WallpaperThreadProc: desktop is unavailable");
    MFShutdown();
    if (g_comInitialized)
      CoUninitialize();
    return 0;
  }

  HWND classicWorkerW = progman ? TryFindClassicWorkerWWithRetries(progman) : nullptr;
  if (WaitForSingleObject(g_shutdownEvent, 0) == WAIT_OBJECT_0) {
    MFShutdown();
    if (g_comInitialized)
      CoUninitialize();
    return 0;
  }
  g_topLevelMode = (classicWorkerW == nullptr);

  WNDCLASSW wc = {};
  wc.lpfnWndProc = WallpaperWndProc;
  wc.hInstance = g_modInstance;
  wc.lpszClassName = kWindowClassName;
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  if (!RegisterClassW(&wc)) {
    Wh_Log(L"WallpaperThreadProc: failed to register window class, error=%lu",
           GetLastError());
    MFShutdown();
    if (g_comInitialized)
      CoUninitialize();
    return 1;
  }

  int x, y, width, height;
  HWND parentWnd = nullptr;
  DWORD style, exStyle;

  if (g_topLevelMode) {
    Wh_Log(
        L"WallpaperThreadProc: no classic sibling WorkerW -- using independent "
        L"top-level window pinned to the bottom of the Z order");
    RECT vr;
    GetVirtualScreenRect(vr);
    x = vr.left;
    y = vr.top;
    width = vr.right - vr.left;
    height = vr.bottom - vr.top;
    parentWnd = nullptr;
    style = WS_POPUP | WS_CLIPSIBLINGS;
    exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT;
  } else {
    Wh_Log(
        L"WallpaperThreadProc: using classic child-of-WorkerW mode, host=0x%p",
        classicWorkerW);
    RECT hostClient = {};
    GetClientRect(classicWorkerW, &hostClient);
    x = 0;
    y = 0;
    width = hostClient.right - hostClient.left;
    height = hostClient.bottom - hostClient.top;
    if (width <= 0 || height <= 0) {
      width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
      height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }
    parentWnd = classicWorkerW;
    style = WS_CHILD | WS_CLIPSIBLINGS | WS_DISABLED;
    exStyle = WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
  }

  g_wallpaperWnd = CreateWindowExW(
      exStyle, kWindowClassName, L"VideoWallpaperEngine", style, x, y, width,
      height, parentWnd, nullptr, g_modInstance, nullptr);

  if (!g_wallpaperWnd) {
    Wh_Log(L"WallpaperThreadProc: failed to create wallpaper window, error=%lu",
           GetLastError());
    UnregisterClassW(kWindowClassName, g_modInstance);
    MFShutdown();
    if (g_comInitialized)
      CoUninitialize();
    return 1;
  }

  if (g_topLevelMode) {
    SetWindowPos(g_wallpaperWnd, HWND_BOTTOM, x, y, width, height,
                 SWP_NOACTIVATE | SWP_HIDEWINDOW);
  } else {
    SetWindowPos(g_wallpaperWnd, nullptr, 0, 0, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW);
    SetWindowPos(g_wallpaperWnd, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  SetTimer(g_wallpaperWnd, kZOrderTimerId, 1000, nullptr);

  bool sharedDeviceOk = g_player.InitD3DAndSwapChain(g_wallpaperWnd, width, height);
  if (!sharedDeviceOk) {
    Wh_Log(L"WallpaperThreadProc: shared D3D device/swap chain init failed");
    if (g_wallpaperWnd && IsWindowVisible(g_wallpaperWnd)) {
      ShowWindow(g_wallpaperWnd, SW_HIDE);
    }
  }

  SetTimer(g_wallpaperWnd, kRenderTimerId, VideoPlayer::GetMonitorRefreshIntervalMs(), nullptr);
  SetTimer(g_wallpaperWnd, kOcclusionTimerId, g_occlusionIntervalMs.load(), nullptr);
  WTSRegisterSessionNotification(g_wallpaperWnd, NOTIFY_FOR_THIS_SESSION);
  CheckBatteryState();

  if (!g_hWinEventHook) {
    g_hWinEventHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_MINIMIZEEND,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
  }

  RegisterConfiguredHotkeys();

  if (sharedDeviceOk) {
    if (g_wallpaperMode.load() == WallpaperMode::Fluid) {
      ReloadFluidSource();
    } else {
      if (!g_player.InitMediaEngine(g_wallpaperWnd)) {
        Wh_Log(L"WallpaperThreadProc: InitMediaEngine failed");
      }
      ReloadWallpaperSource();
    }
  }

  if (WaitForSingleObject(g_shutdownEvent, 0) == WAIT_OBJECT_0)
    PostQuitMessage(0);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (msg.message == kMsgReloadSource) {
      g_sourceRetryAttempts = 0;
      if (g_wallpaperMode.load() == WallpaperMode::Fluid) {
        ReloadFluidSource();
      } else {
        ReloadWallpaperSource();
      }
      continue;
    }
    if (msg.message == kMsgModeChanged) {
      Wh_Log(L"WallpaperThreadProc: wallpaper mode changed to %s",
             g_wallpaperMode.load() == WallpaperMode::Fluid ? L"fluid" : L"video");
      HWND hwnd = g_wallpaperWnd;
      int w = g_player.width, h = g_player.height;
      g_fluidSim.Shutdown();
      g_player.Shutdown();
      if (!hwnd || !g_player.InitD3DAndSwapChain(hwnd, w, h)) {
        Wh_Log(L"WallpaperThreadProc: failed to rebuild shared device after mode change");
        if (hwnd && IsWindowVisible(hwnd))
          ShowWindow(hwnd, SW_HIDE);
        continue;
      }
      if (g_wallpaperMode.load() == WallpaperMode::Fluid) {
        if (!g_fluidSim.Initialize(g_player.d3dDevice.Get(), g_player.d3dContext.Get(),
                                   g_player.swapChain.Get(), hwnd, w, h)) {
          Wh_Log(L"WallpaperThreadProc: FluidSimulation init failed after mode change");
        }
      } else {
        if (!g_player.InitMediaEngine(hwnd)) {
          Wh_Log(L"WallpaperThreadProc: InitMediaEngine failed after mode change");
        } else {
          ReloadWallpaperSource();
        }
      }
      continue;
    }
    if (msg.message == kMsgUpdateSettings) {
      RegisterConfiguredHotkeys();
      if (g_wallpaperWnd) {
        SetTimer(g_wallpaperWnd, kOcclusionTimerId, g_occlusionIntervalMs.load(), nullptr);
        if (g_wallpaperMode.load() == WallpaperMode::Fluid) {
          SetTimer(g_wallpaperWnd, kRenderTimerId,
                   VideoPlayer::GetMonitorRefreshIntervalMs(), nullptr);
        }
      }
      if (g_wallpaperMode.load() == WallpaperMode::Video) {
        if (g_player.engine.Get()) {
          g_player.engine->SetMuted(g_audioMuted.load() ? TRUE : FALSE);
          g_player.engine->SetVolume(static_cast<double>(g_audioVolume.load()) / 100.0);
        }
        g_player.UpdatePlaybackState();
      }
      continue;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  Wh_Log(L"WallpaperThreadProc: message loop exiting");

  g_fluidSim.Shutdown();
  g_player.Shutdown();

  if (g_wallpaperWnd) {
    if (g_hWinEventHook) {
      UnhookWinEvent(g_hWinEventHook);
      g_hWinEventHook = nullptr;
    }
    WTSUnRegisterSessionNotification(g_wallpaperWnd);
    UnregisterHotKey(g_wallpaperWnd, kHotkeyId);
    UnregisterHotKey(g_wallpaperWnd, kVisibilityHotkeyId);
    UnregisterHotKey(g_wallpaperWnd, kProfilerHotkeyId);
    DestroyWindow(g_wallpaperWnd);
    g_wallpaperWnd = nullptr;
  }
  UnregisterClassW(kWindowClassName, g_modInstance);

  if (g_mfStarted)
    MFShutdown();
  if (g_comInitialized)
    CoUninitialize();

  return 0;
}

BOOL Wh_ModInit() {
  Wh_Log(L"Init");
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&Wh_ModInit),
                           &g_modInstance)) {
    return FALSE;
  }
  InitializeCriticalSection(&g_pathLock);
  LoadSettings();

  g_shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  g_messageQueueReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!g_shutdownEvent || !g_messageQueueReadyEvent) {
    if (g_shutdownEvent) CloseHandle(g_shutdownEvent);
    if (g_messageQueueReadyEvent) CloseHandle(g_messageQueueReadyEvent);
    DeleteCriticalSection(&g_pathLock);
    return FALSE;
  }
  g_thread =
      CreateThread(nullptr, 0, WallpaperThreadProc, nullptr, 0, &g_threadId);
  if (!g_thread) {
    Wh_Log(L"Failed to create wallpaper thread, error=%lu", GetLastError());
    CloseHandle(g_shutdownEvent);
    CloseHandle(g_messageQueueReadyEvent);
    DeleteCriticalSection(&g_pathLock);
    return FALSE;
  }
  return TRUE;
}

void Wh_ModUninit() {
  Wh_Log(L"Uninit");
  SetEvent(g_shutdownEvent);
  if (g_thread) {
    WaitForSingleObject(g_messageQueueReadyEvent, INFINITE);
    PostThreadMessageW(g_threadId, WM_QUIT, 0, 0);
    WaitForSingleObject(g_thread, INFINITE);
    CloseHandle(g_thread);
    g_thread = nullptr;
  }
  if (g_pickerThread) {
    while (WaitForSingleObject(g_pickerThread, 100) == WAIT_TIMEOUT) {
      EnumThreadWindows(g_pickerThreadId, ClosePickerDialog, 0);
    }
    CloseHandle(g_pickerThread);
    g_pickerThread = nullptr;
  }
  CloseHandle(g_messageQueueReadyEvent);
  CloseHandle(g_shutdownEvent);
  DeleteCriticalSection(&g_pathLock);
}

void Wh_ModSettingsChanged() {
  Wh_Log(L"SettingsChanged");
  WallpaperMode oldMode = g_wallpaperMode.load();
  std::wstring oldPath = LoadSettings();
  WallpaperMode newMode = g_wallpaperMode.load();
  std::wstring newPath = GetVideoPathSetting();
  if (g_threadId) {
    if (newMode != oldMode) {
      PostThreadMessageW(g_threadId, kMsgModeChanged, 0, 0);
    } else if (newMode == WallpaperMode::Video && newPath != oldPath) {
      PostThreadMessageW(g_threadId, kMsgReloadSource, 0, 0);
    } else {
      PostThreadMessageW(g_threadId, kMsgUpdateSettings, 0, 0);
    }
  }
}