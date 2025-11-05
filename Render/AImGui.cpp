#include "AImGui.hpp"
#include "../Header/ANwCreator.hpp"

namespace android
{
    AImGui::AImGui(const Options &options)
        : m_options(options)
    {
        InitEnvironment();
    }

    AImGui::~AImGui()
    {
        UnInitEnvironment();
    }

    void AImGui::BeginFrame()
    {
        if (!m_state)
            return;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();
    }

    void AImGui::EndFrame()
    {
        if (!m_state)
            return;

        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        eglSwapBuffers(m_display, m_surface);
    }

    bool AImGui::InitEnvironment()
    {
        if (!m_options.activity)
        {
            LogError("Invalid activity");
            return false;
        }

        ANwCreator::CreateOptions createOptions;
        createOptions.name = "AImGui";
        createOptions.skipScreenshot = m_options.skipScreenshot;

        m_nativeWindow = ANwCreator::Create(m_options.activity, createOptions);
        if (!m_nativeWindow)
        {
            LogError("ANativeWindow create failed");
            return false;
        }

        ANativeWindow_acquire(m_nativeWindow);

        auto displayInfo = ANwCreator::GetDisplayInfo(m_options.activity);
        m_screenWidth = displayInfo.width;
        m_screenHeight = displayInfo.height;

        m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (EGL_NO_DISPLAY == m_display)
        {
            LogError("eglGetDisplay failed: %d", eglGetError());
            return false;
        }

        if (EGL_TRUE != eglInitialize(m_display, nullptr, nullptr))
        {
            LogError("eglInitialize failed: %d", eglGetError());
            return false;
        }

        EGLint numConfig = 0;
        EGLConfig config{};
        const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_STENCIL_SIZE, 8,
            EGL_SAMPLE_BUFFERS, 0,
            EGL_NONE};

        if (EGL_TRUE != eglChooseConfig(m_display, attribs, &config, 1, &numConfig))
        {
            LogError("eglChooseConfig failed: %d", eglGetError());
            return false;
        }

        if (0 == numConfig)
        {
            LogError("eglChooseConfig failed: Unsupported config");
            return false;
        }

        EGLint format;
        if (EGL_TRUE != eglGetConfigAttrib(m_display, config, EGL_NATIVE_VISUAL_ID, &format))
        {
            LogError("eglGetConfigAttrib failed: %d", eglGetError());
            return false;
        }

        ANativeWindow_setBuffersGeometry(m_nativeWindow, 0, 0, format);

        m_surface = eglCreateWindowSurface(m_display, config, m_nativeWindow, nullptr);
        if (EGL_NO_SURFACE == m_surface)
        {
            LogError("eglCreateWindowSurface failed: %d", eglGetError());
            return false;
        }

        const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE};

        m_context = eglCreateContext(m_display, config, EGL_NO_CONTEXT, contextAttribs);
        if (EGL_NO_CONTEXT == m_context)
        {
            LogError("eglCreateContext failed: %d", eglGetError());
            return false;
        }

        if (EGL_TRUE != eglMakeCurrent(m_display, m_surface, m_surface, m_context))
        {
            LogError("eglMakeCurrent failed: %d", eglGetError());
            return false;
        }

        IMGUI_CHECKVERSION();

        m_imguiContext = ImGui::CreateContext();
        if (nullptr == m_imguiContext)
        {
            LogError("ImGui create context failed");
            return false;
        }

        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;

        ImGui::StyleColorsDark();
        ImGui::GetStyle().ScaleAllSizes(3.0f);

        ImFontConfig fontConfig;
        fontConfig.SizePixels = 22.0f;
        io.Fonts->AddFontDefault(&fontConfig);

        if (!ImGui_ImplAndroid_Init(m_nativeWindow))
        {
            LogError("ImGui_ImplAndroid_Init failed");
            return false;
        }

        if (!ImGui_ImplOpenGL3_Init("#version 300 es"))
        {
            LogError("ImGui_ImplOpenGL3_Init failed");
            return false;
        }

        glViewport(0, 0, m_screenWidth, m_screenHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        return (m_state = true);
    }

    void AImGui::UnInitEnvironment()
    {
        m_state = false;

        if (nullptr != m_imguiContext)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplAndroid_Shutdown();
            ImGui::DestroyContext(m_imguiContext);
            m_imguiContext = nullptr;
        }

        if (EGL_NO_DISPLAY != m_display)
        {
            eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (EGL_NO_CONTEXT != m_context)
            {
                eglDestroyContext(m_display, m_context);
                m_context = EGL_NO_CONTEXT;
            }

            if (EGL_NO_SURFACE != m_surface)
            {
                eglDestroySurface(m_display, m_surface);
                m_surface = EGL_NO_SURFACE;
            }

            eglTerminate(m_display);
            m_display = EGL_NO_DISPLAY;
        }

        if (nullptr != m_nativeWindow)
        {
            ANativeWindow_release(m_nativeWindow);
            ANwCreator::Destroy(m_options.activity, m_nativeWindow);
            m_nativeWindow = nullptr;
        }
    }

} // namespace android

