/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2022 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 7 End-User License
   Agreement and JUCE Privacy Policy.

   End User License Agreement: www.juce.com/juce-7-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#if JUCE_MAC
 #include <juce_gui_basics/native/juce_mac_PerScreenDisplayLinks.h>
#endif

namespace juce
{

#if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
 extern JUCE_API double getScaleFactorForWindow (HWND);
#endif

static bool contextHasTextureNpotFeature()
{
    if (getOpenGLVersion() >= Version (2))
        return true;

    // If the version is < 2, we can't use the newer extension-checking API
    // so we have to use glGetString
    const auto* extensionsBegin = glGetString (GL_EXTENSIONS);

    if (extensionsBegin == nullptr)
        return false;

    const auto* extensionsEnd = findNullTerminator (extensionsBegin);
    const std::string extensionsString (extensionsBegin, extensionsEnd);
    const auto stringTokens = StringArray::fromTokens (extensionsString.c_str(), false);
    return stringTokens.contains ("GL_ARB_texture_non_power_of_two");
}

//==============================================================================
class OpenGLContext::CachedImage  : public CachedComponentImage
{
    template <typename T, typename U>
    static constexpr bool isFlagSet (const T& t, const U& u) { return (t & u) != 0; }

    class LockedAreaAndScale
    {
    public:
        auto get() const
        {
            const ScopedLock lock (mutex);
            return std::pair{ area, scale };
        }

        void set (const auto &ifDifferent, const Rectangle<int> &newArea, double newScale)
        {
            const ScopedLock lock (mutex);
            if (area == newArea && scale == newScale)
              return;

            area = newArea;
            scale = newScale;
            ifDifferent();
        }

    private:
        CriticalSection mutex;
        Rectangle<int> area{};
        double scale = 1.0;
    };

public:
    CachedImage (OpenGLContext& c, Component& comp,
                 const OpenGLPixelFormat& pixFormat, void* contextToShare)
        : context (c),
          component (comp)
    {
        nativeContext.reset (new NativeContext (component, pixFormat, contextToShare,
                                                c.useMultisampling, c.versionRequired));

        if (nativeContext->createdOk())
            context.nativeContext = nativeContext.get();
        else
            nativeContext.reset();

        refreshDisplayLinkConnection();
    }

    ~CachedImage() override
    {
        stop();
    }

    //==============================================================================
    void start()
    {
        if (nativeContext != nullptr)
            resume();
    }

    void stop()
    {
        // signal the render thread to destroy this cached image
        auto currentState = state.fetch_or(StateFlags::pendingDestruction);
        // has it already been destroyed
        if ((currentState & StateFlags::initialised) == 0)
          return;

        // we need to wait for the cached image to be destroyed
        currentState |= StateFlags::pendingDestruction;
        renderThread->triggerRepaint();
        do
        {
            state.wait(currentState);
            currentState = state.load();
        } while ((currentState & StateFlags::initialised) != 0);
    }

    //==============================================================================
    void resume()
    {
        renderThread->add (this);
    }

    //==============================================================================
    void paint (Graphics&) override
    {
        if (MessageManager::getInstance()->isThisTheMessageThread())
        {
            updateViewportSize();
        }
        else
        {
            // If you hit this assertion, it's because paint has been called from a thread other
            // than the message thread. This commonly happens when nesting OpenGL contexts, because
            // the 'outer' OpenGL renderer will attempt to call paint on the 'inner' context's
            // component from the OpenGL thread.
            // Nesting OpenGL contexts is not directly supported, however there is a workaround:
            // https://forum.juce.com/t/opengl-how-do-3d-with-custom-shaders-and-2d-with-juce-paint-methods-work-together/28026/7
            jassertfalse;
        }
    }

    bool invalidateAll() override
    {
        triggerRepaint();
        return false;
    }

    bool invalidate (const Rectangle<int>&) override
    {
        triggerRepaint();
        return false;
    }

    void releaseResources() override
    {
        stop();
    }

    void triggerRepaint()
    {
        state |= StateFlags::pendingRender;
        renderThread->triggerRepaint();
    }

    //==============================================================================
    enum class RenderStatus
    {
        nominal,
        destroyed,
    };

