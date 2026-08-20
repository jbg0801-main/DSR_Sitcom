#include "credit.h"

#include "log.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

namespace sitcom {
namespace {

constexpr wchar_t kCreditText[] = L"Sitcom mod by jbg0801 2026";

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);

PresentFn g_orig_present = nullptr;
void** g_present_slot = nullptr;
PresentFn g_present_backup = nullptr;

std::atomic<bool> g_want_credit{false};
std::atomic<bool> g_hooked{false};
std::once_flag g_hook_once;
std::mutex g_draw_mu;

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;
ID3D11Texture2D* g_tex = nullptr;
IDXGISwapChain* g_bound_swap = nullptr;
DXGI_FORMAT g_tex_format = DXGI_FORMAT_UNKNOWN;
int g_tex_w = 0;
int g_tex_h = 0;
bool g_logged_draw = false;
bool g_logged_fail = false;

HWND FindGameWindow() {
  struct Ctx {
    DWORD pid;
    HWND best;
    int best_area;
  } ctx{GetCurrentProcessId(), nullptr, 0};
  EnumWindows(
      [](HWND hwnd, LPARAM lp) -> BOOL {
        auto* c = reinterpret_cast<Ctx*>(lp);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != c->pid || !IsWindowVisible(hwnd)) {
          return TRUE;
        }
        RECT rc{};
        if (!GetClientRect(hwnd, &rc)) {
          return TRUE;
        }
        const int area = (rc.right - rc.left) * (rc.bottom - rc.top);
        if (area > c->best_area) {
          c->best_area = area;
          c->best = hwnd;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&ctx));
  return ctx.best;
}

void ReleaseDeviceObjects() {
  if (g_tex) {
    g_tex->Release();
    g_tex = nullptr;
  }
  g_tex_w = g_tex_h = 0;
  g_tex_format = DXGI_FORMAT_UNKNOWN;
  g_bound_swap = nullptr;
  g_device = nullptr;
  g_ctx = nullptr;
}

bool RenderTextBitmap(std::vector<uint32_t>* out_bgra, int* out_w, int* out_h) {
  const int w = 720;
  const int h = 56;
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HDC screen = GetDC(nullptr);
  HDC hdc = CreateCompatibleDC(screen);
  HBITMAP bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, screen);
  if (!bmp || !bits) {
    if (hdc) {
      DeleteDC(hdc);
    }
    return false;
  }
  HGDIOBJ old_bmp = SelectObject(hdc, bmp);
  RECT rc{0, 0, w, h};
  HBRUSH brush = CreateSolidBrush(RGB(18, 16, 14));
  FillRect(hdc, &rc, brush);
  DeleteObject(brush);

  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(230, 220, 190));
  HFONT font = CreateFontW(-24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
  HGDIOBJ old_font = font ? SelectObject(hdc, font) : nullptr;
  DrawTextW(hdc, kCreditText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  if (old_font) {
    SelectObject(hdc, old_font);
  }
  if (font) {
    DeleteObject(font);
  }

  out_bgra->resize(static_cast<size_t>(w * h));
  const auto* src = static_cast<const uint32_t*>(bits);
  for (int i = 0; i < w * h; ++i) {
    // Force opaque alpha — CopySubresourceRegion has no blending.
    (*out_bgra)[static_cast<size_t>(i)] = (src[i] & 0x00FFFFFFu) | 0xFF000000u;
  }

  SelectObject(hdc, old_bmp);
  DeleteObject(bmp);
  DeleteDC(hdc);
  *out_w = w;
  *out_h = h;
  return true;
}

void BgraToRgba(std::vector<uint32_t>* px) {
  for (uint32_t& p : *px) {
    const uint32_t a = p & 0xFF000000u;
    const uint32_t r = (p >> 16) & 0xFFu;
    const uint32_t g = (p >> 8) & 0xFFu;
    const uint32_t b = p & 0xFFu;
    p = a | (b << 16) | (g << 8) | r;
  }
}

bool EnsureCreditTexture(ID3D11Device* device, DXGI_FORMAT format) {
  if (g_tex && g_tex_format == format) {
    return true;
  }
  if (g_tex) {
    g_tex->Release();
    g_tex = nullptr;
  }

  std::vector<uint32_t> pixels;
  int w = 0;
  int h = 0;
  if (!RenderTextBitmap(&pixels, &w, &h)) {
    return false;
  }
  if (format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
    BgraToRgba(&pixels);
  } else if (format != DXGI_FORMAT_B8G8R8A8_UNORM && format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
    // Fall back to BGRA upload; copy may fail if mismatched — caller logs.
  }

  D3D11_TEXTURE2D_DESC td{};
  td.Width = static_cast<UINT>(w);
  td.Height = static_cast<UINT>(h);
  td.MipLevels = 1;
  td.ArraySize = 1;
  td.Format = format;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_DEFAULT;
  td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  D3D11_SUBRESOURCE_DATA init{};
  init.pSysMem = pixels.data();
  init.SysMemPitch = static_cast<UINT>(w * 4);
  if (FAILED(device->CreateTexture2D(&td, &init, &g_tex))) {
    // Some backbuffers are typeless — retry as UNORM BGRA.
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    if (format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
      // pixels already swapped to RGBA; rebuild BGRA
      pixels.clear();
      if (!RenderTextBitmap(&pixels, &w, &h)) {
        return false;
      }
    }
    if (FAILED(device->CreateTexture2D(&td, &init, &g_tex))) {
      return false;
    }
    g_tex_format = DXGI_FORMAT_B8G8R8A8_UNORM;
  } else {
    g_tex_format = format;
  }
  g_tex_w = w;
  g_tex_h = h;
  return true;
}

bool DrawCreditD3D(IDXGISwapChain* swap) {
  std::lock_guard<std::mutex> lock(g_draw_mu);

  ID3D11Device* device = nullptr;
  if (FAILED(swap->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))) ||
      !device) {
    if (!g_logged_fail) {
      LogWrite("credit: swapchain is not D3D11 (cannot draw)");
      g_logged_fail = true;
    }
    return false;
  }
  ID3D11DeviceContext* ctx = nullptr;
  device->GetImmediateContext(&ctx);
  if (!ctx) {
    device->Release();
    return false;
  }

  if (g_bound_swap != swap || g_device != device) {
    ReleaseDeviceObjects();
    g_device = device;
    g_ctx = ctx;
    g_bound_swap = swap;
    g_device->AddRef();
    g_ctx->AddRef();
  }
  device->Release();
  ctx->Release();

  if (!g_device || !g_ctx) {
    return false;
  }

  ID3D11Texture2D* back = nullptr;
  if (FAILED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back))) ||
      !back) {
    return false;
  }
  D3D11_TEXTURE2D_DESC back_desc{};
  back->GetDesc(&back_desc);

  DXGI_FORMAT copy_fmt = back_desc.Format;
  if (copy_fmt == DXGI_FORMAT_R8G8B8A8_TYPELESS) {
    copy_fmt = DXGI_FORMAT_R8G8B8A8_UNORM;
  } else if (copy_fmt == DXGI_FORMAT_B8G8R8A8_TYPELESS) {
    copy_fmt = DXGI_FORMAT_B8G8R8A8_UNORM;
  }

  if (!EnsureCreditTexture(g_device, copy_fmt)) {
    back->Release();
    if (!g_logged_fail) {
      LogWrite("credit: failed to create credit texture");
      g_logged_fail = true;
    }
    return false;
  }

  const UINT bw = back_desc.Width;
  const UINT bh = back_desc.Height;
  if (bw < 64 || bh < 64) {
    back->Release();
    return false;
  }

  const UINT tw = static_cast<UINT>(g_tex_w);
  const UINT th = static_cast<UINT>(g_tex_h);
  const UINT margin = bh / 18;
  UINT dstx = (bw > tw) ? (bw - tw) / 2 : 0;
  UINT dsty = (bh > th + margin) ? (bh - th - margin) : 0;

  D3D11_BOX box{};
  box.left = 0;
  box.top = 0;
  box.front = 0;
  box.right = tw;
  box.bottom = th;
  box.back = 1;

  // Blit opaque credit bar onto the backbuffer — no shaders / d3dcompiler needed.
  g_ctx->CopySubresourceRegion(back, 0, dstx, dsty, 0, g_tex, 0, &box);
  back->Release();

  if (!g_logged_draw) {
    LogWrite("credit: drew title credit via D3D11 Present blit");
    g_logged_draw = true;
  }
  return true;
}

