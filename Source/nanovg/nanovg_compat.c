#ifdef _WIN32
#define NANOVG_D3D11_IMPLEMENTATION
#elif defined __linux__
#define NANOVG_GLES2_IMPLEMENTATION
// TODO: add linux support
#endif

#include "nanovg_compat.h"
#include "nanovg.c"

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dxguid.lib")

NanoVGDrawCallCount nvgGetDrawCallCount(NVGcontext* ctx)
{
    NanoVGDrawCallCount callCount;
    callCount.draws  = ctx->drawCallCount;
    callCount.fill   = ctx->fillTriCount;
    callCount.stroke = ctx->strokeTriCount;
    callCount.text   = ctx->textTriCount;
    callCount.total  = ctx->drawCallCount + ctx->fillTriCount + ctx->strokeTriCount + ctx->textTriCount;
    return callCount;
}

NVGcolor nvgGetFillColor(NVGcontext* ctx)
{
    NVGstate* state = nvg__getState(ctx);
    return state->fill.innerColor;
}

void nvgCurrentScissor(NVGcontext* ctx, float* x, float* y, float* w, float* h)
{
    NVGstate* state = nvg__getState(ctx);
    float     pxform[6], invxorm[6];
    float     ex, ey, tex, tey;

    // If no previous scissor has been set, set the scissor as current scissor.
    if (state->scissor.extent[0] < 0)
    {
        return;
    }

    // Transform the current scissor rect into current transform space.
    // If there is difference in rotation, this will be approximation.
    memcpy(pxform, state->scissor.xform, sizeof(float) * 6);
    ex = state->scissor.extent[0];
    ey = state->scissor.extent[1];
    nvgTransformInverse(invxorm, state->xform);
    nvgTransformMultiply(pxform, invxorm);
    tex = ex * fabsf(pxform[0]) + ey * fabsf(pxform[2]);
    tey = ex * fabsf(pxform[1]) + ey * fabsf(pxform[3]);

    *x = pxform[4] - tex;
    *y = pxform[5] - tey;
    *w = tex * 2;
    *h = tey * 2;
}

void nvgStrokeBlur(NVGcontext* ctx, float fringeWidth)
{
    NVGstate*      state       = nvg__getState(ctx);
    float          scale       = nvg__getAverageScale(state->xform);
    float          strokeWidth = nvg__clampf(state->strokeWidth * scale, 0.0f, 200.0f);
    NVGpaint       strokePaint = state->stroke;
    const NVGpath* path;
    int            i;

    if (strokeWidth < fringeWidth)
    {
        // If the stroke width is less than pixel size, use alpha to emulate
        // coverage. Since coverage is area, scale by alpha*alpha.
        float alpha               = nvg__clampf(strokeWidth / fringeWidth, 0.0f, 1.0f);
        strokePaint.innerColor.a *= alpha * alpha;
        strokePaint.outerColor.a *= alpha * alpha;
        strokeWidth               = fringeWidth;
    }

    // Apply global alpha
    strokePaint.innerColor.a *= state->alpha;
    strokePaint.outerColor.a *= state->alpha;

    nvg__flattenPaths(ctx);

    if (ctx->params.edgeAntiAlias && state->shapeAntiAlias)
        nvg__expandStroke(ctx, strokeWidth * 0.5f, fringeWidth, state->lineCap, state->lineJoin, state->miterLimit);
    else
        nvg__expandStroke(ctx, strokeWidth * 0.5f, 0.0f, state->lineCap, state->lineJoin, state->miterLimit);

    ctx->params.renderStroke(
        ctx->params.userPtr,
        &strokePaint,
        state->compositeOperation,
        &state->scissor,
        fringeWidth,
        strokeWidth,
        ctx->cache->paths,
        ctx->cache->npaths);

    // Count triangles
    for (i = 0; i < ctx->cache->npaths; i++)
    {
        path                 = &ctx->cache->paths[i];
        ctx->strokeTriCount += path->nstroke - 2;
        ctx->drawCallCount++;
    }
}