    RenderStatus renderFrame()
    {
       if (! isFlagSet (state, StateFlags::initialised))
       {
           if (isFlagSet(state, StateFlags::pendingDestruction))
              return RenderStatus::destroyed;

            switch (initialiseOnThread())
            {
                case InitResult::fatal:
                case InitResult::retry: return RenderStatus::nominal;
                case InitResult::success: break;
            }
       }

        state |= StateFlags::initialised;

        if (&context != getCurrentContext())
        {
            deactivateCurrentContext();
            if (! context.makeActive())
                return RenderStatus::nominal;
        }

        if (isFlagSet(state, StateFlags::pendingDestruction))
        {
            shutdownOnThread();
            return RenderStatus::destroyed;
        }

        const auto stateToUse = state.fetch_and (StateFlags::persistent);

       #if JUCE_MAC
        // On macOS, we use a display link callback to trigger repaints, rather than
        // letting them run at full throttle
        const bool noAutomaticRepaint = true;
       #else
        const bool noAutomaticRepaint = ! context.continuousRepaint;
       #endif

        if (! isFlagSet (stateToUse, StateFlags::pendingRender) && noAutomaticRepaint)
            return RenderStatus::nominal;

        {
            NativeContext::Locker locker (*nativeContext);

            JUCE_CHECK_OPENGL_ERROR

            const auto [currentArea, currentScale] = areaAndScale.get();
            jassert(context.renderer != nullptr);

            bindVertexArray();
            glViewport (0, 0, currentArea.getWidth(), currentArea.getHeight());
            context.currentRenderScale = currentScale;
            context.renderer->renderOpenGL();
            clearGLError();
        }

        //nativeContext->swapBuffers();
        return RenderStatus::nominal;
    }

    void updateViewportSize()
    {
        JUCE_ASSERT_MESSAGE_THREAD

        if (auto* peer = component.getPeer())
        {
           #if JUCE_MAC
            updateScreen();

            const auto displayScale = Desktop::getInstance().getGlobalScaleFactor() * [this]
            {
                if (auto* view = getCurrentView())
                {
                    if ([view respondsToSelector: @selector (backingScaleFactor)])
                        return [(id) view backingScaleFactor];

                    if (auto* window = [view window])
                        return [window backingScaleFactor];
                }

                return areaAndScale.get().second;
            }();
           #else
            const auto displayScale = Desktop::getInstance().getDisplays()
                                                            .getDisplayForRect (component.getTopLevelComponent()
                                                                                        ->getScreenBounds())
                                                           ->scale;
           #endif

            const auto localBounds = component.getLocalBounds();
            const auto newArea = peer->getComponent().getLocalArea (&component, localBounds).withZeroOrigin() * displayScale;

           #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
            // Some hosts (Pro Tools 2022.7) do not take the current DPI into account when sizing
            // plugin editor windows. Instead of querying the OS for the DPI of the editor window,
            // we approximate based on the physical size of the window that was actually provided
            // for the context to draw into. This may break if the OpenGL context's component is
            // scaled differently in its width and height - but in this case, a single scale factor
            // isn't that helpful anyway.
            const auto newScale = (float) newArea.getWidth() / (float) localBounds.getWidth();
           #else
            const auto newScale = (float) displayScale;
           #endif

            areaAndScale.set ([&]
            {
                nativeContext->updateWindowPosition (peer->getAreaCoveredBy (component));
                //triggerRepaint();
            }, newArea, newScale);
        }
    }

    void bindVertexArray() noexcept
    {
        if (shouldUseCustomVAO())
            if (vertexArrayObject != 0)
                context.extensions.glBindVertexArray (vertexArrayObject);
    }

    void checkViewportBounds()
    {
        auto screenBounds = component.getTopLevelComponent()->getScreenBounds();

        if (lastScreenBounds != screenBounds)
        {
            updateViewportSize();
            lastScreenBounds = screenBounds;
        }
    }

    void handleResize()
    {
        updateViewportSize();

       #if JUCE_MAC
        if (isFlagSet (state, StateFlags::initialised))
        {
            [nativeContext->view update];

            renderFrame ();
        }
       #endif
    }

