#pragma once

#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_activity.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <sys/system_properties.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#ifndef LOGTAG
#define LOGTAG "ANwCreator"
#define LOGTAG_DEFINED 1
#endif

#ifndef LogInfo
#define LogInfo(formatter, ...) __android_log_print(ANDROID_LOG_INFO, LOGTAG, formatter __VA_OPT__(, ) __VA_ARGS__)
#define LogDebug(formatter, ...) __android_log_print(ANDROID_LOG_DEBUG, LOGTAG, formatter __VA_OPT__(, ) __VA_ARGS__)
#define LogError(formatter, ...) __android_log_print(ANDROID_LOG_ERROR, LOGTAG, formatter __VA_OPT__(, ) __VA_ARGS__)
#define LogInfo_DEFINED 1
#endif

namespace android::anwcreator::detail::types
{

    enum class SurfaceControlFlags : uint32_t
    {
        eHidden = 0x00000004,
        eSkipScreenshot = 0x00000040,
        eSecure = 0x00000080,
        eNonPremultiplied = 0x00000100,
        eOpaque = 0x00000400,
        eNoColorFill = 0x00004000,
    };

    enum class DisplayRotation : int32_t
    {
        Rotation0 = 0,
        Rotation90 = 1,
        Rotation180 = 2,
        Rotation270 = 3
    };

    template <typename enum_t, typename = std::enable_if_t<std::is_enum_v<enum_t>>>
    constexpr enum_t operator|(enum_t lhs, enum_t rhs)
    {
        using underlying_t = std::underlying_type_t<enum_t>;
        return static_cast<enum_t>(static_cast<underlying_t>(lhs) | static_cast<underlying_t>(rhs));
    }

    template <typename enum_t, typename = std::enable_if_t<std::is_enum_v<enum_t>>>
    constexpr enum_t operator|=(enum_t &lhs, enum_t rhs)
    {
        return (lhs = lhs | rhs);
    }

} // namespace android::anwcreator::detail::types

namespace android::anwcreator::detail::jni
{

    struct JNIEnvironment
    {
        JNIEnv *env;
        JavaVM *vm;

        JNIEnvironment(JavaVM *javaVM) : vm(javaVM), env(nullptr)
        {
            if (vm)
                vm->AttachCurrentThread(&env, nullptr);
        }

        ~JNIEnvironment() {}

        inline bool IsValid() const
        {
            return env != nullptr;
        }

        inline operator JNIEnv *() const
        {
            return env;
        }

        inline JNIEnv *operator->() const
        {
            return env;
        }

        inline bool CheckException(const char *context = nullptr)
        {
            if (!env || !env->ExceptionCheck())
                return false;

            env->ExceptionDescribe();
            env->ExceptionClear();
            return true;
        }
    };

    struct LocalRef
    {
        JNIEnv *env;
        jobject obj;

        LocalRef(JNIEnv *e, jobject o) : env(e), obj(o) {}

        ~LocalRef()
        {
            if (env && obj)
                env->DeleteLocalRef(obj);
        }

        LocalRef(const LocalRef &) = delete;
        LocalRef &operator=(const LocalRef &) = delete;

        LocalRef(LocalRef &&other) noexcept : env(other.env), obj(other.obj)
        {
            other.obj = nullptr;
        }

        inline operator jobject() const { return obj; }
        inline operator jclass() const { return reinterpret_cast<jclass>(obj); }
        inline jobject get() const { return obj; }
        inline jclass getClass() const { return reinterpret_cast<jclass>(obj); }
        inline bool IsValid() const { return obj != nullptr; }
        inline explicit operator bool() const { return obj != nullptr; }
    };

    struct GlobalRef
    {
        JNIEnv *env;
        jobject obj;

        GlobalRef(JNIEnv *e, jobject o) : env(e), obj(nullptr)
        {
            if (e && o)
                obj = e->NewGlobalRef(o);
        }