void    nvgPopPath(NVGcontext* ctx, int N) { ctx->ncommands -= N; }
int     nvgPathLen(NVGcontext* ctx) { return ctx->ncommands; }
float** nvgGetPath(NVGcontext* ctx) { return &ctx->commands; }

#ifdef _WIN32

#define WCODE_HRESULT_FIRST MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x200)
#define WCODE_HRESULT_LAST MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF + 1, 0) - 1
#define HRESULTToWCode(r) (r >= WCODE_HRESULT_FIRST && r <= WCODE_HRESULT_LAST) ? (r - WCODE_HRESULT_FIRST) : 0

struct D3DNVGdevice* d3dnvgGetDevice(NVGcontext* ctx)
{
    struct D3DNVGcontext* D3D    = (struct D3DNVGcontext*)ctx->params.userPtr;
    struct D3DNVGdevice*  device = (struct D3DNVGdevice*)D3D->userPtr;
    return device;
}

void d3dnvgPresent(NVGcontext* ctx)
{
    struct D3DNVGdevice* device = d3dnvgGetDevice(ctx);

    D3D_API_2(device->pSwapChain, Present, 0, 0);
}

// Frees everything
static void d3dnvgDestroyDevice(struct D3DNVGdevice* device)
{
    // Detach RTs
    if (device->pDeviceContext)
    {
        ID3D11RenderTargetView* viewList[1] = {NULL};
        D3D_API_3(device->pDeviceContext, OMSetRenderTargets, 1, viewList, NULL);
    }

    D3D_API_RELEASE(device->pDeviceContext);
    D3D_API_RELEASE(device->pDevice);
    D3D_API_RELEASE(device->pSwapChain);
    D3D_API_RELEASE(device->pMainView);
    D3D_API_RELEASE(device->pDepthStencil);
    D3D_API_RELEASE(device->pDepthStencilView);

    NVG_FREE(device);
}

// Setup the device and the rendering targets
static struct D3DNVGdevice* d3dnvgCreateDevice(HWND hwnd, unsigned width, unsigned height)
{
    struct D3DNVGdevice* device = NULL;

    HRESULT       hr           = S_OK;
    IDXGIDevice*  pDXGIDevice  = NULL;
    IDXGIAdapter* pAdapter     = NULL;
    IDXGIFactory* pDXGIFactory = NULL;
    UINT          deviceFlags  = 0;
    UINT          driver       = 0;

    device = (struct D3DNVGdevice*)NVG_MALLOC(sizeof(*device));

    if (device == NULL)
    {
        return NULL;
    }
    ZeroMemory(device, sizeof(*device));

    static const D3D_DRIVER_TYPE driverAttempts[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };

    static const D3D_FEATURE_LEVEL levelAttempts[] = {
        D3D_FEATURE_LEVEL_12_1, // Direct3D 12.1 SM 6
        D3D_FEATURE_LEVEL_12_0, // Direct3D 12.0 SM 5.1
        D3D_FEATURE_LEVEL_11_1, // Direct3D 11.1 SM 5
        D3D_FEATURE_LEVEL_11_0, // Direct3D 11.0 SM 5
        D3D_FEATURE_LEVEL_10_1, // Direct3D 10.1 SM 4
        D3D_FEATURE_LEVEL_10_0, // Direct3D 10.0 SM 4
        D3D_FEATURE_LEVEL_9_3,  // Direct3D 9.3  SM 3
        D3D_FEATURE_LEVEL_9_2,  // Direct3D 9.2  SM 2
    };

