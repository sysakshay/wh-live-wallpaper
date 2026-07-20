// clang-format off
// ==WindhawkMod==
// @id              wh-live-wallpaper
// @name            Live Video Wallpaper
// @description     Play MP4 videos as your Windows desktop wallpaper using hardware-accelerated Media Foundation rendering.
// @version         1.0.0
// @author          AKS HAY
// @include         explorer.exe
// @compilerOptions -lgdi32 -lshlwapi -lcomdlg32 -ldwmapi -lmfplat -lmfuuid -luuid -ld3d11 -ldxgi -lole32 -loleaut32
// ==/WindhawkMod==
// clang-format on

// ==WindhawkModReadme==
/*
# Live Video Wallpaper

Play local MP4 videos directly on your Windows desktop as a live wallpaper using Media Foundation and Direct3D 11.

Unlike traditional animated wallpaper implementations, this mod renders video directly through the GPU, minimizing CPU usage while maintaining smooth playback.

## Features

- Hardware-accelerated MP4 playback
- Direct3D 11 rendering
- Media Foundation video decoding
- Automatic pause when the desktop is fully covered
- Battery-aware power saving modes
- Fill and Fit scaling modes
- Hotkey to choose videos
- Automatic device-loss recovery
- Automatic display resize handling
- Local video playback (no downloads or network activity)

## Controls

**Ctrl + Alt + G**

Open the file picker and choose a new MP4.

**Ctrl + Alt + H**

Toggle wallpaper visibility.

## Settings

### Video Path

Select any local MP4 file.

### Fit Mode

- Fill
- Fit

### Battery Mode

- Pause playback
- Reduce playback to 15 FPS
- Ignore battery status

## Notes

Only local MP4 files are supported.

Videos are decoded using Windows Media Foundation and rendered through Direct3D 11 directly onto the desktop wallpaper window.

Changing scaling or battery settings applies immediately without restarting playback.

Changing the selected video reloads only the media source while preserving the rendering pipeline.
*/
// ==/WindhawkModReadme==

// clang-format off
// ==WindhawkModSettings==
/*
- videoPath: ""
  $name: Local video path
  $description: Full path to an .mp4 file on disk. Tip -- press Ctrl+Alt+G in the desktop to pick a file interactively instead of typing a path here.
- fitMode: fill
  $name: Fit mode
  $description: How to scale the video to fill the screen
  $options:
  - fill: Fill (stretch, may distort)
  - fit: Fit (letterbox, preserve aspect)
- batteryMode: pause
  $name: Battery saving mode
  $description: What to do when running on laptop battery power
  $options:
  - pause: Pause video (maximum battery life)
  - drop: Drop to 15 FPS (reduced power consumption)
  - normal: Play normally (ignore battery status)
*/
// ==/WindhawkModSettings==
// clang-format on

#include <atomic>
#include <commdlg.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfmediaengine.h>
#include <shlwapi.h>
#include <string>
#include <windows.h>

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

#ifndef DWMWA_EXTENDED_FRAME_BOUNDS
#define DWMWA_EXTENDED_FRAME_BOUNDS 9
#endif
#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif

// ---------------------------------------------------------------------------
// Minimal COM smart pointer (kept dependency-free rather than relying on
// <wrl/client.h>, which isn't reliably available in this MinGW toolchain).
// ---------------------------------------------------------------------------