        ~GlobalRef()
        {
            Release();
        }

        void Release()
        {
            if (env && obj)
            {
                env->DeleteGlobalRef(obj);
                obj = nullptr;
            }
        }

        GlobalRef(const GlobalRef &) = delete;
        GlobalRef &operator=(const GlobalRef &) = delete;

        GlobalRef(GlobalRef &&other) noexcept : env(other.env), obj(other.obj)
        {
            other.obj = nullptr;
        }

        inline operator jobject() const { return obj; }
        inline jobject get() const { return obj; }
        inline bool IsValid() const { return obj != nullptr; }
        inline explicit operator bool() const { return obj != nullptr; }
    };

} // namespace android::anwcreator::detail::jni

namespace android::anwcreator::detail::framework
{

    struct DisplayMetrics
    {
        int32_t widthPixels;
        int32_t heightPixels;
        float density;
        int32_t densityDpi;
    };

    struct DisplayInfo
    {
        int32_t width;
        int32_t height;
        types::DisplayRotation rotation;
        float refreshRate;
    };

    class Display
    {
        public:
        static DisplayInfo GetDisplayInfo(JNIEnv *env, ANativeActivity *activity)
            {
                DisplayInfo info{};

            if (!env || !activity || !activity->clazz)
                    return info;

            jni::LocalRef activityCls(env, env->GetObjectClass(activity->clazz));
            if (!activityCls)
                    return info;

            jmethodID getWindowManager = env->GetMethodID(activityCls, "getWindowManager", "()Landroid/view/WindowManager;");
            if (!getWindowManager)
                return info;

            jni::LocalRef windowManager(env, env->CallObjectMethod(activity->clazz, getWindowManager));
            if (!windowManager || jni::JNIEnvironment(activity->vm).CheckException("Activity.getWindowManager()"))
            {
                    return info;
            }

            jni::LocalRef windowManagerCls(env, env->GetObjectClass(windowManager));
            jmethodID getDefaultDisplay = env->GetMethodID(windowManagerCls, "getDefaultDisplay", "()Landroid/view/Display;");
            if (!getDefaultDisplay)
                return info;

            jni::LocalRef display(env, env->CallObjectMethod(windowManager, getDefaultDisplay));
            if (!display || jni::JNIEnvironment(activity->vm).CheckException("WindowManager.getDefaultDisplay()"))
            {
                    return info;
            }

            jni::LocalRef displayMetricsCls(env, env->FindClass("android/util/DisplayMetrics"));
            jmethodID displayMetricsCtor = env->GetMethodID(displayMetricsCls, "<init>", "()V");
            jni::LocalRef displayMetrics(env, env->NewObject(displayMetricsCls, displayMetricsCtor));

            jni::LocalRef displayCls(env, env->GetObjectClass(display));
            jmethodID getRealMetrics = env->GetMethodID(displayCls, "getRealMetrics", "(Landroid/util/DisplayMetrics;)V");
            env->CallVoidMethod(display, getRealMetrics, displayMetrics.get());

            jfieldID widthPixelsField = env->GetFieldID(displayMetricsCls, "widthPixels", "I");
            jfieldID heightPixelsField = env->GetFieldID(displayMetricsCls, "heightPixels", "I");

            info.width = env->GetIntField(displayMetrics, widthPixelsField);
            info.height = env->GetIntField(displayMetrics, heightPixelsField);

            jmethodID getRotation = env->GetMethodID(displayCls, "getRotation", "()I");
            int32_t rotation = env->CallIntMethod(display, getRotation);
            info.rotation = static_cast<types::DisplayRotation>(rotation);

            jmethodID getRefreshRate = env->GetMethodID(displayCls, "getRefreshRate", "()F");
            if (getRefreshRate)
            {
                info.refreshRate = env->CallFloatMethod(display, getRefreshRate);
            }

                return info;
            }
    };

    class SurfaceControl
    {
    public:
        struct Builder
        {
            JNIEnv *env;
            jobject builder;
            jclass builderClass;