    for (driver = 0; driver < ARRAYSIZE(driverAttempts); driver++)
    {
        hr = D3D11CreateDevice(
            NULL,
            driverAttempts[driver],
            NULL,
            deviceFlags,
            levelAttempts,
            ARRAYSIZE(levelAttempts),
            D3D11_SDK_VERSION,
            &device->pDevice,
            NULL,
            &device->pDeviceContext);

        if (SUCCEEDED(hr))
        {
            char text[32];
            sprintf(text, "Using graphics driver: %d\n", driver);
            OutputDebugString(text);

            break;
        }
    }

    if (FAILED(hr))
    {
        OutputDebugString("Failed D3D11CreateDevice()\n");
    }

    if (SUCCEEDED(hr))
    {
        hr = D3D_API_2(device->pDevice, QueryInterface, &IID_IDXGIDevice, (void**)&pDXGIDevice);
        if (FAILED(hr))
        {
            OutputDebugString("Failed ID3D11Device::QueryInterface()\n");
        }
    }
    if (SUCCEEDED(hr))
    {
        hr = D3D_API_1(pDXGIDevice, GetAdapter, &pAdapter);
        if (FAILED(hr))
        {
            OutputDebugString("Failed IDXGIDevice::GetAdapter()\n");
        }
    }
    if (SUCCEEDED(hr))
    {
        hr = D3D_API_2(pAdapter, GetParent, &IID_IDXGIFactory, (void**)&pDXGIFactory);
        if (FAILED(hr))
        {
            OutputDebugString("Failed IDXGIAdapter::GetParent()\n");
        }
    }
    if (SUCCEEDED(hr))
    {
        device->swapDesc.SampleDesc.Count   = 1; // The Number of Multisamples per Level
        device->swapDesc.SampleDesc.Quality = 0; // between 0(lowest Quality) and one lesser than
                                                 // pDevice->CheckMultisampleQualityLevels

        device->swapDesc.OutputWindow                       = hwnd;
        device->swapDesc.Windowed                           = TRUE;
        device->swapDesc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        device->swapDesc.BufferDesc.Width                   = width;
        device->swapDesc.BufferDesc.Height                  = height;
        device->swapDesc.BufferDesc.RefreshRate.Numerator   = 60;
        device->swapDesc.BufferDesc.RefreshRate.Denominator = 1;
        device->swapDesc.BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        device->swapDesc.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;

        // According to randos the internet, BGRA is a format favoured by most
        // hardware. Using RGBA will often mean that each pixel will have to be
        // converted (swizzled), which apprently has a ~5% performance cost.
        device->swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

        // Using a flip discard model is faster and recommended by Microsoft,
        // but the feature is only available starting from Windows 8.
        // Recommended settings:
        // https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model

        // NOTE: VLC use different settings here, and may be worth copying
        // They also use some a cool library loading trick to detech the Windows plaform
        // However, the simplest thing to do is run into a brick wall with these settings
        // https://github.com/videolan/vlc/blob/232fb13b0d6110c4d1b683cde24cf9a7f2c5c2ea/modules/video_output/win32/d3d11_swapchain.c#L263

        // First we will attempt the recommended model
        device->swapDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        device->swapDesc.BufferCount = 2;
        hr                           = D3D_API_3(
            pDXGIFactory,
            CreateSwapChain,
            (IUnknown*)device->pDevice,
            &device->swapDesc,
            &device->pSwapChain);

        if (FAILED(hr))
        {
            OutputDebugString("Failed to CreateSwapChain using DXGI_SWAP_EFFECT_FLIP_DISCARD "
                              "strategy, using fallback...\n");
            // These settings should work on Windows 7...
            device->swapDesc.SwapEffect  = DXGI_SWAP_EFFECT_DISCARD;
            device->swapDesc.BufferCount = 1;
            hr                           = D3D_API_3(
                pDXGIFactory,
                CreateSwapChain,
                (IUnknown*)device->pDevice,
                &device->swapDesc,
                &device->pSwapChain);
            if (FAILED(hr))
            {
                OutputDebugString("Failed IDXGIFactory::CreateSwapChain()\n");
            }
        }
    }