template <typename T> struct ComPtr {
  T *ptr = nullptr;
  ComPtr() = default;
  ComPtr(const ComPtr &other) {
    ptr = other.ptr;
    if (ptr)
      ptr->AddRef();
  }
  ComPtr(ComPtr &&other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
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
  T **operator&() { return &ptr; }
  T *operator->() const { return ptr; }
  T *Get() const { return ptr; }
  operator T *() const { return ptr; }
};

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
std::atomic<bool> g_fitStretch{true}; // true = fill/stretch, false = fit
std::atomic<BatteryMode> g_batteryMode{BatteryMode::Pause};

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
std::wstring LoadSettings() {
  PCWSTR videoPath = Wh_GetStringSetting(L"videoPath");
  std::wstring newPath = videoPath;
  Wh_FreeStringSetting(videoPath);

  PCWSTR fitMode = Wh_GetStringSetting(L"fitMode");
  g_fitStretch = (wcscmp(fitMode, L"fill") == 0);
  Wh_FreeStringSetting(fitMode);

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

  EnterCriticalSection(&g_pathLock);
  std::wstring oldPath = g_videoPath;
  g_videoPath = newPath;
  LeaveCriticalSection(&g_pathLock);
  return oldPath;
}

// ---------------------------------------------------------------------------
// Window / message constants
// ---------------------------------------------------------------------------

HWND g_wallpaperWnd = nullptr;
const UINT_PTR kRenderTimerId = 1;
const UINT_PTR kZOrderTimerId = 2;
const UINT_PTR kOcclusionTimerId = 3;
const int kHotkeyId = 1;
const int kVisibilityHotkeyId = 2;
const wchar_t kWindowClassName[] = L"VideoWallpaperEngine_WorkerWindow";
const UINT kMsgReloadSource = WM_APP + 1;
const UINT kMsgMediaEngineEvent = WM_APP + 2;

bool g_topLevelMode = false;
bool g_wallpaperHidden = false;
bool g_isOnBattery = false;

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
    PostMessageW(wnd, kMsgMediaEngineEvent, (WPARAM)event, (LPARAM)param1);
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

  bool loaded = false;
  bool canPlay = false;
  bool wantsPlay =
      false; // true once caller asked to play, even if CANPLAY hasn't fired yet
  bool pausedForFullscreen = false;
  bool pausedForBattery = false;
  DWORD loadStartTickMs = 0;
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

    if (!CreateRenderTarget(w, h))
      return false;

    return true;
  }

  // (Re)creates the offscreen texture TransferVideoFrame writes into.
  // Must be called any time width/height changes (initial creation and
  // Resize()).
  bool CreateRenderTarget(int w, int h) {
    renderTarget.Reset();

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;

    HRESULT hr = d3dDevice->CreateTexture2D(&desc, nullptr, &renderTarget);
    if (FAILED(hr)) {
      Wh_Log(L"VideoPlayer::CreateRenderTarget: CreateTexture2D failed, "
             L"hr=0x%08lX",
             (unsigned long)hr);
      return false;
    }
    return true;
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
    engine->SetMuted(TRUE);

    return true;
  }

  // file:// URL builder. UrlCreateFromPathW (shlwapi, already linked) is
  // the real RFC3986-aware Win32 API for this -- it correctly escapes
  // '#', '%', '&', '+', non-ASCII characters, etc, which a hand-rolled
  // escaper handling only backslashes and spaces would mangle.
  static std::wstring BuildFileUrl(const std::wstring &path) {
    wchar_t urlBuf[2084] = {};
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
    if (deviceLost) {
      if (!Recover())
        return false;
    }
    if (!engine.Get())
      return false;

    loaded = false;
    canPlay = false;

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
    if (canPlay || loadStartTickMs == 0 || deviceLost)
      return;
    if (GetTickCount() - loadStartTickMs <= 8000)
      return;

    if (loadTimeoutRetryCount >= kMaxLoadTimeoutRetries) {
      if (!loadTimeoutLogged) {
        loadTimeoutLogged = true;
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
    Wh_Log(L"VideoPlayer: video did not become playable within 8s of Load() "
           L"(attempt %d/%d) -- reloading",
           loadTimeoutRetryCount, kMaxLoadTimeoutRetries);
    if (boundHwnd) {
      PostMessageW(boundHwnd, kMsgReloadSource, 0, 0);
    }
  }

  // Called when MF_MEDIA_ENGINE_EVENT_CANPLAY (or similar readiness event)
  // arrives via kMsgMediaEngineEvent, back on the wallpaper thread.
  bool IsEffectivePaused() const {
    return pausedForFullscreen || pausedForBattery;
  }

  void UpdatePlaybackState() {
    if (!engine.Get() || !canPlay) {
      if (boundHwnd)
        KillTimer(boundHwnd, kRenderTimerId);
      return;
    }
    if (IsEffectivePaused()) {
      engine->Pause();
      if (boundHwnd)
        KillTimer(boundHwnd, kRenderTimerId);
    } else if (wantsPlay) {
      engine->Play();
      if (boundHwnd) {
        UINT interval =
            (g_isOnBattery && g_batteryMode.load() == BatteryMode::DropFps)
                ? 66
                : 16;
        SetTimer(boundHwnd, kRenderTimerId, interval, nullptr);
      }
    }
  }

  void OnCanPlay() {
    canPlay = true;
    Wh_Log(L"VideoPlayer::OnCanPlay: canPlay=true, wantsPlay=%d, "
           L"pausedForFullscreen=%d, pausedForBattery=%d",
           wantsPlay ? 1 : 0, pausedForFullscreen ? 1 : 0,
           pausedForBattery ? 1 : 0);
    if (wantsPlay) {
      HRESULT hr = engine->Play();
      Wh_Log(L"VideoPlayer::OnCanPlay: Play() hr=0x%08lX", (unsigned long)hr);
      if (IsEffectivePaused()) {
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
    swapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
    CreateRenderTarget(w, h);
  }

  void ComputeDestRect(RECT &dest, bool fitStretch) {
    if (fitStretch || !engine.Get()) {
      dest = {0, 0, width, height};
      return;
    }
    if (destRectValid && cachedDestRect.right > 0) {
      dest = cachedDestRect;
      return;
    }

    DWORD videoW = 0, videoH = 0;
    if (FAILED(engine->GetNativeVideoSize(&videoW, &videoH)) || videoW == 0 ||
        videoH == 0) {
      dest = {0, 0, width, height};
      return;
    }
    cachedVideoW = videoW;
    cachedVideoH = videoH;
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
    if (!boundHwnd)
      return false;
    Wh_Log(L"VideoPlayer::Recover: rebuilding D3D device/swap chain/media "
           L"engine after device loss");

    HWND hwnd = boundHwnd;
    int w = width, h = height;
    Shutdown();

    if (!InitD3DAndSwapChain(hwnd, w, h) || !InitMediaEngine(hwnd)) {
      Wh_Log(L"VideoPlayer::Recover: rebuild failed, wallpaper will stay "
             L"unavailable "
             L"until a full mod reload");
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

  bool Tick(bool fitStretch, bool isOnBattery) {
    tickCallCount++;

    if (deviceLost || !engine.Get() || !canPlay || IsEffectivePaused() ||
        !swapChain.Get() || !renderTarget.Get()) {
// Throttled diagnostic: if we're perpetually blocked on one of
// these gate conditions (most likely canPlay never becoming
// true), say so periodically instead of being silent forever.
#ifdef DEBUG
      DWORD now = GetTickCount();
      if (now - lastTickLogMs > 3000) {
        lastTickLogMs = now;
        Wh_Log(L"VideoPlayer::Tick: gated -- deviceLost=%d hasEngine=%d "
               L"canPlay=%d IsEffectivePaused=%d hasSwapChain=%d "
               L"hasRenderTarget=%d",
               deviceLost ? 1 : 0, engine.Get() != nullptr, canPlay ? 1 : 0,
               IsEffectivePaused() ? 1 : 0, swapChain.Get() != nullptr,
               renderTarget.Get() != nullptr);
      }
#endif
      return false;
    }

    if (g_batteryMode.load() == BatteryMode::DropFps && isOnBattery) {
      static DWORD lastDropFpsTick = 0;
      DWORD now = GetTickCount();
      if (now - lastDropFpsTick < 66 && lastDropFpsTick != 0) {
        return false;
      }
      lastDropFpsTick = now;
    }

    LONGLONG pts = 0;
    HRESULT hr = engine->OnVideoStreamTick(&pts);
    DWORD now = GetTickCount();
    static DWORD lastSuccessTickMs = 0;
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
      // If stalled for more than 4 seconds while active and unpaused, recover
      // from sleep/lock stall:
      if (lastSuccessTickMs != 0 && (now - lastSuccessTickMs > 4000)) {
        Wh_Log(L"VideoPlayer::Tick: stalled after sleep/lock (>4s without "
               L"ready frame), triggering reload");
        lastSuccessTickMs = now;
        PostMessageW(boundHwnd, kMsgReloadSource, 0, 0);
      }
      return false;
    }
    lastSuccessTickMs = now;

    tickSuccessCount++;

    RECT dest;
    ComputeDestRect(dest, fitStretch);

    // Write into the offscreen render target, NOT the swap chain's own
    // back buffer -- see the comment on the renderTarget member.
    //
    // pBorderClr must NOT be NULL -- despite some sample code passing
    // nullptr here, IMFMediaEngine::TransferVideoFrame validates this
    // parameter and returns E_INVALIDARG (0x80070057) if it's null.
    // This is almost certainly why every transfer below was failing
    // and the wallpaper only ever showed a single static frame.
    static const MFARGB kBorderColor = {0, 0, 0, 0xFF};
    hr = engine->TransferVideoFrame(renderTarget.Get(), nullptr, &dest,
                                    &kBorderColor);
    if (FAILED(hr)) {
#ifdef DEBUG
      DWORD now = GetTickCount();
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

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                              (void **)&backBuffer);
    if (FAILED(hr)) {
      if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        OnDeviceLost();
      }
      return false;
    }

    d3dContext->CopyResource(backBuffer.Get(), renderTarget.Get());

    hr = swapChain->Present(1, 0);
    if (FAILED(hr) &&
        (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)) {
      OnDeviceLost();
      return false;
    }
    return true;
  }

  void Shutdown() {
    if (engine.Get()) {
      engine->Shutdown();
    }
    engine.Reset();
    // notify's lifetime is ref-counted by the engine's AddRef in
    // SetUnknown; once engine is released, notify's refcount drops to
    // zero via its own Release() and it deletes itself. Do not delete
    // it manually here.
    notify = nullptr;
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
    deviceLost = false;
    boundHwnd = nullptr;
  }
};

VideoPlayer g_player;

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

HWND FindClassicTopLevelWorkerW(HWND progman) {
  HWND workerW = nullptr;
  EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&workerW));
  if (workerW) {
    Wh_Log(L"FindClassicTopLevelWorkerW: found top-level WorkerW sibling 0x%p",
           workerW);
    return workerW;
  }
  return nullptr;
}

HWND FindChildWorkerW(HWND progman) {
  HWND workerW = nullptr;
  while ((workerW = FindWindowExW(progman, workerW, L"WorkerW", nullptr)) !=
         nullptr) {
    HWND defView =
        FindWindowExW(workerW, nullptr, L"SHELLDLL_DefView", nullptr);
    if (!defView) {
      Wh_Log(
          L"FindChildWorkerW: found child WorkerW 0x%p (no SHELLDLL_DefView)",
          workerW);
      return workerW;
    }
  }
  Wh_Log(L"FindChildWorkerW: no usable child WorkerW found under Progman");
  return nullptr;
}

HWND TryFindClassicWorkerWWithRetries(HWND progman) {
  DWORD_PTR result = 0;
  SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);

  for (int attempt = 0; attempt < 10; attempt++) {
    HWND workerW = FindClassicTopLevelWorkerW(progman);
    if (workerW)
      return workerW;
    HWND childWorkerW = FindChildWorkerW(progman);
    if (childWorkerW)
      return childWorkerW;
    Sleep(200);
    SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);
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
  if (!g_topLevelMode || !g_wallpaperWnd || g_wallpaperHidden)
    return;
  SetWindowPos(g_wallpaperWnd, HWND_BOTTOM, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// ---------------------------------------------------------------------------
// Fullscreen-pause heuristic (unchanged)
// ---------------------------------------------------------------------------

bool IsDesktopFullyCovered() {
  static DWORD lastCheckTick = 0;
  static bool cachedResult = false;
  DWORD now = GetTickCount();
  if (now - lastCheckTick < 250 && lastCheckTick != 0) {
    return cachedResult;
  }
  lastCheckTick = now;

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

  HRGN hrgnUncovered = nullptr;
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

    // 1. Skip click-through / transparent hit-test windows (e.g. Discord overlay,
    // RTSS overlay, recording indicator borders, crosshairs).
    if (exStyle & WS_EX_TRANSPARENT) {
      hwnd = GetWindow(hwnd, GW_HWNDNEXT);
      continue;
    }

    // 2. Skip tool windows unless they explicitly opt into being taskbar app windows
    // (e.g. NVIDIA GeForce recording overlay, floating status popups, hidden background helpers).
    if ((exStyle & WS_EX_TOOLWINDOW) && !(exStyle & WS_EX_APPWINDOW)) {
      hwnd = GetWindow(hwnd, GW_HWNDNEXT);
      continue;
    }

    // 3. Skip non-activating overlay/indicator windows (e.g. Game Bar background capture,
    // performance monitors, widgets that never take keyboard focus).
    if ((exStyle & WS_EX_NOACTIVATE) && !(exStyle & WS_EX_APPWINDOW)) {
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
        // If nearly invisible alpha OR chroma-keyed (transparent background cutouts), skip
        if (((flags & LWA_ALPHA) && alpha < 15) || (flags & LWA_COLORKEY)) {
          hwnd = GetWindow(hwnd, GW_HWNDNEXT);
          continue;
        }
      } else {
        // GetLayeredWindowAttributes returns FALSE when a window uses UpdateLayeredWindow
        // (DirectComposition/DirectX per-pixel alpha rendering, commonly used by modern
        // GPU-rendered overlays). If such a window is borderless (no standard caption/frame),
        // treat it as a non-occluding composition overlay.
        if (!(style & WS_CAPTION) && !(style & WS_THICKFRAME)) {
          hwnd = GetWindow(hwnd, GW_HWNDNEXT);
          continue;
        }
      }
    }

    // Get true visible bounds (excluding invisible DWM drop shadows around
    // normal windows)
    RECT wr = {};
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
      if (hrgnUncovered) {
        DeleteObject(hrgnUncovered);
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
      if (!hrgnUncovered) {
        hrgnUncovered = CreateRectRgnIndirect(&rcTarget);
      }
      HRGN hrgnWnd = CreateRectRgnIndirect(&wr);
      if (hrgnWnd) {
        CombineRgn(hrgnUncovered, hrgnUncovered, hrgnWnd, RGN_DIFF);
        DeleteObject(hrgnWnd);
      }

      RECT rcBox = {};
      int rgnType = GetRgnBox(hrgnUncovered, &rcBox);
      if (rgnType == NULLREGION || IsRectEmpty(&rcBox) ||
          (rcBox.right - rcBox.left <= 4 || rcBox.bottom - rcBox.top <= 4)) {
        DeleteObject(hrgnUncovered);
        cachedResult = true;
        return true;
      }
    }

    hwnd = GetWindow(hwnd, GW_HWNDNEXT);
  }

  if (hrgnUncovered) {
    DeleteObject(hrgnUncovered);
  }
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

  SYSTEM_POWER_STATUS status = {};
  if (GetSystemPowerStatus(&status)) {
    g_isOnBattery = (status.ACLineStatus == 0);
  } else {
    g_isOnBattery = false;
  }

  if (g_batteryMode.load() == BatteryMode::Pause) {
    g_player.SetPausedForBattery(g_isOnBattery);
  } else {
    g_player.SetPausedForBattery(false);
  }
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

bool PickAndLoadVideoViaDialog(HWND ownerWnd);

LRESULT CALLBACK WallpaperWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam) {
  switch (msg) {
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
      g_player.Tick(g_fitStretch.load(), g_isOnBattery);
    } else if (wParam == kOcclusionTimerId) {
      g_player.CheckLoadTimeout();
      CheckBatteryState();
      bool covered = IsDesktopFullyCovered();
      g_player.SetPausedForFullscreen(covered);
    } else if (wParam == kZOrderTimerId) {
      PinBehindTargetWindow();
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
      PickAndLoadVideoViaDialog(hwnd);
    } else if (wParam == kVisibilityHotkeyId) {
      g_wallpaperHidden = !g_wallpaperHidden;
      ShowWindow(hwnd, g_wallpaperHidden ? SW_HIDE : SW_SHOWNOACTIVATE);
      if (!g_wallpaperHidden) {
        PinBehindTargetWindow();
      }
      Wh_Log(L"Visibility toggled, hidden=%d", g_wallpaperHidden ? 1 : 0);
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
        }
      }
    }

    g_player.OnSystemResume();
    return 0;
  }
  case WM_DESTROY:
    KillTimer(hwnd, kRenderTimerId);
    KillTimer(hwnd, kZOrderTimerId);
    KillTimer(hwnd, kOcclusionTimerId);
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

