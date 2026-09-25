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

#pragma once

#include <algorithm>
#include <atomic>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_4.h>
#include <string>
#include <windows.h>

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