    D3D_API_RELEASE(pDXGIDevice);
    D3D_API_RELEASE(pAdapter);
    D3D_API_RELEASE(pDXGIFactory);

    if (SUCCEEDED(hr))
    {
        hr = d3dnvgSetViewBounds(device, hwnd, width, height);
        if (FAILED(hr))
        {
            OutputDebugString("Failed to set view bounds - d3dnvgSetViewBounds()\n");
        }
    }
    if (FAILED(hr))
    {
        d3dnvgDestroyDevice(device);
        return NULL;
    }

    return device;
}

NVGcontext* d3dnvgCreateContext(void* hwnd, int flags, unsigned int width, unsigned int height)
{
    NVGcontext* ctx = NULL;

    struct D3DNVGdevice* device = d3dnvgCreateDevice(hwnd, width, height);
    if (device == NULL)
    {
        OutputDebugString("Could not Initialize DX11\n");
        return NULL;
    }
    OutputDebugString("Initialized DX11\n");

    ctx = nvgCreateD3D11(device->pDevice, flags);

    if (ctx == NULL)
    {
        OutputDebugString("Failed creating NVGcontext\n");
        d3dnvgDestroyDevice(device);
        return NULL;
    }

    struct D3DNVGcontext* D3D = (struct D3DNVGcontext*)ctx->params.userPtr;
    D3D->userPtr              = device;

    return ctx;
}

void d3dnvgDeleteContext(NVGcontext* ctx)
{
    struct D3DNVGdevice* device = d3dnvgGetDevice(ctx);

    d3dnvgDestroyDevice(device);
    nvgDeleteD3D11(ctx);
}