            Builder(JNIEnv *e) : env(e), builder(nullptr), builderClass(nullptr)
            {
                builderClass = env->FindClass("android/view/SurfaceControl$Builder");
                if (!builderClass)
                    return;

                jmethodID ctor = env->GetMethodID(builderClass, "<init>", "()V");
                builder = env->NewObject(builderClass, ctor);
            }

            ~Builder()
            {
                if (env && builder)
                    env->DeleteLocalRef(builder);
                if (env && builderClass)
                    env->DeleteLocalRef(builderClass);
            }

            Builder &SetName(const char *name)
            {
                if (!builder)
                    return *this;

                jmethodID setName = env->GetMethodID(builderClass, "setName",
                                                     "(Ljava/lang/String;)Landroid/view/SurfaceControl$Builder;");
                if (setName)
                {
                    jni::LocalRef jName(env, env->NewStringUTF(name));
                    builder = env->CallObjectMethod(builder, setName, jName.get());
                }
                return *this;
            }

            Builder &SetParent(jobject parent)
            {
                if (!builder || !parent)
                    return *this;

                jmethodID setParent = env->GetMethodID(builderClass, "setParent",
                                                       "(Landroid/view/SurfaceControl;)Landroid/view/SurfaceControl$Builder;");
                if (setParent)
                {
                    builder = env->CallObjectMethod(builder, setParent, parent);
                }
                return *this;
            }

            Builder &SetBufferSize(int32_t width, int32_t height)
            {
                if (!builder)
                    return *this;

                jmethodID setBufferSize = env->GetMethodID(builderClass, "setBufferSize",
                                                           "(II)Landroid/view/SurfaceControl$Builder;");
                if (setBufferSize)
                {
                    builder = env->CallObjectMethod(builder, setBufferSize, width, height);
                }
                return *this;
            }

            Builder &SetFlags(uint32_t flags, uint32_t mask)
            {
                if (!builder)
                    return *this;

                jmethodID setFlags = env->GetMethodID(builderClass, "setFlags",
                                                      "(II)Landroid/view/SurfaceControl$Builder;");
                if (setFlags)
                {
                    builder = env->CallObjectMethod(builder, setFlags,
                                                     static_cast<jint>(flags),
                                                     static_cast<jint>(mask));
                }
                return *this;
            }

            Builder &SetSkipScreenshot(bool skip)
            {
                if (skip)
                {
                    constexpr uint32_t FLAG_SKIP_SCREENSHOT = 0x40;
                    SetFlags(FLAG_SKIP_SCREENSHOT, FLAG_SKIP_SCREENSHOT);
                }
                return *this;
            }

            jobject Build()
            {
                if (!builder)
                    return nullptr;

                jmethodID buildMethod = env->GetMethodID(builderClass, "build", "()Landroid/view/SurfaceControl;");
                if (!buildMethod)
                    return nullptr;

                return env->CallObjectMethod(builder, buildMethod);
            }

            inline bool IsValid() const { return builder != nullptr; }
        };

        class Transaction
        {
        public:
            JNIEnv *env;
            jobject transaction;
            jclass transactionClass;

            Transaction(JNIEnv *e) : env(e), transaction(nullptr), transactionClass(nullptr)
            {
                transactionClass = env->FindClass("android/view/SurfaceControl$Transaction");
                if (!transactionClass)
                    return;

                jmethodID ctor = env->GetMethodID(transactionClass, "<init>", "()V");
                transaction = env->NewObject(transactionClass, ctor);
            }

            ~Transaction()
            {
                if (env && transaction)
                    env->DeleteLocalRef(transaction);
                if (env && transactionClass)
                    env->DeleteLocalRef(transactionClass);
            }

