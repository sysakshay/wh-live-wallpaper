// ============================================================================
// PerformanceProfiler.cpp
// Professional-Grade Runtime Profiling & Diagnostics System for Windhawk Mods
// ============================================================================
//
// Implements high-precision QPC timing, circular ring buffer statistics,
// psapi process memory queries, DXGI VRAM tracking, bottleneck detection,
// periodic 5-second logging, and live on-screen overlay rendering.
// ============================================================================

#include "PerformanceProfiler.h"
#include <psapi.h>
#include <stdio.h>

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