long d3dnvgSetViewBounds(struct D3DNVGdevice* device, void* hwnd, unsigned int width, unsigned int height)
{
    D3D11_RENDER_TARGET_VIEW_DESC renderDesc;
    ID3D11RenderTargetView*       viewList[1]         = {NULL};
    HRESULT                       hr                  = S_OK;
    ID3D11Resource*               pBackBufferResource = NULL;
    D3D11_TEXTURE2D_DESC          texDesc;
    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc;

    device->swapDesc.BufferDesc.Width  = width;
    device->swapDesc.BufferDesc.Height = height;

    if (! device->pDevice || ! device->pDeviceContext)
    {
        OutputDebugString("Cannot use d3dnvgSetViewBounds(). ID3D11Device & "
                          "ID3D11DeviceContext not initialised.\n");
        return E_FAIL;
    }

    // pDeviceContext->ClearState();
    D3D_API_3(device->pDeviceContext, OMSetRenderTargets, 1, viewList, NULL);

    // Ensure that nobody is holding onto one of the old resources
    D3D_API_RELEASE(device->pMainView);
    D3D_API_RELEASE(device->pDepthStencilView);

    // Resize render target buffers
    hr = D3D_API_5(
        device->pSwapChain,
        ResizeBuffers,
        device->swapDesc.BufferCount,
        width,
        height,
        device->swapDesc.BufferDesc.Format,
        0);
    if (FAILED(hr))
    {
        OutputDebugString("Failed IDXGISwapChain::ResizeBuffers()\n");
        return hr;
    }

    // Create the render target view and set it on the device
    hr = D3D_API_3(device->pSwapChain, GetBuffer, 0, &IID_ID3D11Texture2D, (void**)&pBackBufferResource);
    if (FAILED(hr))
    {
        OutputDebugString("Failed IDXGISwapChain::GetBuffer()\n");
        return hr;
    }

    renderDesc.Format = device->swapDesc.BufferDesc.Format;
    renderDesc.ViewDimension =
        (device->swapDesc.SampleDesc.Count > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
    renderDesc.Texture2D.MipSlice = 0;

    hr = D3D_API_3(device->pDevice, CreateRenderTargetView, pBackBufferResource, &renderDesc, &device->pMainView);
    D3D_API_RELEASE(pBackBufferResource);
    if (FAILED(hr))
    {
        OutputDebugString("Failed ID3D11Device::CreateRenderTargetView()\n");
        return hr;
    }

    texDesc.ArraySize          = 1;
    texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
    texDesc.CPUAccessFlags     = 0;
    texDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    texDesc.Height             = (UINT)height;
    texDesc.Width              = (UINT)width;
    texDesc.MipLevels          = 1;
    texDesc.MiscFlags          = 0;
    texDesc.SampleDesc.Count   = device->swapDesc.SampleDesc.Count;
    texDesc.SampleDesc.Quality = device->swapDesc.SampleDesc.Quality;
    texDesc.Usage              = D3D11_USAGE_DEFAULT;

    D3D_API_RELEASE(device->pDepthStencil);
    hr = D3D_API_3(device->pDevice, CreateTexture2D, &texDesc, NULL, &device->pDepthStencil);
    if (FAILED(hr))
    {
        OutputDebugString("Failed ID3D11Device::CreateTexture2D()\n");
        return hr;
    }

    depthViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthViewDesc.ViewDimension =
        (device->swapDesc.SampleDesc.Count > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
    depthViewDesc.Flags              = 0;
    depthViewDesc.Texture2D.MipSlice = 0;

    hr = D3D_API_3(
        device->pDevice,
        CreateDepthStencilView,
        (ID3D11Resource*)device->pDepthStencil,
        &depthViewDesc,
        &device->pDepthStencilView);
    if (FAILED(hr))
    {
        OutputDebugString("Failed ID3D11Device::CreateDepthStencilView()\n");
        return hr;
    }
    return hr;
}

void d3dnvgClearWithColor(NVGcontext* ctx, NVGcolor color)
{
    struct D3DNVGdevice* device = d3dnvgGetDevice(ctx);

    D3D_API_2(device->pDeviceContext, ClearRenderTargetView, device->pTargetView, color.rgba);
}

void d3dnvgBindFramebuffer(NVGcontext* ctx, int texId)
{
    struct D3DNVGdevice* device = d3dnvgGetDevice(ctx);

    D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof(viewport));
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    if (texId == 0)
    {
        viewport.Width      = device->swapDesc.BufferDesc.Width;
        viewport.Height     = device->swapDesc.BufferDesc.Height;
        device->pTargetView = device->pMainView;
    }
    else
    {
        NVGparams*            params = nvgInternalParams(ctx);
        struct D3DNVGcontext* D3D    = (struct D3DNVGcontext*)params->userPtr;
        struct D3DNVGtexture* tex    = D3Dnvg__findTexture(D3D, texId);
        viewport.Width               = tex->width;
        viewport.Height              = tex->height;
        device->pTargetView          = tex->renderTargetView;
    }

    D3D_API_3(device->pDeviceContext, OMSetRenderTargets, 1, &device->pTargetView, device->pDepthStencilView);
    D3D_API_2(device->pDeviceContext, RSSetViewports, 1, &viewport);
}

int d3dnvgCreateFramebuffer(NVGcontext* ctx, int w, int h, int flags)
{
    NVGparams*            params = nvgInternalParams(ctx);
    struct D3DNVGcontext* D3D    = (struct D3DNVGcontext*)params->userPtr;
    struct D3DNVGdevice*  device = (struct D3DNVGdevice*)D3D->userPtr;

    int texId = nvgCreateImageRGBA(ctx, w, h, flags | NVG_IMAGE_RENDER_TARGET, NULL);

    struct D3DNVGtexture* tex = D3Dnvg__findTexture(D3D, texId);

    D3D11_RENDER_TARGET_VIEW_DESC renderDesc;
    ZeroMemory(&renderDesc, sizeof(renderDesc));
    // nanovg images use the RGBA format, so we must use the same else the red &
    // blue channels will be flipped
    renderDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    renderDesc.ViewDimension =
        (device->swapDesc.SampleDesc.Count > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
    renderDesc.Texture2D.MipSlice = 0;

    HRESULT hr = device->pDevice->lpVtbl->CreateRenderTargetView(
        device->pDevice,
        (struct ID3D11Resource*)tex->tex,
        &renderDesc,
        &tex->renderTargetView);
    if (FAILED(hr))
    {
        // WORD code = HRESULTToWCode(hr);
        OutputDebugString("Failed creating frame buffer "
                          "ID3D11Device::CreateRenderTargetView()");
        return 0;
    }

    return texId;
}

void d3dnvgCopyImage(NVGcontext* ctx, int imgDest, int imgSrc)
{
    NVGparams*            params  = nvgInternalParams(ctx);
    struct D3DNVGcontext* D3D     = (struct D3DNVGcontext*)params->userPtr;
    struct D3DNVGtexture* texDest = D3Dnvg__findTexture(D3D, imgDest);
    struct D3DNVGtexture* texSrc  = D3Dnvg__findTexture(D3D, imgSrc);

    D3D_API_2(D3D->pDeviceContext, CopyResource, (ID3D11Resource*)texDest->tex, (ID3D11Resource*)texSrc->tex);
}

void d3dnvgWriteImage(NVGcontext* ctx, int image, void* data)
{
    NVGparams*            params = nvgInternalParams(ctx);
    struct D3DNVGcontext* D3D    = (struct D3DNVGcontext*)params->userPtr;
    struct D3DNVGtexture* tex    = D3Dnvg__findTexture(D3D, image);

    D3D11_MAPPED_SUBRESOURCE resource;
    int                      width  = tex->width;
    int                      height = tex->height;

    // All the results on stackoverflow mention to use the UpdateResource function to write to a tex CPU -> GPU
    // But the resulting tex looks all messed up...
    // This strategy, Map, memcpy, Upmap seems to do the trick...
    HRESULT hr = D3D->pDeviceContext->lpVtbl
                     ->Map(D3D->pDeviceContext, (ID3D11Resource*)tex->tex, 0, D3D11_MAP_WRITE, 0, &resource);

    if (FAILED(hr))
    {
        // WORD code = HRESULTToWCode(hr);
        OutputDebugString("Failed mapping resource - ID3D11DeviceContext::Map()");
        return;
    }

    size_t row_bytes = width * sizeof(unsigned);
    for (int i = 0; i < height; i++)
    {
        memcpy((char*)resource.pData + i * resource.RowPitch, (char*)data + i * row_bytes, row_bytes);
    }

    D3D_API_2(D3D->pDeviceContext, Unmap, (ID3D11Resource*)tex->tex, 0);
}

void d3dnvgReadPixels(NVGcontext* ctx, int image, int x, int y, int width, int height, void* data)
{
    NVGparams*            params = nvgInternalParams(ctx);
    struct D3DNVGcontext* D3D    = (struct D3DNVGcontext*)params->userPtr;
    struct D3DNVGtexture* tex    = D3Dnvg__findTexture(D3D, image);

    D3D11_MAPPED_SUBRESOURCE resource;
    HRESULT                  hr = D3D->pDeviceContext->lpVtbl
                     ->Map(D3D->pDeviceContext, (ID3D11Resource*)tex->tex, 0, D3D11_MAP_READ, 0, &resource);

    if (FAILED(hr))
    {
        // WORD code = HRESULTToWCode(hr);
        OutputDebugString("Failed mapping resource - ID3D11DeviceContext::Map()");
        return;
    }

    for (int i = y; i < height; i++)
    {
        memcpy(
            (char*)data + i * width * sizeof(unsigned),
            (char*)resource.pData + i * resource.RowPitch,
            width * sizeof(unsigned));
    }

    D3D_API_2(D3D->pDeviceContext, Unmap, (ID3D11Resource*)tex->tex, 0);
}

#endif