            Transaction &SetAlpha(jobject surfaceControl, float alpha)
            {
                if (!transaction || !surfaceControl)
                    return *this;

                jmethodID setAlpha = env->GetMethodID(transactionClass, "setAlpha",
                                                      "(Landroid/view/SurfaceControl;F)Landroid/view/SurfaceControl$Transaction;");
                if (setAlpha)
                {
                    transaction = env->CallObjectMethod(transaction, setAlpha, surfaceControl, alpha);
                }
                return *this;
            }

            Transaction &SetLayer(jobject surfaceControl, int32_t z)
            {
                if (!transaction || !surfaceControl)
                    return *this;

                jmethodID setLayer = env->GetMethodID(transactionClass, "setLayer",
                                                      "(Landroid/view/SurfaceControl;I)Landroid/view/SurfaceControl$Transaction;");
                if (setLayer)
                {
                    transaction = env->CallObjectMethod(transaction, setLayer, surfaceControl, z);
                }
                return *this;
            }

            Transaction &Show(jobject surfaceControl)
            {
                if (!transaction || !surfaceControl)
                    return *this;

                jmethodID show = env->GetMethodID(transactionClass, "show",
                                    "(Landroid/view/SurfaceControl;)Landroid/view/SurfaceControl$Transaction;");
                if (show)
                {
                    transaction = env->CallObjectMethod(transaction, show, surfaceControl);
                }
                return *this;
            }

            Transaction &Hide(jobject surfaceControl)
            {
                if (!transaction || !surfaceControl)
                    return *this;

                jmethodID hide = env->GetMethodID(transactionClass, "hide",
                                                  "(Landroid/view/SurfaceControl;)Landroid/view/SurfaceControl$Transaction;");
                if (hide)
                {
                    transaction = env->CallObjectMethod(transaction, hide, surfaceControl);
                }
                return *this;
            }

            Transaction &Remove(jobject surfaceControl)
            {
                if (!transaction || !surfaceControl)
                    return *this;

                jmethodID remove = env->GetMethodID(transactionClass, "remove",
                                                    "(Landroid/view/SurfaceControl;)Landroid/view/SurfaceControl$Transaction;");
                if (remove)
                {
                    transaction = env->CallObjectMethod(transaction, remove, surfaceControl);
                }
                return *this;
            }

            void Apply()
            {
                if (!transaction)
                    return;

                jmethodID apply = env->GetMethodID(transactionClass, "apply", "()V");
                if (apply)
                {
                    env->CallVoidMethod(transaction, apply);
                }
            }

            inline bool IsValid() const { return transaction != nullptr; }
        };

        static jobject GetParentSurfaceControl(JNIEnv *env, ANativeActivity *activity)
        {
            if (!env || !activity || !activity->clazz)
                return nullptr;

            jni::LocalRef activityCls(env, env->GetObjectClass(activity->clazz));
            jmethodID getWindow = env->GetMethodID(activityCls, "getWindow", "()Landroid/view/Window;");
            if (!getWindow)
                return nullptr;

            jni::LocalRef window(env, env->CallObjectMethod(activity->clazz, getWindow));
            if (!window)
                return nullptr;

            jni::LocalRef windowCls(env, env->GetObjectClass(window));
            jmethodID getDecorView = env->GetMethodID(windowCls, "getDecorView", "()Landroid/view/View;");
            if (!getDecorView)
                return nullptr;

            jni::LocalRef decorView(env, env->CallObjectMethod(window, getDecorView));
            if (!decorView)
                return nullptr;

            jni::LocalRef viewCls(env, env->GetObjectClass(decorView));
            jmethodID getViewRootImpl = env->GetMethodID(viewCls, "getViewRootImpl", "()Landroid/view/ViewRootImpl;");
            if (!getViewRootImpl)
                return nullptr;

            jni::LocalRef viewRootImpl(env, env->CallObjectMethod(decorView, getViewRootImpl));
            if (!viewRootImpl)
                return nullptr;

            jni::LocalRef viewRootImplCls(env, env->GetObjectClass(viewRootImpl));
            jmethodID getSurfaceControl = env->GetMethodID(viewRootImplCls, "getSurfaceControl",
                                                           "()Landroid/view/SurfaceControl;");
            if (!getSurfaceControl)
                return nullptr;

            return env->CallObjectMethod(viewRootImpl, getSurfaceControl);
        }
    };