HRESULT __stdcall HookedPresent(IDXGISwapChain* swap, UINT sync, UINT flags) {
  if (g_want_credit.load(std::memory_order_relaxed) && swap) {
    DrawCreditD3D(swap);
  }
  return g_orig_present ? g_orig_present(swap, sync, flags) : S_OK;
}

bool PatchPresentVtable(IDXGISwapChain* swap) {
  if (!swap) {
    return false;
  }
  void** vtable = *reinterpret_cast<void***>(swap);
  void** slot = &vtable[8];  // IDXGISwapChain::Present
  if (g_hooked.load() && g_present_slot == slot) {
    return true;
  }

  DWORD old = 0;
  if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) {
    return false;
  }
  g_present_slot = slot;
  g_present_backup = reinterpret_cast<PresentFn>(vtable[8]);
  g_orig_present = g_present_backup;
  *slot = reinterpret_cast<void*>(&HookedPresent);
  VirtualProtect(slot, sizeof(void*), old, &old);
  g_hooked.store(true);
  LogWrite("credit: IDXGISwapChain::Present hooked (D3D11 blit)");
  return true;
}

bool InstallPresentHook() {
  HWND hwnd = FindGameWindow();
  if (!hwnd) {
    hwnd = CreateWindowExW(0, L"STATIC", L"sitcom_dummy", WS_OVERLAPPED, 0, 0, 100, 100, nullptr,
                           nullptr, GetModuleHandleW(nullptr), nullptr);
  }
  if (!hwnd) {
    return false;
  }

  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 1;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.Width = 2;
  sd.BufferDesc.Height = 2;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
  ID3D11Device* dev = nullptr;
  ID3D11DeviceContext* ctx = nullptr;
  IDXGISwapChain* swap = nullptr;
  const HRESULT hr =
      D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &level, 1,
                                    D3D11_SDK_VERSION, &sd, &swap, &dev, nullptr, &ctx);
  if (FAILED(hr) || !swap) {
    LogWrite("credit: dummy D3D11 device failed (Present hook skipped)");
    return false;
  }

  const bool ok = PatchPresentVtable(swap);
  swap->Release();
  ctx->Release();
  dev->Release();
  return ok;
}

}  // namespace

void CreditUpdate(bool on_title_screen) {
  g_want_credit.store(on_title_screen, std::memory_order_relaxed);
  // Only patch Present once we actually see the title menu — avoids touching DXGI
  // during early boot.
  if (!on_title_screen) {
    return;
  }
  std::call_once(g_hook_once, []() {
    for (int i = 0; i < 50 && !g_hooked.load(); ++i) {
      if (InstallPresentHook()) {
        break;
      }
      Sleep(100);
    }
    if (!g_hooked.load()) {
      LogWrite("credit: Present hook never installed");
    }
  });
}

void CreditShutdown() {
  g_want_credit.store(false);
  {
    std::lock_guard<std::mutex> lock(g_draw_mu);
    if (g_device) {
      g_device->Release();
    }
    if (g_ctx) {
      g_ctx->Release();
    }
    ReleaseDeviceObjects();
  }
  if (g_hooked.load() && g_present_slot && g_present_backup) {
    DWORD old = 0;
    if (VirtualProtect(g_present_slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) {
      *g_present_slot = reinterpret_cast<void*>(g_present_backup);
      VirtualProtect(g_present_slot, sizeof(void*), old, &old);
    }
  }
  g_hooked.store(false);
}

}  // namespace sitcom