    //==============================================================================
    InitResult initialiseOnThread()
    {
        // On android, this can get called twice, so drop any previous state.
        associatedObjectNames.clear();
        associatedObjects.clear();

        context.makeActive();

        if (const auto nativeResult = nativeContext->initialiseOnRenderThread (context); nativeResult != InitResult::success)
            return nativeResult;

        gl::loadFunctions();

        if (shouldUseCustomVAO())
        {
            context.extensions.glGenVertexArrays (1, &vertexArrayObject);
            bindVertexArray();
        }

       #if JUCE_DEBUG
        if (getOpenGLVersion() >= Version { 4, 3 } && glDebugMessageCallback != nullptr)
        {
            glEnable (GL_DEBUG_OUTPUT);
            glDebugMessageCallback ([] (GLenum type, GLenum, GLuint, GLenum severity, GLsizei, const GLchar* message, const void*)
            {
                // This may reiterate issues that are also flagged by JUCE_CHECK_OPENGL_ERROR.
                // The advantage of this callback is that it will catch *all* errors, even if we
                // forget to check manually.
                DBG ("OpenGL DBG message: " << message);
                jassertquiet (type != GL_DEBUG_TYPE_ERROR && severity != GL_DEBUG_SEVERITY_HIGH);
            }, nullptr);
        }
       #endif

        const auto [currentViewportArea, _]= areaAndScale.get();
        glViewport (0, 0, currentViewportArea.getWidth(), currentViewportArea.getHeight());

        nativeContext->setSwapInterval (1);

       #if ! JUCE_OPENGL_ES
        JUCE_CHECK_OPENGL_ERROR
        shadersAvailable = OpenGLShaderProgram::getLanguageVersion() > 0;
        clearGLError();
       #endif

        textureNpotSupported = contextHasTextureNpotFeature();

        if (context.renderer != nullptr)
            context.renderer->newOpenGLContextCreated();

        return InitResult::success;
    }

    void shutdownOnThread()
    {
        if (context.renderer != nullptr)
            context.renderer->openGLContextClosing();

        if (vertexArrayObject != 0)
            context.extensions.glDeleteVertexArrays (1, &vertexArrayObject);

        associatedObjectNames.clear();
        associatedObjects.clear();
        nativeContext->shutdownOnRenderThread();
    }

    /*  Returns true if the context requires a non-zero vertex array object (VAO) to be bound.

        If the context is a compatibility context, we can just pretend that VAOs don't exist,
        and use the default VAO all the time instead. This provides a more consistent experience
        in user code, which might make calls (like glVertexPointer()) that only work when VAO 0 is
        bound in OpenGL 3.2+.
    */
    bool shouldUseCustomVAO() const
    {
       #if JUCE_OPENGL_ES
        return false;
       #else
        clearGLError();
        GLint mask = 0;
        glGetIntegerv (GL_CONTEXT_PROFILE_MASK, &mask);

        // The context isn't aware of the profile mask, so it pre-dates the core profile
        if (glGetError() == GL_INVALID_ENUM)
            return false;

        // Also assumes a compatibility profile if the mask is completely empty for some reason
        return (mask & (GLint) GL_CONTEXT_CORE_PROFILE_BIT) != 0;
       #endif
    }

    //==============================================================================
    static CachedImage* get (Component& c) noexcept
    {
        return dynamic_cast<CachedImage*> (c.getCachedComponentImage());
    }

    class RenderThread
    {
    public:
        RenderThread() = default;

        ~RenderThread()
        {
            flags.fetch_or(destructorCalled, std::memory_order_acq_rel);
            flags.notify_one();
            thread.join();
        }

        void add (CachedImage* x)
        {
            const std::scoped_lock lock { listMutex };
            images.push_back (x);
        }

        void triggerRepaint()
        {
            flags.fetch_or(renderRequested, std::memory_order_acq_rel);
            flags.notify_one();
        }

    private:
        void renderAll()
        {
            std::vector<CachedImage *> toDelete;
            while (true)
            {
                if (!waitForWork())
                  break;

                listMutex.lock();

                for (auto* x : images)
                {
                    listMutex.unlock();

                    switch (x->renderFrame())
                    {
                    case RenderStatus::destroyed:
                      toDelete.push_back(x);
                      break;
                    case RenderStatus::nominal:
                    default:
                      break;
                    }

                    listMutex.lock();
                }

                // deletion of any cached images
                for (auto *x : toDelete)
                {
                    images.remove(x);
                    x->state.fetch_and(~StateFlags::initialised);
                    x->state.notify_one();
                }

                listMutex.unlock();
            }
        }

