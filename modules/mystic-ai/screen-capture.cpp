/**
 * screen-capture.cpp
 *
 * Capture GW2's D3D11 swap-chain back buffer from inside the addon, on
 * GW2's render thread, via a Nexus RT_Render hook. See screen-capture.h
 * for rationale.
 */

#include "shared.h"
#include "screen-capture.h"
#include <d3d11.h>
#include <dxgi.h>
#include <atomic>
#include <mutex>

// ---------------------------------------------------------------------------
// Inter-thread state.
//
// Macro thread:        sets g_Request{x,y,w,h} + g_RequestPending=true,
//                      waits on g_ResultReady.
// Render thread (hook): if g_RequestPending, do the readback, fill
//                      g_ResultPixels + g_Result{W,H,Success},
//                      set g_ResultReady=true, clear g_RequestPending.
// ---------------------------------------------------------------------------

static std::atomic<bool> g_RequestPending{false};
static std::atomic<bool> g_ResultReady{false};
static std::atomic<bool> g_ResultSuccess{false};
static std::mutex        g_BufferMutex;
static std::vector<uint8_t> g_ResultPixels;
static int g_ResultW = 0;
static int g_ResultH = 0;
static int g_ReqX = 0, g_ReqY = 0, g_ReqW = 0, g_ReqH = 0;

// ---------------------------------------------------------------------------
// Render-thread hook: do the actual D3D11 readback.
// ---------------------------------------------------------------------------

static void DoCaptureOnRenderThread()
{
    g_ResultSuccess = false;
    g_ResultW = 0;
    g_ResultH = 0;

    if (!APIDefs || !APIDefs->SwapChain) return;
    auto* swapChain = (IDXGISwapChain*)APIDefs->SwapChain;

    ID3D11Texture2D* backBuffer = nullptr;
    if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                    (void**)&backBuffer)) || !backBuffer)
        return;

    ID3D11Device* device = nullptr;
    backBuffer->GetDevice(&device);
    if (!device) { backBuffer->Release(); return; }

    D3D11_TEXTURE2D_DESC bbDesc{};
    backBuffer->GetDesc(&bbDesc);

    // Create a CPU-readable staging texture that mirrors the back buffer.
    D3D11_TEXTURE2D_DESC stagingDesc = bbDesc;
    stagingDesc.Usage          = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags      = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags      = 0;

    ID3D11Texture2D* staging = nullptr;
    if (FAILED(device->CreateTexture2D(&stagingDesc, nullptr, &staging)) || !staging)
    {
        device->Release();
        backBuffer->Release();
        return;
    }

    ID3D11DeviceContext* ctx = nullptr;
    device->GetImmediateContext(&ctx);
    if (!ctx)
    {
        staging->Release();
        device->Release();
        backBuffer->Release();
        return;
    }

    ctx->CopyResource(staging, backBuffer);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)))
    {
        const int frameW = (int)bbDesc.Width;
        const int frameH = (int)bbDesc.Height;

        // We don't know whether the swap-chain format is BGRA or RGBA without
        // checking. Both are 4-bytes-per-pixel. For our two common formats
        // (R8G8B8A8_UNORM and B8G8R8A8_UNORM) the channels swap. Convert to
        // 24-bit BGR for tesseract's preprocessing pipeline.
        const bool isRGBA =
            (bbDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
             bbDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

        std::lock_guard<std::mutex> lock(g_BufferMutex);
        g_ResultPixels.assign((size_t)g_ReqW * g_ReqH * 3, 0);  // black-fill OOB

        for (int yy = 0; yy < g_ReqH; yy++)
        {
            int srcY = g_ReqY + yy;
            if (srcY < 0 || srcY >= frameH) continue;
            const uint8_t* srcRow =
                (const uint8_t*)mapped.pData + (size_t)srcY * mapped.RowPitch;
            uint8_t* dstRow = &g_ResultPixels[(size_t)yy * g_ReqW * 3];
            for (int xx = 0; xx < g_ReqW; xx++)
            {
                int srcX = g_ReqX + xx;
                if (srcX < 0 || srcX >= frameW) continue;
                const uint8_t* s = srcRow + (size_t)srcX * 4;
                uint8_t* d = dstRow + (size_t)xx * 3;
                if (isRGBA) {
                    // RGBA: s = [R,G,B,A] → BGR = [B,G,R] = [s[2],s[1],s[0]]
                    d[0] = s[2]; d[1] = s[1]; d[2] = s[0];
                } else {
                    // BGRA: s = [B,G,R,A] → BGR direct
                    d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
                }
            }
        }
        g_ResultW = g_ReqW;
        g_ResultH = g_ReqH;
        g_ResultSuccess = true;

        ctx->Unmap(staging, 0);
    }

    ctx->Release();
    staging->Release();
    device->Release();
    backBuffer->Release();
}

static void RenderCaptureHook()
{
    if (!g_RequestPending.load(std::memory_order_acquire)) return;

    DoCaptureOnRenderThread();

    g_ResultReady.store(true, std::memory_order_release);
    g_RequestPending.store(false, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void RegisterScreenCaptureHook()
{
    if (APIDefs && APIDefs->GUI_Register)
        APIDefs->GUI_Register(RT_PostRender, RenderCaptureHook);
}

void DeregisterScreenCaptureHook()
{
    if (APIDefs && APIDefs->GUI_Deregister)
        APIDefs->GUI_Deregister(RenderCaptureHook);
}

bool CaptureBackBufferRegion(int x, int y, int w, int h,
                             std::vector<uint8_t>& outPixels,
                             int timeoutMs)
{
    if (w <= 0 || h <= 0) return false;

    // Reset result + post the request.
    g_ResultReady.store(false, std::memory_order_release);
    g_ResultSuccess.store(false, std::memory_order_release);
    g_ReqX = x; g_ReqY = y; g_ReqW = w; g_ReqH = h;
    g_RequestPending.store(true, std::memory_order_release);

    // Wait for the next render-thread tick to fulfil it.
    int waited = 0;
    while (!g_ResultReady.load(std::memory_order_acquire) && waited < timeoutMs)
    {
        Sleep(5);
        waited += 5;
    }
    if (!g_ResultReady.load(std::memory_order_acquire))
    {
        // Timed out — clear the request so a stale capture doesn't get used
        // by a later request.
        g_RequestPending.store(false, std::memory_order_release);
        return false;
    }
    if (!g_ResultSuccess.load(std::memory_order_acquire)) return false;

    std::lock_guard<std::mutex> lock(g_BufferMutex);
    outPixels = g_ResultPixels;  // copy out
    return true;
}
