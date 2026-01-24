#ifndef NANOVG_COMPAT_H
#define NANOVG_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nanovg.h"

#define NVG_ALIGN_TL (NVG_ALIGN_TOP | NVG_ALIGN_LEFT)
#define NVG_ALIGN_TC (NVG_ALIGN_TOP | NVG_ALIGN_CENTER)
#define NVG_ALIGN_TR (NVG_ALIGN_TOP | NVG_ALIGN_RIGHT)

#define NVG_ALIGN_CL (NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT)
#define NVG_ALIGN_CC (NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER)
#define NVG_ALIGN_CR (NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT)

#define NVG_ALIGN_BL (NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT)
#define NVG_ALIGN_BC (NVG_ALIGN_BOTTOM | NVG_ALIGN_CENTER)
#define NVG_ALIGN_BR (NVG_ALIGN_BOTTOM | NVG_ALIGN_RIGHT)

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef D3D11_NO_HELPERS
#define D3D11_NO_HELPERS
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <d3d11.h>
#include "nanovg_d3d11.h"

#define NVG_DEFAULT_CONTEXT_FLAGS NVG_ANTIALIAS
#define NVG_DEFAULT_PIXEL_RATIO 1.5f

typedef struct D3DNVGdevice
{
    ID3D11Device*        pDevice;
    ID3D11DeviceContext* pDeviceContext;
    IDXGISwapChain*      pSwapChain;
    DXGI_SWAP_CHAIN_DESC swapDesc;
    // Main framebuffer target, stored for reference
    ID3D11RenderTargetView* pMainView;
    ID3D11Texture2D*        pDepthStencil;
    ID3D11DepthStencilView* pDepthStencilView;
    // Currently bound framebuffer target
    ID3D11RenderTargetView* pTargetView;
} D3DNVGdevice;

D3DNVGdevice* d3dnvgGetDevice(NVGcontext*);

NVGcontext* d3dnvgCreateContext(void* hwnd, int flags, unsigned width, unsigned height);
long        d3dnvgSetViewBounds(D3DNVGdevice*, void* hwnd, unsigned width, unsigned height);
void        d3dnvgDeleteContext(NVGcontext* ctx);
void        d3dnvgClearWithColor(NVGcontext* ctx, NVGcolor color);

// Binds the output-merger render target
void d3dnvgBindFramebuffer(NVGcontext* ctx, int image);
// Creates a 2D texture to use as a render target
int d3dnvgCreateFramebuffer(NVGcontext* ctx, int w, int h, int flags);

void d3dnvgCopyImage(NVGcontext* ctx, int imgDest, int imgSrc);
void d3dnvgWriteImage(NVGcontext* ctx, int imgDest, void* data);

// Copies the pixels from the specified image into the specified `data`.
void d3dnvgReadPixels(NVGcontext* ctx, int image, int x, int y, int width, int height, void* data);

void d3dnvgCopyImage(NVGcontext* ctx, int imgDest, int imgSrc);
void d3dnvgWriteImage(NVGcontext* ctx, int imgDest, void* data);

void d3dnvgPresent(NVGcontext* ctx);

#define nvgCreateContext d3dnvgCreateContext
#define nvgDeleteContext d3dnvgDeleteContext
#define nvgBindFramebuffer d3dnvgBindFramebuffer
#define nvgCreateFramebuffer d3dnvgCreateFramebuffer
#define nvgDeleteFramebuffer nvgDeleteImage
#define nvgReadPixels d3dnvgReadPixels
#define nvgClearWithColor d3dnvgClearWithColor
#define nvgSetViewBounds(ctx, window, w, h) d3dnvgSetViewBounds(d3dnvgGetDevice(ctx), window, w, h)

#elif defined __APPLE__
#include <nanovg_mtl.h>
#define NVG_DEFAULT_CONTEXT_FLAGS (NVG_ANTIALIAS | NVG_TRIPLE_BUFFER)
#define NVG_DEFAULT_PIXEL_RATIO 2.0f

NVGcontext* mnvgCreateContext(void* view, int flags, int width, int height);
void        mnvgSetViewBounds(void* view, int width, int height);

#define nvgCreateContext mnvgCreateContext
#define nvgDeleteContext nvgDeleteMTL
#define nvgBindFramebuffer mnvgBindFramebuffer
#define nvgCreateFramebuffer mnvgCreateFramebuffer
#define nvgDeleteFramebuffer nvgDeleteImage
#define nvgClearWithColor mnvgClearWithColor
#define nvgSetViewBounds(ctx, nsview, w, h) mnvgSetViewBounds(nsview, w, h)
#define nvgReadPixels mnvgReadPixels

#elif defined __linux__
// TODO make linux work...
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>

#define nvgCreateContext nvgCreateGLES2
#define nvgDeleteContext nvgDeleteGLES2
#define nvgBindFramebuffer(ctx, fb) nvgluBindFramebuffer(fb)
#define nvgCreateFramebuffer nvgluCreateFramebuffer
#define nvgDeleteFramebuffer nvgluDeleteFramebuffer

#endif

struct NanoVGDrawCallCount
{
    int draws;
    int fill;
    int stroke;
    int text;
    int total;
};
typedef struct NanoVGDrawCallCount NanoVGDrawCallCount;

NanoVGDrawCallCount nvgGetDrawCallCount(NVGcontext* ctx);

NVGcolor nvgGetFillColor(NVGcontext* ctx);

void nvgCurrentScissor(NVGcontext* ctx, float* x, float* y, float* w, float* h);

// Fills the current path with current stroke style.
// Set fringeWidth to value > 1 to control blur amount.
// https://github.com/memononen/nanovg/issues/460
void nvgStrokeBlur(NVGcontext* ctx, float fringeWidth);

// Remove N commands from the path cache. MoveTo = 3, LineTo = 3, BezierTo = 7, QuadTo = 7, Close = 1
void    nvgPopPath(NVGcontext* ctx, int N);
int     nvgPathLen(NVGcontext* ctx);
float** nvgGetPath(NVGcontext* ctx);

#ifdef __cplusplus
}
#endif

#endif // NANOVG_COMPAT_H