        bool waitForWork()
        {
            int current = flags.load();
            while (true)
            {
                if (current != 0)
                  break;

                flags.wait(current);
                current = flags.load();
            }

            current = flags.fetch_and(~renderRequested);
            return ((current & ~renderRequested) & destructorCalled) == 0;
        }

        enum
        {
          renderRequested = 1 << 0,
          destructorCalled = 1 << 1
        };

        std::mutex listMutex;
        std::list<CachedImage*> images;
        std::atomic<int> flags = 0;

        std::thread thread { [this]
        {
            Thread::setCurrentThreadName ("OpenGL Renderer");
            renderAll();
        } };
    };

    void refreshDisplayLinkConnection()
    {
       #if JUCE_MAC
        if (context.continuousRepaint)
        {
            connection.emplace (sharedDisplayLinks->registerFactory ([this] (CGDirectDisplayID display)
            {
                return [this, display]
                {
                    if (auto* view = nativeContext->getNSView())
                        if (auto* window = [view window])
                            if (auto* screen = [window screen])
                                if (display == ScopedDisplayLink::getDisplayIdForScreen (screen))
                                    triggerRepaint();
                };
            }));
        }
        else
        {
            connection.reset();
        }
       #endif
    }

    //==============================================================================
    friend class NativeContext;
    std::unique_ptr<NativeContext> nativeContext;

    OpenGLContext& context;
    Component& component;

    SharedResourcePointer<RenderThread> renderThread;

    Rectangle<int> lastScreenBounds;
    GLuint vertexArrayObject = 0;
    LockedAreaAndScale areaAndScale;

    StringArray associatedObjectNames;
    ReferenceCountedArray<ReferenceCountedObject> associatedObjects;

   #if JUCE_OPENGL_ES
    bool shadersAvailable = true;
   #else
    bool shadersAvailable = false;
   #endif
    bool textureNpotSupported = false;

   #if JUCE_MAC
    NSView* getCurrentView() const
    {
        JUCE_ASSERT_MESSAGE_THREAD;

        if (auto* peer = component.getPeer())
            return static_cast<NSView*> (peer->getNativeHandle());

        return nullptr;
    }

    NSWindow* getCurrentWindow() const
    {
        JUCE_ASSERT_MESSAGE_THREAD;

        if (auto* view = getCurrentView())
            return [view window];

        return nullptr;
    }

    NSScreen* getCurrentScreen() const
    {
        JUCE_ASSERT_MESSAGE_THREAD;

        if (auto* window = getCurrentWindow())
            return [window screen];

        return nullptr;
    }

    void updateScreen()
    {
        const auto screen = getCurrentScreen();
        const auto display = ScopedDisplayLink::getDisplayIdForScreen (screen);

        if (lastDisplay.exchange (display) == display)
            return;

        const auto newRefreshPeriod = sharedDisplayLinks->getNominalVideoRefreshPeriodSForScreen (display);

        if (newRefreshPeriod != 0.0 && std::exchange (refreshPeriod, newRefreshPeriod) != newRefreshPeriod)
            nativeContext->setNominalVideoRefreshPeriodS (newRefreshPeriod);

        updateColourSpace();
    }

    void updateColourSpace()
    {
        if (auto* view = nativeContext->getNSView())
            if (auto* window = [view window])
                [window setColorSpace: [NSColorSpace sRGBColorSpace]];
    }

    std::atomic<CGDirectDisplayID> lastDisplay { 0 };
    double refreshPeriod = 0.0;

    FunctionNotificationCenterObserver observer { NSWindowDidChangeScreenNotification,
                                                  getCurrentWindow(),
                                                  [this] { updateScreen(); } };

    // Note: the NSViewComponentPeer also has a SharedResourcePointer<PerScreenDisplayLinks> to
    // avoid unnecessarily duplicating display-link threads.
    SharedResourcePointer<PerScreenDisplayLinks> sharedDisplayLinks;

