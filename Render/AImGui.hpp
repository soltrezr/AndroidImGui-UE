#pragma once

#include <imgui.h>
#include <imgui_impl_android.h>
#include <imgui_impl_opengl3.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <android/native_activity.h>
#include <string>

namespace android
{
    class AImGui
    {
    public:
        struct Options
        {
            ANativeActivity *activity = nullptr;
            bool skipScreenshot = false;
        };

    public:
        AImGui() : AImGui(Options{}) {}
        AImGui(const Options &options);

        void BeginFrame();
        void EndFrame();
        void Destroy();

    public:
        bool InitEnvironment();
        void UnInitEnvironment();

    private:
        bool m_state = false;
        int32_t m_screenWidth = -1;
        int32_t m_screenHeight = -1;
        Options m_options;

        ANativeWindow *m_nativeWindow = nullptr;
        EGLDisplay m_display = EGL_NO_DISPLAY;
        EGLSurface m_surface = EGL_NO_SURFACE;
        EGLContext m_context = EGL_NO_CONTEXT;
        ImGuiContext *m_imguiContext = nullptr;
    };

} // namespace android

