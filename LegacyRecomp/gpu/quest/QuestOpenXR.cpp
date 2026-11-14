#if defined(UNLEASHED_RECOMP_OPENXR)

#include "QuestOpenXR.h"

#include <SDL.h>
#include <SDL_system.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace quest
{
    namespace
    {
        constexpr XrViewConfigurationType VIEW_CONFIGURATION = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        constexpr float UI_PANEL_DISTANCE_METERS = 1.5f;
        constexpr float UI_PANEL_WIDTH_METERS = 1.6f;
        constexpr float UI_PANEL_HEIGHT_METERS = 0.9f;
        constexpr float UI_WIDTH = 1280.0f;
        constexpr float UI_HEIGHT = 720.0f;

        bool HasExtension(const std::vector<XrExtensionProperties>& properties, const char* name)
        {
            return std::any_of(properties.begin(), properties.end(), [name](const XrExtensionProperties& property)
            {
                return std::strcmp(property.extensionName, name) == 0;
            });
        }

        XrPath MakePath(XrInstance instance, const char* value)
        {
            XrPath path = XR_NULL_PATH;
            if (XR_FAILED(xrStringToPath(instance, value, &path)))
                return XR_NULL_PATH;
            return path;
        }

        XrVector3f RotateForward(const XrQuaternionf& q)
        {
            // OpenXR forward is -Z. Rotate that vector by the controller aim quaternion.
            return {
                -2.0f * (q.x * q.z + q.w * q.y),
                -2.0f * (q.y * q.z - q.w * q.x),
                -1.0f + 2.0f * (q.x * q.x + q.y * q.y)
            };
        }

        void CopyActionName(char (&destination)[XR_MAX_ACTION_NAME_SIZE], const char* source)
        {
            std::strncpy(destination, source, XR_MAX_ACTION_NAME_SIZE - 1);
            destination[XR_MAX_ACTION_NAME_SIZE - 1] = '\0';
        }
    }

    OpenXRRuntime& GetOpenXRRuntime()
    {
        static OpenXRRuntime runtime;
        return runtime;
    }

    bool OpenXRRuntime::SetError(const char* operation, XrResult result)
    {
        m_lastError = std::string(operation) + " failed (OpenXR result " + std::to_string(static_cast<int>(result)) + ")";
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", m_lastError.c_str());
        return false;
    }

    bool OpenXRRuntime::SetVkError(const char* operation, VkResult result)
    {
        m_lastError = std::string(operation) + " failed (Vulkan result " + std::to_string(static_cast<int>(result)) + ")";
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", m_lastError.c_str());
        return false;
    }

    bool OpenXRRuntime::Initialize()
    {
        if (m_initialized)
            return true;

        m_lastError.clear();
        if (!InitializeLoaderAndInstance())
        {
            Shutdown();
            return false;
        }
        if (!CreateVulkanInstance())
        {
            Shutdown();
            return false;
        }

        m_initialized = true;
        SDL_Log("Quest OpenXR initialized; Vulkan physical device selected by the runtime.");
        return true;
    }

    bool OpenXRRuntime::InitializeLoaderAndInstance()
    {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        if (!env || !activity)
        {
            m_lastError = "SDL did not provide the Android Java environment and Activity.";
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", m_lastError.c_str());
            return false;
        }

        JavaVM* vm = nullptr;
        if (env->GetJavaVM(&vm) != JNI_OK || !vm)
        {
            m_lastError = "Could not obtain the Android JavaVM from SDL.";
            return false;
        }

        jobject activityGlobal = env->NewGlobalRef(activity);
        if (!activityGlobal)
        {
            m_lastError = "Could not retain the Android Activity for OpenXR loader initialization.";
            return false;
        }

        m_javaVm = vm;
        m_activityGlobalRef = activityGlobal;

        PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
        XrResult result = xrGetInstanceProcAddr(
            XR_NULL_HANDLE,
            "xrInitializeLoaderKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&initializeLoader));
        if (XR_FAILED(result) || !initializeLoader)
            return SetError("xrGetInstanceProcAddr(xrInitializeLoaderKHR)", result);

        XrLoaderInitInfoAndroidKHR loaderInfo{ XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR };
        loaderInfo.applicationVM = m_javaVm;
        loaderInfo.applicationContext = m_activityGlobalRef;

        result = initializeLoader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loaderInfo));
        if (XR_FAILED(result))
            return SetError("xrInitializeLoaderKHR", result);

        uint32_t extensionCount = 0;
        result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
        if (XR_FAILED(result))
            return SetError("xrEnumerateInstanceExtensionProperties(count)", result);

        std::vector<XrExtensionProperties> availableExtensions(extensionCount);
        for (auto& extension : availableExtensions)
            extension = { XR_TYPE_EXTENSION_PROPERTIES };

        result = xrEnumerateInstanceExtensionProperties(
            nullptr, extensionCount, &extensionCount, availableExtensions.data());
        if (XR_FAILED(result))
            return SetError("xrEnumerateInstanceExtensionProperties", result);

        const char* requiredExtensions[] = {
            XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
            XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME
        };

        for (const char* extension : requiredExtensions)
        {
            if (!HasExtension(availableExtensions, extension))
            {
                m_lastError = std::string("OpenXR runtime is missing required extension: ") + extension;
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", m_lastError.c_str());
                return false;
            }
        }

        XrInstanceCreateInfoAndroidKHR androidCreateInfo{
            XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR
        };
        androidCreateInfo.applicationVM = m_javaVm;
        androidCreateInfo.applicationActivity = m_activityGlobalRef;

        XrInstanceCreateInfo instanceCreateInfo{ XR_TYPE_INSTANCE_CREATE_INFO };
        instanceCreateInfo.next = &androidCreateInfo;
        instanceCreateInfo.applicationInfo.applicationName[0] = '\0';
        std::strncpy(instanceCreateInfo.applicationInfo.applicationName,
            "Legacy Recomp VR", XR_MAX_APPLICATION_NAME_SIZE - 1);
        instanceCreateInfo.applicationInfo.applicationVersion = 1;
        std::strncpy(instanceCreateInfo.applicationInfo.engineName,
            "LegacyRecomp", XR_MAX_ENGINE_NAME_SIZE - 1);
        instanceCreateInfo.applicationInfo.engineVersion = 1;
        instanceCreateInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(requiredExtensions));
        instanceCreateInfo.enabledExtensionNames = requiredExtensions;

        result = xrCreateInstance(&instanceCreateInfo, &m_instance);
        if (XR_FAILED(result))
            return SetError("xrCreateInstance", result);

        XrSystemGetInfo systemInfo{ XR_TYPE_SYSTEM_GET_INFO };
        systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        result = xrGetSystem(m_instance, &systemInfo, &m_systemId);
        if (XR_FAILED(result))
            return SetError("xrGetSystem", result);

        auto loadProc = [this](const char* name, PFN_xrVoidFunction* function)
        {
            XrResult procResult = xrGetInstanceProcAddr(m_instance, name, function);
            return XR_SUCCEEDED(procResult) && *function != nullptr;
        };

        if (!loadProc("xrCreateVulkanInstanceKHR",
                reinterpret_cast<PFN_xrVoidFunction*>(&m_createVulkanInstance)) ||
            !loadProc("xrCreateVulkanDeviceKHR",
                reinterpret_cast<PFN_xrVoidFunction*>(&m_createVulkanDevice)) ||
            !loadProc("xrGetVulkanGraphicsDevice2KHR",
                reinterpret_cast<PFN_xrVoidFunction*>(&m_getVulkanGraphicsDevice)) ||
            !loadProc("xrGetVulkanGraphicsRequirements2KHR",
                reinterpret_cast<PFN_xrVoidFunction*>(&m_getVulkanGraphicsRequirements)))
        {
            m_lastError = "OpenXR Vulkan enable2 entry points are unavailable.";
            return false;
        }

        m_extensionsReady = true;
        return true;
    }

    bool OpenXRRuntime::CreateVulkanInstance()
    {
        VkResult volkResult = volkInitialize();
        if (volkResult != VK_SUCCESS)
            return SetVkError("volkInitialize", volkResult);

        XrGraphicsRequirementsVulkan2KHR requirements{
            XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR
        };
        XrResult result = m_getVulkanGraphicsRequirements(m_instance, m_systemId, &requirements);
        if (XR_FAILED(result))
            return SetError("xrGetVulkanGraphicsRequirements2KHR", result);

        constexpr uint32_t requestedApiVersion = VK_API_VERSION_1_2;
        if (requirements.minApiVersionSupported > requestedApiVersion ||
            requirements.maxApiVersionSupported < requestedApiVersion)
        {
            m_lastError = "Quest OpenXR runtime Vulkan API range does not include Vulkan 1.2 required by Plume.";
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", m_lastError.c_str());
            return false;
        }

        VkApplicationInfo applicationInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        applicationInfo.pApplicationName = "Legacy Recomp VR";
        applicationInfo.applicationVersion = 1;
        applicationInfo.pEngineName = "LegacyRecomp";
        applicationInfo.engineVersion = 1;
        applicationInfo.apiVersion = requestedApiVersion;

        VkInstanceCreateInfo vulkanCreateInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        vulkanCreateInfo.pApplicationInfo = &applicationInfo;

        XrVulkanInstanceCreateInfoKHR xrCreateInfo{
            XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR
        };
        xrCreateInfo.systemId = m_systemId;
        xrCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
        xrCreateInfo.vulkanCreateInfo = &vulkanCreateInfo;

        VkResult vulkanResult = VK_ERROR_INITIALIZATION_FAILED;
        result = m_createVulkanInstance(m_instance, &xrCreateInfo, &m_vkInstance, &vulkanResult);
        if (XR_FAILED(result))
            return SetError("xrCreateVulkanInstanceKHR", result);
        if (vulkanResult != VK_SUCCESS)
            return SetVkError("xrCreateVulkanInstanceKHR/Vulkan", vulkanResult);

        volkLoadInstance(m_vkInstance);

        XrVulkanGraphicsDeviceGetInfoKHR getDeviceInfo{
            XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR
        };
        getDeviceInfo.systemId = m_systemId;
        getDeviceInfo.vulkanInstance = m_vkInstance;

        result = m_getVulkanGraphicsDevice(m_instance, &getDeviceInfo, &m_vkPhysicalDevice);
        if (XR_FAILED(result))
            return SetError("xrGetVulkanGraphicsDevice2KHR", result);

        if (m_vkPhysicalDevice == VK_NULL_HANDLE)
        {
            m_lastError = "OpenXR runtime returned a null Vulkan physical device.";
            return false;
        }

        return true;
    }

    OpenXRRuntime::DeviceCreateCallback OpenXRRuntime::GetDeviceCreateCallback()
    {
        return [this](VkPhysicalDevice physicalDevice,
                      const VkDeviceCreateInfo* createInfo,
                      VkDevice* device) -> VkResult
        {
            if (!m_createVulkanDevice || !createInfo || !device ||
                physicalDevice != m_vkPhysicalDevice || m_instance == XR_NULL_HANDLE)
                return VK_ERROR_INITIALIZATION_FAILED;

            XrVulkanDeviceCreateInfoKHR xrCreateInfo{
                XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR
            };
            xrCreateInfo.systemId = m_systemId;
            xrCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
            xrCreateInfo.vulkanPhysicalDevice = physicalDevice;
            xrCreateInfo.vulkanCreateInfo = createInfo;

            VkResult vulkanResult = VK_ERROR_INITIALIZATION_FAILED;
            XrResult result = m_createVulkanDevice(m_instance, &xrCreateInfo, device, &vulkanResult);
            if (XR_FAILED(result))
            {
                SetError("xrCreateVulkanDeviceKHR", result);
                return VK_ERROR_INITIALIZATION_FAILED;
            }
            return vulkanResult;
        };
    }

    bool OpenXRRuntime::CreateSession(
        VkDevice device, uint32_t graphicsQueueFamily, uint32_t graphicsQueueIndex)
    {
        if (!m_initialized || m_session != XR_NULL_HANDLE || device == VK_NULL_HANDLE)
            return false;

        XrGraphicsBindingVulkan2KHR graphicsBinding{
            XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR
        };
        graphicsBinding.instance = m_vkInstance;
        graphicsBinding.physicalDevice = m_vkPhysicalDevice;
        graphicsBinding.device = device;
        graphicsBinding.queueFamilyIndex = graphicsQueueFamily;
        graphicsBinding.queueIndex = graphicsQueueIndex;

        XrSessionCreateInfo sessionCreateInfo{ XR_TYPE_SESSION_CREATE_INFO };
        sessionCreateInfo.next = &graphicsBinding;
        sessionCreateInfo.systemId = m_systemId;

        XrResult result = xrCreateSession(m_instance, &sessionCreateInfo, &m_session);
        if (XR_FAILED(result))
            return SetError("xrCreateSession", result);

        m_vkDevice = device;
        if (!CreateSpacesAndSwapchains() || !CreateActions())
        {
            Shutdown();
            return false;
        }

        SDL_Log("Quest OpenXR session created; awaiting XR_SESSION_STATE_READY.");
        return true;
    }

    bool OpenXRRuntime::CreateSpacesAndSwapchains()
    {
        XrReferenceSpaceCreateInfo appSpaceInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        appSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        appSpaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
        XrResult result = xrCreateReferenceSpace(m_session, &appSpaceInfo, &m_appSpace);
        if (XR_FAILED(result))
            return SetError("xrCreateReferenceSpace(LOCAL)", result);

        XrReferenceSpaceCreateInfo viewSpaceInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        viewSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        viewSpaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
        result = xrCreateReferenceSpace(m_session, &viewSpaceInfo, &m_viewSpace);
        if (XR_FAILED(result))
            return SetError("xrCreateReferenceSpace(VIEW)", result);

        uint32_t viewCount = 0;
        result = xrEnumerateViewConfigurationViews(
            m_instance, m_systemId, VIEW_CONFIGURATION, 0, &viewCount, nullptr);
        if (XR_FAILED(result))
            return SetError("xrEnumerateViewConfigurationViews(count)", result);
        if (viewCount != 2)
        {
            m_lastError = "Quest primary stereo view configuration did not return exactly two views.";
            return false;
        }

        std::vector<XrViewConfigurationView> configurationViews(viewCount);
        for (auto& view : configurationViews)
            view = { XR_TYPE_VIEW_CONFIGURATION_VIEW };

        result = xrEnumerateViewConfigurationViews(
            m_instance, m_systemId, VIEW_CONFIGURATION, viewCount, &viewCount, configurationViews.data());
        if (XR_FAILED(result))
            return SetError("xrEnumerateViewConfigurationViews", result);

        for (uint32_t eye = 0; eye < 2; ++eye)
        {
            if (!CreateEyeSwapchain(
                eye,
                configurationViews[eye].recommendedImageRectWidth,
                configurationViews[eye].recommendedImageRectHeight))
                return false;
        }

        return true;
    }

    bool OpenXRRuntime::CreateEyeSwapchain(uint32_t eye, uint32_t width, uint32_t height)
    {
        uint32_t formatCount = 0;
        XrResult result = xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr);
        if (XR_FAILED(result))
            return SetError("xrEnumerateSwapchainFormats(count)", result);

        std::vector<int64_t> formats(formatCount);
        result = xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data());
        if (XR_FAILED(result))
            return SetError("xrEnumerateSwapchainFormats", result);

        VkFormat selectedFormat = VK_FORMAT_R8G8B8A8_UNORM;
        if (std::find(formats.begin(), formats.end(), static_cast<int64_t>(selectedFormat)) == formats.end())
        {
            selectedFormat = VK_FORMAT_B8G8R8A8_UNORM;
            if (std::find(formats.begin(), formats.end(), static_cast<int64_t>(selectedFormat)) == formats.end())
            {
                m_lastError = "OpenXR runtime offers no RGBA8/BGRA8 color swapchain format supported by the current Plume path.";
                return false;
            }
        }

        XrSwapchainCreateInfo createInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
        createInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.format = static_cast<int64_t>(selectedFormat);
        createInfo.sampleCount = 1;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.faceCount = 1;
        createInfo.arraySize = 1;
        createInfo.mipCount = 1;

        EyeSwapchain& swapchain = m_eyeSwapchains[eye];
        result = xrCreateSwapchain(m_session, &createInfo, &swapchain.handle);
        if (XR_FAILED(result))
            return SetError("xrCreateSwapchain", result);

        swapchain.width = width;
        swapchain.height = height;

        uint32_t imageCount = 0;
        result = xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount, nullptr);
        if (XR_FAILED(result))
            return SetError("xrEnumerateSwapchainImages(count)", result);

        swapchain.images.resize(imageCount);
        for (auto& image : swapchain.images)
            image = { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR };

        result = xrEnumerateSwapchainImages(
            swapchain.handle, imageCount, &imageCount,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchain.images.data()));
        if (XR_FAILED(result))
            return SetError("xrEnumerateSwapchainImages", result);

        return true;
    }

    bool OpenXRRuntime::CreateActions()
    {
        m_handPaths[0] = MakePath(m_instance, "/user/hand/left");
        m_handPaths[1] = MakePath(m_instance, "/user/hand/right");
        if (m_handPaths[0] == XR_NULL_PATH || m_handPaths[1] == XR_NULL_PATH)
        {
            m_lastError = "Could not create OpenXR left/right hand paths.";
            return false;
        }

        XrActionSetCreateInfo setInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
        CopyActionName(setInfo.actionSetName, "gameplay");
        std::strncpy(setInfo.localizedActionSetName, "Gameplay", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
        setInfo.priority = 0;
        XrResult result = xrCreateActionSet(m_instance, &setInfo, &m_actionSet);
        if (XR_FAILED(result))
            return SetError("xrCreateActionSet", result);

        auto createAction = [this](XrActionType type, const char* name, const char* localized, XrAction* action)
        {
            XrActionCreateInfo info{ XR_TYPE_ACTION_CREATE_INFO };
            info.actionType = type;
            info.countSubactionPaths = static_cast<uint32_t>(m_handPaths.size());
            info.subactionPaths = m_handPaths.data();
            CopyActionName(info.actionName, name);
            std::strncpy(info.localizedActionName, localized, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
            XrResult createResult = xrCreateAction(m_actionSet, &info, action);
            if (XR_FAILED(createResult))
                SetError("xrCreateAction", createResult);
            return XR_SUCCEEDED(createResult);
        };

        if (!createAction(XR_ACTION_TYPE_POSE_INPUT, "aim_pose", "Aim Pose", &m_aimPoseAction) ||
            !createAction(XR_ACTION_TYPE_POSE_INPUT, "grip_pose", "Grip Pose", &m_gripPoseAction) ||
            !createAction(XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Trigger", &m_triggerAction) ||
            !createAction(XR_ACTION_TYPE_FLOAT_INPUT, "squeeze", "Squeeze", &m_squeezeAction) ||
            !createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "primary_button", "Primary Button", &m_primaryAction) ||
            !createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "secondary_button", "Secondary Button", &m_secondaryAction) ||
            !createAction(XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick", "Thumbstick", &m_thumbstickAction))
            return false;

        const XrPath interactionProfile = MakePath(m_instance, "/interaction_profiles/oculus/touch_controller");
        if (interactionProfile == XR_NULL_PATH)
        {
            m_lastError = "Could not create the Oculus Touch interaction profile path.";
            return false;
        }

        std::vector<XrActionSuggestedBinding> bindings;
        auto addBinding = [this, &bindings](XrAction action, const char* path)
        {
            const XrPath bindingPath = MakePath(m_instance, path);
            if (bindingPath != XR_NULL_PATH)
                bindings.push_back({ action, bindingPath });
        };

        addBinding(m_aimPoseAction, "/user/hand/left/input/aim/pose");
        addBinding(m_aimPoseAction, "/user/hand/right/input/aim/pose");
        addBinding(m_gripPoseAction, "/user/hand/left/input/grip/pose");
        addBinding(m_gripPoseAction, "/user/hand/right/input/grip/pose");
        addBinding(m_triggerAction, "/user/hand/left/input/trigger/value");
        addBinding(m_triggerAction, "/user/hand/right/input/trigger/value");
        addBinding(m_squeezeAction, "/user/hand/left/input/squeeze/value");
        addBinding(m_squeezeAction, "/user/hand/right/input/squeeze/value");
        addBinding(m_primaryAction, "/user/hand/left/input/x/click");
        addBinding(m_primaryAction, "/user/hand/right/input/a/click");
        addBinding(m_secondaryAction, "/user/hand/left/input/y/click");
        addBinding(m_secondaryAction, "/user/hand/right/input/b/click");
        addBinding(m_thumbstickAction, "/user/hand/left/input/thumbstick");
        addBinding(m_thumbstickAction, "/user/hand/right/input/thumbstick");

        XrInteractionProfileSuggestedBinding suggestedBindings{
            XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING
        };
        suggestedBindings.interactionProfile = interactionProfile;
        suggestedBindings.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
        suggestedBindings.suggestedBindings = bindings.data();
        result = xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings);
        if (XR_FAILED(result))
            return SetError("xrSuggestInteractionProfileBindings(Touch)", result);

        for (uint32_t hand = 0; hand < 2; ++hand)
        {
            XrActionSpaceCreateInfo aimInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
            aimInfo.action = m_aimPoseAction;
            aimInfo.subactionPath = m_handPaths[hand];
            aimInfo.poseInActionSpace.orientation.w = 1.0f;
            result = xrCreateActionSpace(m_session, &aimInfo, &m_aimSpaces[hand]);
            if (XR_FAILED(result))
                return SetError("xrCreateActionSpace(aim)", result);

            XrActionSpaceCreateInfo gripInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
            gripInfo.action = m_gripPoseAction;
            gripInfo.subactionPath = m_handPaths[hand];
            gripInfo.poseInActionSpace.orientation.w = 1.0f;
            result = xrCreateActionSpace(m_session, &gripInfo, &m_gripSpaces[hand]);
            if (XR_FAILED(result))
                return SetError("xrCreateActionSpace(grip)", result);
        }

        XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &m_actionSet;
        result = xrAttachSessionActionSets(m_session, &attachInfo);
        if (XR_FAILED(result))
            return SetError("xrAttachSessionActionSets", result);

        return true;
    }

    void OpenXRRuntime::PollEvents()
    {
        if (m_instance == XR_NULL_HANDLE)
            return;

        XrEventDataBuffer eventData{ XR_TYPE_EVENT_DATA_BUFFER };
        while (xrPollEvent(m_instance, &eventData) == XR_SUCCESS)
        {
            switch (eventData.type)
            {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
            {
                const auto* changed = reinterpret_cast<const XrEventDataSessionStateChanged*>(&eventData);
                m_sessionState = changed->state;

                if (changed->state == XR_SESSION_STATE_READY && !m_sessionRunning)
                {
                    XrSessionBeginInfo beginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
                    beginInfo.primaryViewConfigurationType = VIEW_CONFIGURATION;
                    XrResult result = xrBeginSession(m_session, &beginInfo);
                    if (XR_SUCCEEDED(result))
                        m_sessionRunning = true;
                    else
                        SetError("xrBeginSession", result);
                }
                else if (changed->state == XR_SESSION_STATE_STOPPING && m_sessionRunning)
                {
                    XrResult result = xrEndSession(m_session);
                    if (XR_FAILED(result))
                        SetError("xrEndSession", result);
                    m_sessionRunning = false;
                }
                else if (changed->state == XR_SESSION_STATE_EXITING ||
                         changed->state == XR_SESSION_STATE_LOSS_PENDING)
                {
                    m_sessionRunning = false;
                }
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                m_lastError = "OpenXR runtime reported instance loss pending.";
                m_sessionRunning = false;
                break;
            default:
                break;
            }

            eventData = { XR_TYPE_EVENT_DATA_BUFFER };
        }
    }

    bool OpenXRRuntime::BeginFrame()
    {
        PollEvents();
        if (!m_sessionRunning || m_session == XR_NULL_HANDLE || m_frameBegun)
            return false;

        m_frameState = { XR_TYPE_FRAME_STATE };
        XrFrameWaitInfo waitInfo{ XR_TYPE_FRAME_WAIT_INFO };
        XrResult result = xrWaitFrame(m_session, &waitInfo, &m_frameState);
        if (XR_FAILED(result))
            return SetError("xrWaitFrame", result);

        XrFrameBeginInfo beginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
        result = xrBeginFrame(m_session, &beginInfo);
        if (XR_FAILED(result))
            return SetError("xrBeginFrame", result);

        m_frameBegun = true;
        m_predictedDisplayTime = m_frameState.predictedDisplayTime;

        XrActiveActionSet activeSet{ m_actionSet, XR_NULL_PATH };
        XrActionsSyncInfo syncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeSet;
        result = xrSyncActions(m_session, &syncInfo);
        if (XR_FAILED(result) && result != XR_SESSION_NOT_FOCUSED)
        {
            SetError("xrSyncActions", result);
            return false;
        }

        UpdateActions(m_predictedDisplayTime);

        if (!m_frameState.shouldRender)
            return true;

        XrViewLocateInfo locateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
        locateInfo.viewConfigurationType = VIEW_CONFIGURATION;
        locateInfo.displayTime = m_predictedDisplayTime;
        locateInfo.space = m_appSpace;

        uint32_t viewCount = 0;
        result = xrLocateViews(m_session, &locateInfo, &m_viewState,
            static_cast<uint32_t>(m_views.size()), &viewCount, m_views.data());
        if (XR_FAILED(result))
        {
            SetError("xrLocateViews", result);
            return false;
        }
        if (viewCount != 2)
        {
            m_lastError = "OpenXR returned a non-stereo view count during frame location.";
            return false;
        }

        for (uint32_t eye = 0; eye < 2; ++eye)
        {
            EyeSwapchain& swapchain = m_eyeSwapchains[eye];
            XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
            result = xrAcquireSwapchainImage(swapchain.handle, &acquireInfo, &swapchain.imageIndex);
            if (XR_FAILED(result))
            {
                SetError("xrAcquireSwapchainImage", result);
                return false;
            }
            swapchain.acquired = true;

            XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
            waitInfo.timeout = XR_INFINITE_DURATION;
            result = xrWaitSwapchainImage(swapchain.handle, &waitInfo);
            if (XR_FAILED(result))
            {
                SetError("xrWaitSwapchainImage", result);
                return false;
            }
        }

        return true;
    }

    void OpenXRRuntime::UpdateActions(XrTime displayTime)
    {
        for (uint32_t hand = 0; hand < 2; ++hand)
        {
            TouchControllerState& state = m_controllers[hand];
            state.aimTracked = LocateActionPose(m_aimPoseAction, m_handPaths[hand], m_aimSpaces[hand], m_viewSpace, displayTime, state.aimPose);
            state.gripTracked = LocateActionPose(m_gripPoseAction, m_handPaths[hand], m_gripSpaces[hand], m_viewSpace, displayTime, state.gripPose);

            auto readFloatPressed = [this, hand](XrAction action, bool& value)
            {
                XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
                getInfo.action = action;
                getInfo.subactionPath = m_handPaths[hand];
                XrActionStateFloat actionState{ XR_TYPE_ACTION_STATE_FLOAT };
                if (XR_SUCCEEDED(xrGetActionStateFloat(m_session, &getInfo, &actionState)))
                    value = actionState.isActive && actionState.currentState >= 0.55f;
                else
                    value = false;
            };

            auto readBoolean = [this, hand](XrAction action, bool& value)
            {
                XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
                getInfo.action = action;
                getInfo.subactionPath = m_handPaths[hand];
                XrActionStateBoolean actionState{ XR_TYPE_ACTION_STATE_BOOLEAN };
                if (XR_SUCCEEDED(xrGetActionStateBoolean(m_session, &getInfo, &actionState)))
                    value = actionState.isActive && actionState.currentState;
                else
                    value = false;
            };

            readFloatPressed(m_triggerAction, state.triggerDown);
            readFloatPressed(m_squeezeAction, state.squeezeDown);
            readBoolean(m_primaryAction, state.primaryDown);
            readBoolean(m_secondaryAction, state.secondaryDown);

            XrActionStateGetInfo stickGetInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
            stickGetInfo.action = m_thumbstickAction;
            stickGetInfo.subactionPath = m_handPaths[hand];
            XrActionStateVector2f stickState{ XR_TYPE_ACTION_STATE_VECTOR2F };
            if (XR_SUCCEEDED(xrGetActionStateVector2f(m_session, &stickGetInfo, &stickState)) && stickState.isActive)
                state.thumbstick = stickState.currentState;
            else
                state.thumbstick = { 0.0f, 0.0f };
        }
    }

    bool OpenXRRuntime::LocateActionPose(
        XrAction action, XrPath subactionPath, XrSpace space, XrSpace baseSpace, XrTime time, XrPosef& pose)
    {
        XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action = action;
        getInfo.subactionPath = subactionPath;

        XrActionStatePose poseState{ XR_TYPE_ACTION_STATE_POSE };
        if (XR_FAILED(xrGetActionStatePose(m_session, &getInfo, &poseState)) || !poseState.isActive)
            return false;

        XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
        if (XR_FAILED(xrLocateSpace(space, baseSpace, time, &location)))
            return false;

        constexpr XrSpaceLocationFlags validFlags =
            XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if ((location.locationFlags & validFlags) != validFlags)
            return false;

        pose = location.pose;
        return true;
    }

    void OpenXRRuntime::EndFrame(bool projectionLayerReady)
    {
        if (!m_frameBegun)
            return;

        for (auto& swapchain : m_eyeSwapchains)
        {
            if (!swapchain.acquired)
                continue;

            XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
            XrResult result = xrReleaseSwapchainImage(swapchain.handle, &releaseInfo);
            if (XR_FAILED(result))
                SetError("xrReleaseSwapchainImage", result);
            swapchain.acquired = false;
        }

        XrCompositionLayerProjectionView projectionViews[2] = {
            { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW },
            { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW }
        };

        for (uint32_t eye = 0; eye < 2; ++eye)
        {
            projectionViews[eye].pose = m_views[eye].pose;
            projectionViews[eye].fov = m_views[eye].fov;
            projectionViews[eye].subImage.swapchain = m_eyeSwapchains[eye].handle;
            projectionViews[eye].subImage.imageRect.offset = { 0, 0 };
            projectionViews[eye].subImage.imageRect.extent = {
                static_cast<int32_t>(m_eyeSwapchains[eye].width),
                static_cast<int32_t>(m_eyeSwapchains[eye].height)
            };
            projectionViews[eye].subImage.imageArrayIndex = 0;
        }

        XrCompositionLayerProjection projectionLayer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        projectionLayer.space = m_appSpace;
        projectionLayer.viewCount = 2;
        projectionLayer.views = projectionViews;

        const XrCompositionLayerBaseHeader* layers[] = {
            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer)
        };

        XrFrameEndInfo endInfo{ XR_TYPE_FRAME_END_INFO };
        endInfo.displayTime = m_predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

        // Until Plume records the guest scene into both acquired eye images, submit no
        // projection layer rather than presenting uninitialized swapchain memory.
        if (projectionLayerReady && m_frameState.shouldRender)
        {
            endInfo.layerCount = 1;
            endInfo.layers = layers;
        }

        XrResult result = xrEndFrame(m_session, &endInfo);
        if (XR_FAILED(result))
            SetError("xrEndFrame", result);

        m_frameBegun = false;
    }

    VkImage OpenXRRuntime::GetEyeImage(uint32_t eye) const
    {
        if (eye >= m_eyeSwapchains.size())
            return VK_NULL_HANDLE;
        const EyeSwapchain& swapchain = m_eyeSwapchains[eye];
        if (!swapchain.acquired || swapchain.imageIndex >= swapchain.images.size())
            return VK_NULL_HANDLE;
        return swapchain.images[swapchain.imageIndex].image;
    }

    XrSwapchain OpenXRRuntime::GetEyeSwapchain(uint32_t eye) const
    {
        return eye < m_eyeSwapchains.size() ? m_eyeSwapchains[eye].handle : XR_NULL_HANDLE;
    }

    uint32_t OpenXRRuntime::GetEyeWidth(uint32_t eye) const
    {
        return eye < m_eyeSwapchains.size() ? m_eyeSwapchains[eye].width : 0;
    }

    uint32_t OpenXRRuntime::GetEyeHeight(uint32_t eye) const
    {
        return eye < m_eyeSwapchains.size() ? m_eyeSwapchains[eye].height : 0;
    }

    const TouchControllerState& OpenXRRuntime::GetController(uint32_t hand) const
    {
        static const TouchControllerState empty{};
        return hand < m_controllers.size() ? m_controllers[hand] : empty;
    }

    UiPointer OpenXRRuntime::GetUiPointer() const
    {
        UiPointer pointer;
        uint32_t hand = 1;
        if (!m_controllers[hand].aimTracked)
            hand = 0;

        const TouchControllerState& controller = m_controllers[hand];
        if (!controller.aimTracked)
            return pointer;

        const XrVector3f direction = RotateForward(controller.aimPose.orientation);
        if (direction.z >= -0.0001f)
            return pointer;

        const float distance = (-UI_PANEL_DISTANCE_METERS - controller.aimPose.position.z) / direction.z;
        if (distance <= 0.0f)
            return pointer;

        const float hitX = controller.aimPose.position.x + direction.x * distance;
        const float hitY = controller.aimPose.position.y + direction.y * distance;
        const float normalizedX = hitX / UI_PANEL_WIDTH_METERS + 0.5f;
        const float normalizedY = 0.5f - hitY / UI_PANEL_HEIGHT_METERS;

        if (normalizedX < 0.0f || normalizedX > 1.0f ||
            normalizedY < 0.0f || normalizedY > 1.0f)
            return pointer;

        pointer.valid = true;
        pointer.x = normalizedX * UI_WIDTH;
        pointer.y = normalizedY * UI_HEIGHT;
        pointer.primaryDown = controller.triggerDown;
        return pointer;
    }

    void OpenXRRuntime::DestroyActions()
    {
        for (XrSpace& space : m_aimSpaces)
        {
            if (space != XR_NULL_HANDLE)
                xrDestroySpace(space);
            space = XR_NULL_HANDLE;
        }
        for (XrSpace& space : m_gripSpaces)
        {
            if (space != XR_NULL_HANDLE)
                xrDestroySpace(space);
            space = XR_NULL_HANDLE;
        }
        if (m_actionSet != XR_NULL_HANDLE)
            xrDestroyActionSet(m_actionSet);
        m_actionSet = XR_NULL_HANDLE;
        m_aimPoseAction = XR_NULL_HANDLE;
        m_gripPoseAction = XR_NULL_HANDLE;
        m_triggerAction = XR_NULL_HANDLE;
        m_squeezeAction = XR_NULL_HANDLE;
        m_primaryAction = XR_NULL_HANDLE;
        m_secondaryAction = XR_NULL_HANDLE;
        m_thumbstickAction = XR_NULL_HANDLE;
    }

    void OpenXRRuntime::Shutdown()
    {
        if (m_frameBegun)
            EndFrame(false);

        if (m_sessionRunning && m_session != XR_NULL_HANDLE)
            xrEndSession(m_session);
        m_sessionRunning = false;

        DestroyActions();

        for (auto& swapchain : m_eyeSwapchains)
        {
            if (swapchain.handle != XR_NULL_HANDLE)
                xrDestroySwapchain(swapchain.handle);
            swapchain = {};
        }

        if (m_viewSpace != XR_NULL_HANDLE)
            xrDestroySpace(m_viewSpace);
        m_viewSpace = XR_NULL_HANDLE;
        if (m_appSpace != XR_NULL_HANDLE)
            xrDestroySpace(m_appSpace);
        m_appSpace = XR_NULL_HANDLE;
        if (m_session != XR_NULL_HANDLE)
            xrDestroySession(m_session);
        m_session = XR_NULL_HANDLE;

        if (m_instance != XR_NULL_HANDLE)
            xrDestroyInstance(m_instance);
        m_instance = XR_NULL_HANDLE;
        m_systemId = XR_NULL_SYSTEM_ID;

        if (m_activityGlobalRef)
        {
            JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
            if (env)
                env->DeleteGlobalRef(static_cast<jobject>(m_activityGlobalRef));
        }
        m_activityGlobalRef = nullptr;
        m_javaVm = nullptr;

        // The VkInstance is intentionally left to process teardown: Plume's static
        // render objects are not yet exposed through a complete shutdown API.
        m_vkPhysicalDevice = VK_NULL_HANDLE;
        m_vkDevice = VK_NULL_HANDLE;
        m_initialized = false;
        m_extensionsReady = false;
        m_sessionState = XR_SESSION_STATE_UNKNOWN;
    }
}

#endif // UNLEASHED_RECOMP_OPENXR