    class Surface
    {
    public:
        static jobject CreateFromSurfaceControl(JNIEnv *env, jobject surfaceControl)
        {
            if (!env || !surfaceControl)
                return nullptr;

            jni::LocalRef surfaceCls(env, env->FindClass("android/view/Surface"));
            if (!surfaceCls)
                return nullptr;

            jmethodID ctor = env->GetMethodID(surfaceCls, "<init>", "(Landroid/view/SurfaceControl;)V");
            if (!ctor)
                return nullptr;

            return env->NewObject(surfaceCls, ctor, surfaceControl);
        }
    };

} // namespace android::anwcreator::detail::framework

namespace android::anwcreator::detail
{
    struct WindowContext
    {
        std::unique_ptr<jni::GlobalRef> surfaceControl;
        std::unique_ptr<jni::GlobalRef> surface;
        ANativeWindow *nativeWindow;
        int32_t width;
        int32_t height;
        bool skipScreenshot;

        WindowContext()
            : surfaceControl(nullptr),
              surface(nullptr),
              nativeWindow(nullptr),
              width(0),
              height(0),
              skipScreenshot(false)
        {
        }

        ~WindowContext()
        {
            Release();
        }

        void Release()
        {
            if (nativeWindow)
            {
                ANativeWindow_release(nativeWindow);
                nativeWindow = nullptr;
            }
            surface.reset();
            surfaceControl.reset();
        }
    };

} // namespace android::anwcreator::detail

namespace android
{
    class ANwCreator
    {
    public:
        struct DisplayInfo
        {
            int32_t theta;
            int32_t width;
            int32_t height;
            float refreshRate;

            DisplayInfo()
                : theta(0), width(0), height(0), refreshRate(60.0f)
            {
            }
        };

        struct CreateOptions
        {
            const char *name;
            int32_t width;
            int32_t height;
            bool skipScreenshot;

            CreateOptions()
                : name(""),
                  width(-1),
                  height(-1),
                  skipScreenshot(false)
            {
            }
        };

    public:
        static DisplayInfo GetDisplayInfo(ANativeActivity *activity)
        {
            DisplayInfo result{};

            if (!activity || !activity->vm || !activity->clazz)
                return result;

            anwcreator::detail::jni::JNIEnvironment jniEnv(activity->vm);
            if (!jniEnv.IsValid())
                return result;

            auto displayInfo = anwcreator::detail::framework::Display::GetDisplayInfo(jniEnv, activity);

            result.width = displayInfo.width;
            result.height = displayInfo.height;
            result.theta = 90 * static_cast<int32_t>(displayInfo.rotation);
            result.refreshRate = displayInfo.refreshRate;

            return result;
        }