    // On macOS, rather than letting swapBuffers block as appropriate, we use a display link
    // callback to mark the view as needing to repaint.
    std::optional<PerScreenDisplayLinks::Connection> connection;
   #endif

    enum StateFlags
    {
        pendingRender           = 1 << 0,
        pendingDestruction      = 1 << 1,
        initialised             = 1 << 2,

        // Flags that should retain their state after each frame
        persistent              = initialised | pendingDestruction
    };

    std::atomic<int> state { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CachedImage)
};

//==============================================================================
class OpenGLContext::Attachment  : public ComponentMovementWatcher,
                                   private Timer
{
public:
    Attachment (OpenGLContext& c, Component& comp)
       : ComponentMovementWatcher (&comp), context (c)
    {
        if (canBeAttached (comp))
            attach();
    }

    ~Attachment() override
    {
        detach();
    }

    void detach()
    {
        auto& comp = *getComponent();
        stop();
        comp.setCachedComponentImage (nullptr);
        context.nativeContext = nullptr;
    }

    void componentMovedOrResized (bool /*wasMoved*/, bool /*wasResized*/) override
    {
        auto& comp = *getComponent();

        if (isAttached (comp) != canBeAttached (comp))
            componentVisibilityChanged();

        if (comp.getWidth() > 0 && comp.getHeight() > 0
             && context.nativeContext != nullptr)
        {
            if (auto* c = CachedImage::get (comp))
                c->handleResize();

            if (auto* peer = comp.getTopLevelComponent()->getPeer())
                context.nativeContext->updateWindowPosition (peer->getAreaCoveredBy (comp));
        }
    }

    using ComponentMovementWatcher::componentMovedOrResized;

    void componentPeerChanged() override
    {
        detach();
        componentVisibilityChanged();
    }

    void componentVisibilityChanged() override
    {
        auto& comp = *getComponent();

        if (canBeAttached (comp))
        {
            if (isAttached (comp))
                comp.repaint(); // (needed when windows are un-minimised)
            else
                attach();
        }
        else
        {
            detach();
        }
    }

    using ComponentMovementWatcher::componentVisibilityChanged;

   #if JUCE_DEBUG || JUCE_LOG_ASSERTIONS
    void componentBeingDeleted (Component& c) override
    {
        /* You must call detach() or delete your OpenGLContext to remove it
           from a component BEFORE deleting the component that it is using!
        */
        jassertfalse;

        ComponentMovementWatcher::componentBeingDeleted (c);
    }
   #endif

private:
    OpenGLContext& context;

    bool canBeAttached (const Component& comp) noexcept
    {
        return (! context.overrideCanAttach) && comp.getWidth() > 0 && comp.getHeight() > 0 && isShowingOrMinimised (comp);
    }

    static bool isShowingOrMinimised (const Component& c)
    {
        if (! c.isVisible())
            return false;

        if (auto* p = c.getParentComponent())
            return isShowingOrMinimised (*p);

        return c.getPeer() != nullptr;
    }

    static bool isAttached (const Component& comp) noexcept
    {
        return comp.getCachedComponentImage() != nullptr;
    }

    void attach()
    {
        auto& comp = *getComponent();
        auto* newCachedImage = new CachedImage (context, comp,
                                                context.openGLPixelFormat,
                                                context.contextToShareWith);
        comp.setCachedComponentImage (newCachedImage);

        start();
    }

    void stop()
    {
        stopTimer();

        auto& comp = *getComponent();

       #if JUCE_MAC
        [[(NSView*) comp.getWindowHandle() window] disableScreenUpdatesUntilFlush];
       #endif

        if (auto* oldCachedImage = CachedImage::get (comp))
            oldCachedImage->stop(); // (must stop this before detaching it from the component)
    }

    void start()
    {
        auto& comp = *getComponent();

        if (auto* cachedImage = CachedImage::get (comp))
        {
            cachedImage->start(); // (must wait until this is attached before starting its thread)
            cachedImage->updateViewportSize();

            startTimer (400);
        }
    }

    void timerCallback() override
    {
        if (auto* cachedImage = CachedImage::get (*getComponent()))
            cachedImage->checkViewportBounds();
    }
};

//==============================================================================
OpenGLContext::OpenGLContext()
{
}

OpenGLContext::~OpenGLContext()
{
    detach();
}

void OpenGLContext::setRenderer (OpenGLRenderer* rendererToUse) noexcept
{
    // This method must not be called when the context has already been attached!
    // Call it before attaching your context, or use detach() first, before calling this!
    jassert (nativeContext == nullptr);

    renderer = rendererToUse;
}

void OpenGLContext::setComponentPaintingEnabled (bool shouldPaintComponent) noexcept
{
    // This method must not be called when the context has already been attached!
    // Call it before attaching your context, or use detach() first, before calling this!
    jassert (nativeContext == nullptr);

    renderComponents = shouldPaintComponent;
}

void OpenGLContext::setContinuousRepainting (bool shouldContinuouslyRepaint) noexcept
{
    continuousRepaint = shouldContinuouslyRepaint;

   #if JUCE_MAC
    if (auto* component = getTargetComponent())
    {
        detach();
        attachment.reset (new Attachment (*this, *component));
    }

    if (auto* cachedImage = getCachedImage())
        cachedImage->refreshDisplayLinkConnection();
   #endif

    triggerRepaint();
}

void OpenGLContext::setPixelFormat (const OpenGLPixelFormat& preferredPixelFormat) noexcept
{
    // This method must not be called when the context has already been attached!
    // Call it before attaching your context, or use detach() first, before calling this!
    jassert (nativeContext == nullptr);

    openGLPixelFormat = preferredPixelFormat;
}

void OpenGLContext::setTextureMagnificationFilter (OpenGLContext::TextureMagnificationFilter magFilterMode) noexcept
{
    texMagFilter = magFilterMode;
}

void OpenGLContext::setNativeSharedContext (void* nativeContextToShareWith) noexcept
{
    // This method must not be called when the context has already been attached!
    // Call it before attaching your context, or use detach() first, before calling this!
    jassert (nativeContext == nullptr);

    contextToShareWith = nativeContextToShareWith;
}

void OpenGLContext::setMultisamplingEnabled (bool b) noexcept
{
    // This method must not be called when the context has already been attached!
    // Call it before attaching your context, or use detach() first, before calling this!
    jassert (nativeContext == nullptr);

    useMultisampling = b;
}

void OpenGLContext::setOpenGLVersionRequired (OpenGLVersion v) noexcept
{
    versionRequired = v;
}

void OpenGLContext::attachTo (Component& component)
{
    component.repaint();

    if (getTargetComponent() != &component)
    {
        detach();
        attachment.reset (new Attachment (*this, component));
    }
}

void OpenGLContext::detach()
{
    if (auto* a = attachment.get())
    {
        a->detach(); // must detach before nulling our pointer
        attachment.reset();
    }

    nativeContext = nullptr;
}

bool OpenGLContext::isAttached() const noexcept
{
    return nativeContext != nullptr;
}

Component* OpenGLContext::getTargetComponent() const noexcept
{
    return attachment != nullptr ? attachment->getComponent() : nullptr;
}

OpenGLContext* OpenGLContext::getContextAttachedTo (Component& c) noexcept
{
    if (auto* ci = CachedImage::get (c))
        return &(ci->context);

    return nullptr;
}

static ThreadLocalValue<OpenGLContext*> currentThreadActiveContext;

OpenGLContext* OpenGLContext::getCurrentContext()
{
    return currentThreadActiveContext.get();
}

bool OpenGLContext::makeActive() const noexcept
{
    auto& current = currentThreadActiveContext.get();

    if (nativeContext != nullptr && nativeContext->makeActive())
    {
        current = const_cast<OpenGLContext*> (this);
        return true;
    }

    current = nullptr;
    return false;
}

bool OpenGLContext::isActive() const noexcept
{
    return nativeContext != nullptr && nativeContext->isActive();
}

void OpenGLContext::deactivateCurrentContext()
{
    NativeContext::deactivateCurrentContext();
    currentThreadActiveContext.get() = nullptr;
}

void OpenGLContext::triggerRepaint()
{
    if (auto* cachedImage = getCachedImage())
        cachedImage->triggerRepaint();
}

void OpenGLContext::swapBuffers()
{
    if (nativeContext != nullptr)
        nativeContext->swapBuffers();
}

unsigned int OpenGLContext::getFrameBufferID() const noexcept
{
    return nativeContext != nullptr ? nativeContext->getFrameBufferID() : 0;
}

bool OpenGLContext::setSwapInterval (int numFramesPerSwap)
{
    return nativeContext != nullptr && nativeContext->setSwapInterval (numFramesPerSwap);
}

int OpenGLContext::getSwapInterval() const
{
    return nativeContext != nullptr ? nativeContext->getSwapInterval() : 0;
}

void* OpenGLContext::getRawContext() const noexcept
{
    return nativeContext != nullptr ? nativeContext->getRawContext() : nullptr;
}

OpenGLContext::CachedImage* OpenGLContext::getCachedImage() const noexcept
{
    if (auto* comp = getTargetComponent())
        return CachedImage::get (*comp);

    return nullptr;
}

bool OpenGLContext::areShadersAvailable() const
{
    auto* c = getCachedImage();
    return c != nullptr && c->shadersAvailable;
}

bool OpenGLContext::isTextureNpotSupported() const
{
    auto* c = getCachedImage();
    return c != nullptr && c->textureNpotSupported;
}

ReferenceCountedObject* OpenGLContext::getAssociatedObject (const char* name) const
{
    jassert (name != nullptr);

    auto* c = getCachedImage();

    // This method must only be called from an openGL rendering callback.
    jassert (c != nullptr && nativeContext != nullptr);
    jassert (getCurrentContext() != nullptr);

    auto index = c->associatedObjectNames.indexOf (name);
    return index >= 0 ? c->associatedObjects.getUnchecked (index).get() : nullptr;
}

void OpenGLContext::setAssociatedObject (const char* name, ReferenceCountedObject* newObject)
{
    jassert (name != nullptr);

    if (auto* c = getCachedImage())
    {
        // This method must only be called from an openGL rendering callback.
        jassert (nativeContext != nullptr);
        jassert (getCurrentContext() != nullptr);

        const int index = c->associatedObjectNames.indexOf (name);

        if (index >= 0)
        {
            if (newObject != nullptr)
            {
                c->associatedObjects.set (index, newObject);
            }
            else
            {
                c->associatedObjectNames.remove (index);
                c->associatedObjects.remove (index);
            }
        }
        else if (newObject != nullptr)
        {
            c->associatedObjectNames.add (name);
            c->associatedObjects.add (newObject);
        }
    }
}

void OpenGLContext::setImageCacheSize (size_t newSize) noexcept     { imageCacheMaxSize = newSize; }
size_t OpenGLContext::getImageCacheSize() const noexcept            { return imageCacheMaxSize; }

//==============================================================================
struct DepthTestDisabler
{
    DepthTestDisabler() noexcept
    {
        glGetBooleanv (GL_DEPTH_TEST, &wasEnabled);

        if (wasEnabled)
            glDisable (GL_DEPTH_TEST);
    }