bool PickAndLoadVideoViaDialog(HWND ownerWnd) {
  HWND tempOwner =
      CreateWindowExW(WS_EX_TOPMOST, L"STATIC", L"", WS_POPUP, 0, 0, 0, 0,
                      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  if (tempOwner) {
    SetWindowPos(tempOwner, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(tempOwner);
  }

  wchar_t fileBuffer[MAX_PATH] = {};
  OPENFILENAMEW ofn = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = tempOwner ? tempOwner : ownerWnd;
  ofn.lpstrFilter = L"Video files (*.mp4)\0*.mp4\0All files (*.*)\0*.*\0";
  ofn.lpstrFile = fileBuffer;
  ofn.nMaxFile = ARRAYSIZE(fileBuffer);
  ofn.lpstrTitle = L"Pick a video for your wallpaper";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

  BOOL picked = GetOpenFileNameW(&ofn);
  if (tempOwner)
    DestroyWindow(tempOwner);
  if (!picked)
    return false;

  Wh_Log(L"PickAndLoadVideoViaDialog: picked %s", fileBuffer);
  return g_player.Load(fileBuffer);
}

void ReloadWallpaperSource() {
  std::wstring path;
  if (!ResolveVideoSource(path)) {
    Wh_Log(L"ReloadWallpaperSource: could not resolve a video source");
    return;
  }
  g_player.Load(path);
}

// ---------------------------------------------------------------------------
// Mod lifecycle
// ---------------------------------------------------------------------------

HANDLE g_thread = nullptr;
DWORD g_threadId = 0;
HANDLE g_threadReadyEvent = nullptr;
bool g_mfStarted = false;
bool g_comInitialized = false;

DWORD WINAPI WallpaperThreadProc(LPVOID) {
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
    SetEvent(g_threadReadyEvent);
    if (g_comInitialized)
      CoUninitialize();
    return 1;
  }

  HWND progman = nullptr;
  for (int attempt = 0; attempt < 20 && !progman; attempt++) {
    progman = FindWindowW(L"Progman", nullptr);
    if (!progman)
      Sleep(250);
  }
  if (!progman) {
    Wh_Log(L"WallpaperThreadProc: Progman not found after retrying for 5s");
    SetEvent(g_threadReadyEvent);
    MFShutdown();
    if (g_comInitialized)
      CoUninitialize();
    return 1;
  }

  HWND classicWorkerW = TryFindClassicWorkerWWithRetries(progman);
  g_topLevelMode = (classicWorkerW == nullptr);

  WNDCLASSW wc = {};
  wc.lpfnWndProc = WallpaperWndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = kWindowClassName;
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    Wh_Log(L"WallpaperThreadProc: failed to register window class, error=%lu",
           GetLastError());
    SetEvent(g_threadReadyEvent);
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
    exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
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
    style = WS_CHILD | WS_CLIPSIBLINGS;
    exStyle = 0; // deliberately no WS_EX_LAYERED -- see readme
  }

  g_wallpaperWnd = CreateWindowExW(
      exStyle, kWindowClassName, L"VideoWallpaperEngine", style, x, y, width,
      height, parentWnd, nullptr, GetModuleHandleW(nullptr), nullptr);

  if (!g_wallpaperWnd) {
    Wh_Log(L"WallpaperThreadProc: failed to create wallpaper window, error=%lu",
           GetLastError());
    SetEvent(g_threadReadyEvent);
    UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
    MFShutdown();
    if (g_comInitialized)
      CoUninitialize();
    return 1;
  }

  if (g_topLevelMode) {
    SetWindowPos(g_wallpaperWnd, HWND_BOTTOM, x, y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetTimer(g_wallpaperWnd, kZOrderTimerId, 1000, nullptr);
  } else {
    SetWindowPos(g_wallpaperWnd, nullptr, 0, 0, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowPos(g_wallpaperWnd, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  if (!g_player.InitD3DAndSwapChain(g_wallpaperWnd, width, height) ||
      !g_player.InitMediaEngine(g_wallpaperWnd)) {
    Wh_Log(L"WallpaperThreadProc: video player init failed -- wallpaper window "
           L"will stay black");
  }

  SetTimer(g_wallpaperWnd, kRenderTimerId, 16, nullptr);
  SetTimer(g_wallpaperWnd, kOcclusionTimerId, 250, nullptr);
  CheckBatteryState();

  bool hotkeyRegistered = false;
  struct {
    UINT mods;
    UINT vk;
    const wchar_t *label;
  } hotkeyCandidates[] = {
      {MOD_CONTROL | MOD_ALT, 'G', L"Ctrl+Alt+G"},
      {MOD_CONTROL | MOD_ALT | MOD_SHIFT, 'G', L"Ctrl+Alt+Shift+G"},
      {MOD_CONTROL | MOD_ALT, 'W', L"Ctrl+Alt+W"},
      {MOD_CONTROL | MOD_ALT | MOD_SHIFT, 'W', L"Ctrl+Alt+Shift+W"},
  };
  for (const auto &hk : hotkeyCandidates) {
    if (RegisterHotKey(g_wallpaperWnd, kHotkeyId, hk.mods, hk.vk)) {
      Wh_Log(L"WallpaperThreadProc: registered file-picker hotkey %s",
             hk.label);
      hotkeyRegistered = true;
      break;
    }
  }
  if (!hotkeyRegistered) {
    Wh_Log(L"WallpaperThreadProc: could not register ANY file-picker hotkey");
  }
  if (!RegisterHotKey(g_wallpaperWnd, kVisibilityHotkeyId,
                      MOD_CONTROL | MOD_ALT, 'H')) {
    Wh_Log(
        L"WallpaperThreadProc: visibility-toggle hotkey unavailable, error=%lu",
        GetLastError());
  }

  ReloadWallpaperSource();

  SetEvent(g_threadReadyEvent);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (msg.message == kMsgReloadSource) {
      ReloadWallpaperSource();
      continue;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  Wh_Log(L"WallpaperThreadProc: message loop exiting");

  g_player.Shutdown();

  if (g_wallpaperWnd) {
    UnregisterHotKey(g_wallpaperWnd, kHotkeyId);
    UnregisterHotKey(g_wallpaperWnd, kVisibilityHotkeyId);
    DestroyWindow(g_wallpaperWnd);
    g_wallpaperWnd = nullptr;
  }
  UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));

  if (g_mfStarted)
    MFShutdown();
  if (g_comInitialized)
    CoUninitialize();

  return 0;
}

BOOL Wh_ModInit() {
  Wh_Log(L"Init");
  InitializeCriticalSection(&g_pathLock);
  LoadSettings();

  g_threadReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  g_thread =
      CreateThread(nullptr, 0, WallpaperThreadProc, nullptr, 0, &g_threadId);
  if (!g_thread) {
    Wh_Log(L"Failed to create wallpaper thread, error=%lu", GetLastError());
    CloseHandle(g_threadReadyEvent);
    g_threadReadyEvent = nullptr;
    return FALSE;
  }

  WaitForSingleObject(g_threadReadyEvent, 10000);
  CloseHandle(g_threadReadyEvent);
  g_threadReadyEvent = nullptr;
  return TRUE;
}

void Wh_ModUninit() {
  Wh_Log(L"Uninit");
  if (g_thread) {
    PostThreadMessageW(g_threadId, WM_QUIT, 0, 0);
    DWORD waitResult = WaitForSingleObject(g_thread, 3000);
    if (waitResult == WAIT_TIMEOUT) {
      // The thread did not exit -- it's very likely blocked inside a
      // D3D/Media Foundation call in Tick() that never returned (see
      // the device-loss handling above; this is the scenario that
      // motivated it). We deliberately do NOT call TerminateThread()
      // here: forcibly killing a thread mid-call inside a shared
      // critical process like explorer.exe risks leaving D3D/COM/heap
      // state corrupted in ways that are worse than a stuck thread --
      // a stuck thread just sits there, a botched forced-terminate
      // can take the whole process down harder than the original
      // hang would have. Windows also cannot unload this DLL while
      // this thread is still alive inside it, so Windhawk's own
      // "Uninitializing..." will hang here too -- if you hit this,
      // the only clean recovery is restarting explorer.exe (Task
      // Manager -> Details -> explorer.exe -> Restart).
      Wh_Log(L"Wh_ModUninit: wallpaper thread did not exit within 3s -- it is "
             L"likely "
             L"stuck in a blocking D3D/MF call. This DLL cannot be safely "
             L"unloaded until "
             L"that thread exits; restart explorer.exe to recover.");
      return; // don't touch g_thread/g_threadId -- the thread is still alive
    }
    CloseHandle(g_thread);
    g_thread = nullptr;
  }
  DeleteCriticalSection(&g_pathLock);
}

void Wh_ModSettingsChanged() {
  Wh_Log(L"SettingsChanged");
  std::wstring oldPath = LoadSettings();
  // Only a real source change needs to interrupt playback -- fitMode and
  // batteryMode are read live (via the atomics) on the next frame/battery
  // check, so toggling those no longer causes a full reload.
  std::wstring newPath = GetVideoPathSetting();
  if (g_threadId && newPath != oldPath) {
    PostThreadMessageW(g_threadId, kMsgReloadSource, 0, 0);
  }
}