        static ANativeWindow *Create(ANativeActivity *activity, const CreateOptions &options = CreateOptions())
        {
            if (!activity || !activity->vm || !activity->clazz)
                return nullptr;

            anwcreator::detail::jni::JNIEnvironment jniEnv(activity->vm);
            if (!jniEnv.IsValid())
                return nullptr;

            int32_t width = options.width;
            int32_t height = options.height;

            if (width <= 0 || height <= 0)
            {
                auto displayInfo = anwcreator::detail::framework::Display::GetDisplayInfo(jniEnv, activity);
                width = displayInfo.width;
                height = displayInfo.height;
            }

            jobject parentSC = anwcreator::detail::framework::SurfaceControl::GetParentSurfaceControl(jniEnv, activity);
            if (!parentSC && jniEnv.CheckException("GetParentSurfaceControl"))
                return nullptr;

            anwcreator::detail::framework::SurfaceControl::Builder builder(jniEnv);
            if (!builder.IsValid())
                return nullptr;

            builder.SetName(options.name).SetBufferSize(width, height).SetSkipScreenshot(options.skipScreenshot);

            if (parentSC)
                builder.SetParent(parentSC);

            jobject localSurfaceControl = builder.Build();
            if (!localSurfaceControl || jniEnv.CheckException("Builder.build()"))
            {
                if (parentSC)
                    jniEnv->DeleteLocalRef(parentSC);
                return nullptr;
            }

            auto context = std::make_unique<anwcreator::detail::WindowContext>();
            context->width = width;
            context->height = height;
            context->skipScreenshot = options.skipScreenshot;
            context->surfaceControl = std::make_unique<anwcreator::detail::jni::GlobalRef>(jniEnv, localSurfaceControl);
            {
                anwcreator::detail::framework::SurfaceControl::Transaction transaction(jniEnv);
                if (transaction.IsValid())
                {
                    transaction.SetAlpha(context->surfaceControl->get(), 1.0f)
                        .SetLayer(context->surfaceControl->get(), 0x7FFFFFFE)
                        .Show(context->surfaceControl->get())
                        .Apply();
                }
            }

            jobject localSurface = anwcreator::detail::framework::Surface::CreateFromSurfaceControl(jniEnv, context->surfaceControl->get());

            if (!localSurface || jniEnv.CheckException("Create Surface"))
            {
                jniEnv->DeleteLocalRef(localSurfaceControl);
                if (parentSC)
                    jniEnv->DeleteLocalRef(parentSC);
                return nullptr;
            }

            context->surface = std::make_unique<anwcreator::detail::jni::GlobalRef>(jniEnv, localSurface);

            context->nativeWindow = ANativeWindow_fromSurface(jniEnv, context->surface->get());
            if (!context->nativeWindow)
            {
                jniEnv->DeleteLocalRef(localSurface);
                jniEnv->DeleteLocalRef(localSurfaceControl);
                if (parentSC)
                    jniEnv->DeleteLocalRef(parentSC);
                return nullptr;
                }

            ANativeWindow *result = context->nativeWindow;

            m_windowContexts.emplace(result, std::move(context));

            jniEnv->DeleteLocalRef(localSurface);
            jniEnv->DeleteLocalRef(localSurfaceControl);
            if (parentSC)
                jniEnv->DeleteLocalRef(parentSC);

            return result;
        }

        static void Destroy(ANativeActivity *activity, ANativeWindow *nativeWindow)
        {
            if (!nativeWindow)
                return;

            auto it = m_windowContexts.find(nativeWindow);
            if (it == m_windowContexts.end())
            {
                ANativeWindow_release(nativeWindow);
                return;
            }

            auto &context = it->second;

            if (activity && activity->vm && context->surfaceControl && context->surfaceControl->IsValid())
            {
                anwcreator::detail::jni::JNIEnvironment jniEnv(activity->vm);
                if (jniEnv.IsValid())
                {
                    anwcreator::detail::framework::SurfaceControl::Transaction transaction(jniEnv);
                    if (transaction.IsValid())
                        transaction.Remove(context->surfaceControl->get()).Apply();
                }
            }

            context->Release();
            m_windowContexts.erase(it);
        }

        static bool IsValid(ANativeWindow *nativeWindow)
        {
            return nativeWindow && m_windowContexts.count(nativeWindow) > 0;
        }

        static bool GetWindowSize(ANativeWindow *nativeWindow, int32_t *outWidth, int32_t *outHeight)
        {
            auto it = m_windowContexts.find(nativeWindow);
            if (it == m_windowContexts.end())
                return false;

            if (outWidth)
                *outWidth = it->second->width;
            if (outHeight)
                *outHeight = it->second->height;

            return true;
        }

    private:
        inline static std::unordered_map<ANativeWindow *, std::unique_ptr<anwcreator::detail::WindowContext>> m_windowContexts;
    };

} // namespace android