    ~DepthTestDisabler() noexcept
    {
        if (wasEnabled)
            glEnable (GL_DEPTH_TEST);
    }

    GLboolean wasEnabled;
};

//==============================================================================
void OpenGLContext::copyTexture (const Rectangle<int>& targetClipArea,
                                 const Rectangle<int>& anchorPosAndTextureSize,
                                 const int contextWidth, const int contextHeight,
                                 bool flippedVertically)
{
    if (contextWidth <= 0 || contextHeight <= 0)
        return;

    JUCE_CHECK_OPENGL_ERROR
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_BLEND);

    DepthTestDisabler depthDisabler;

    if (areShadersAvailable())
    {
        struct OverlayShaderProgram  : public ReferenceCountedObject
        {
            OverlayShaderProgram (OpenGLContext& context)
                : program (context), params (program)
            {}

            static const OverlayShaderProgram& select (OpenGLContext& context)
            {
                static const char programValueID[] = "juceGLComponentOverlayShader";
                OverlayShaderProgram* program = static_cast<OverlayShaderProgram*> (context.getAssociatedObject (programValueID));

                if (program == nullptr)
                {
                    program = new OverlayShaderProgram (context);
                    context.setAssociatedObject (programValueID, program);
                }

                program->program.use();
                return *program;
            }

            struct BuiltProgram : public OpenGLShaderProgram
            {
                explicit BuiltProgram (OpenGLContext& ctx)
                    : OpenGLShaderProgram (ctx)
                {
                    addVertexShader (OpenGLHelpers::translateVertexShaderToV3 (
                        "attribute " JUCE_HIGHP " vec2 position;"
                        "uniform " JUCE_HIGHP " vec2 screenSize;"
                        "uniform " JUCE_HIGHP " float textureBounds[4];"
                        "uniform " JUCE_HIGHP " vec2 vOffsetAndScale;"
                        "varying " JUCE_HIGHP " vec2 texturePos;"
                        "void main()"
                        "{"
                          JUCE_HIGHP " vec2 scaled = position / (0.5 * screenSize.xy);"
                          "gl_Position = vec4 (scaled.x - 1.0, 1.0 - scaled.y, 0, 1.0);"
                          "texturePos = (position - vec2 (textureBounds[0], textureBounds[1])) / vec2 (textureBounds[2], textureBounds[3]);"
                          "texturePos = vec2 (texturePos.x, vOffsetAndScale.x + vOffsetAndScale.y * texturePos.y);"
                        "}"));

                    addFragmentShader (OpenGLHelpers::translateFragmentShaderToV3 (
                        "uniform sampler2D imageTexture;"
                        "varying " JUCE_HIGHP " vec2 texturePos;"
                        "void main()"
                        "{"
                          "gl_FragColor = texture2D (imageTexture, texturePos);"
                        "}"));

                    link();
                }
            };

            struct Params
            {
                Params (OpenGLShaderProgram& prog)
                    : positionAttribute (prog, "position"),
                      screenSize (prog, "screenSize"),
                      imageTexture (prog, "imageTexture"),
                      textureBounds (prog, "textureBounds"),
                      vOffsetAndScale (prog, "vOffsetAndScale")
                {}

                void set (const float targetWidth, const float targetHeight, const Rectangle<float>& bounds, bool flipVertically) const
                {
                    const GLfloat m[] = { bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight() };
                    textureBounds.set (m, 4);
                    imageTexture.set (0);
                    screenSize.set (targetWidth, targetHeight);

                    vOffsetAndScale.set (flipVertically ? 0.0f : 1.0f,
                                         flipVertically ? 1.0f : -1.0f);
                }

                OpenGLShaderProgram::Attribute positionAttribute;
                OpenGLShaderProgram::Uniform screenSize, imageTexture, textureBounds, vOffsetAndScale;
            };

            BuiltProgram program;
            Params params;
        };

        auto left   = (GLshort) targetClipArea.getX();
        auto top    = (GLshort) targetClipArea.getY();
        auto right  = (GLshort) targetClipArea.getRight();
        auto bottom = (GLshort) targetClipArea.getBottom();
        const GLshort vertices[] = { left, bottom, right, bottom, left, top, right, top };

        auto& program = OverlayShaderProgram::select (*this);
        program.params.set ((float) contextWidth, (float) contextHeight, anchorPosAndTextureSize.toFloat(), flippedVertically);

        GLuint vertexBuffer = 0;
        extensions.glGenBuffers (1, &vertexBuffer);
        extensions.glBindBuffer (GL_ARRAY_BUFFER, vertexBuffer);
        extensions.glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW);

        auto index = (GLuint) program.params.positionAttribute.attributeID;
        extensions.glVertexAttribPointer (index, 2, GL_SHORT, GL_FALSE, 4, nullptr);
        extensions.glEnableVertexAttribArray (index);
        JUCE_CHECK_OPENGL_ERROR

        if (extensions.glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

            extensions.glBindBuffer (GL_ARRAY_BUFFER, 0);
            extensions.glUseProgram (0);
            extensions.glDisableVertexAttribArray (index);
            extensions.glDeleteBuffers (1, &vertexBuffer);
        }
        else
        {
            clearGLError();
        }
    }
    else
    {
        jassert (attachment == nullptr); // Running on an old graphics card!
    }

    JUCE_CHECK_OPENGL_ERROR
}

} // namespace juce
