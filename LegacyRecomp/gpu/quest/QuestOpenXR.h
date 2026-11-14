#pragma once

#if defined(UNLEASHED_RECOMP_OPENXR)

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_VULKAN

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <volk.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace quest
{
    struct UiPointer
    {
        bool valid = false;
        bool primaryDown = false;
        float x = 0.0f;
        float y = 0.0f;
    };

    // Data handoff from the XR runtime to the Plume video layer for one acquired eye.
    struct EyeFrameData
    {
        VkImage image = VK_NULL_HANDLE;
        XrSwapchain swapchain = XR_NULL_HANDLE;
        XrView view{ XR_TYPE_VIEW };
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t imageIndex = 0;
    };

    struct TouchControllerState
    {
        bool aimTracked = false;
        bool gripTracked = false;
        bool triggerDown = false;
        bool squeezeDown = false;
        bool primaryDown = false;
        bool secondaryDown = false;
        XrVector2f thumbstick{ 0.0f, 0.0f };
        XrPosef aimPose{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } };
        XrPosef gripPose{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } };
    };

    class OpenXRRuntime
    {
    public:
        using DeviceCreateCallback =
            std::function<VkResult(VkPhysicalDevice, const VkDeviceCreateInfo*, VkDevice*)>;

        bool Initialize();
        bool CreateSession(VkDevice device, uint32_t graphicsQueueFamily, uint32_t graphicsQueueIndex);
        void Shutdown();

        void PollEvents();
        bool BeginFrame();
        void EndFrame(bool projectionLayerReady = false);

        VkInstance GetVulkanInstance() const { return m_vkInstance; }
        VkPhysicalDevice GetVulkanPhysicalDevice() const { return m_vkPhysicalDevice; }
        DeviceCreateCallback GetDeviceCreateCallback();

        bool IsSessionRunning() const { return m_sessionRunning; }
        bool IsFrameBegun() const { return m_frameBegun; }
        const XrView* GetViews() const { return m_views.data(); }
        uint32_t GetViewCount() const { return static_cast<uint32_t>(m_views.size()); }
        VkImage GetEyeImage(uint32_t eye) const;
        bool GetEyeFrameData(uint32_t eye, EyeFrameData& frameData) const;
        XrSwapchain GetEyeSwapchain(uint32_t eye) const;
        uint32_t GetEyeWidth(uint32_t eye) const;
        uint32_t GetEyeHeight(uint32_t eye) const;
        const TouchControllerState& GetController(uint32_t hand) const;
        UiPointer GetUiPointer() const;

        const std::string& GetLastError() const { return m_lastError; }

    private:
        struct EyeSwapchain
        {
            XrSwapchain handle = XR_NULL_HANDLE;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t imageIndex = 0;
            bool acquired = false;
            std::vector<XrSwapchainImageVulkan2KHR> images;
        };

        bool InitializeLoaderAndInstance();
        bool CreateVulkanInstance();
        bool CreateActions();
        bool CreateSpacesAndSwapchains();
        bool CreateEyeSwapchain(uint32_t eye, uint32_t width, uint32_t height);
        void DestroyActions();
        void UpdateActions(XrTime displayTime);
        bool LocateActionPose(XrAction action, XrPath subactionPath, XrSpace space, XrSpace baseSpace, XrTime time, XrPosef& pose);
        bool SetError(const char* operation, XrResult result);
        bool SetVkError(const char* operation, VkResult result);

        XrInstance m_instance = XR_NULL_HANDLE;
        XrSystemId m_systemId = XR_NULL_SYSTEM_ID;
        XrSession m_session = XR_NULL_HANDLE;
        XrSpace m_appSpace = XR_NULL_HANDLE;
        XrSpace m_viewSpace = XR_NULL_HANDLE;

        VkInstance m_vkInstance = VK_NULL_HANDLE;
        VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_vkDevice = VK_NULL_HANDLE;

        PFN_xrCreateVulkanInstanceKHR m_createVulkanInstance = nullptr;
        PFN_xrCreateVulkanDeviceKHR m_createVulkanDevice = nullptr;
        PFN_xrGetVulkanGraphicsDevice2KHR m_getVulkanGraphicsDevice = nullptr;
        PFN_xrGetVulkanGraphicsRequirements2KHR m_getVulkanGraphicsRequirements = nullptr;

        XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;
        XrFrameState m_frameState{ XR_TYPE_FRAME_STATE };
        XrViewState m_viewState{ XR_TYPE_VIEW_STATE };
        std::array<XrView, 2> m_views{
            XrView{ XR_TYPE_VIEW }, XrView{ XR_TYPE_VIEW }
        };
        std::array<EyeSwapchain, 2> m_eyeSwapchains;
        std::array<TouchControllerState, 2> m_controllers;

        XrActionSet m_actionSet = XR_NULL_HANDLE;
        XrAction m_aimPoseAction = XR_NULL_HANDLE;
        XrAction m_gripPoseAction = XR_NULL_HANDLE;
        XrAction m_triggerAction = XR_NULL_HANDLE;
        XrAction m_squeezeAction = XR_NULL_HANDLE;
        XrAction m_primaryAction = XR_NULL_HANDLE;
        XrAction m_secondaryAction = XR_NULL_HANDLE;
        XrAction m_thumbstickAction = XR_NULL_HANDLE;
        std::array<XrSpace, 2> m_aimSpaces{ XR_NULL_HANDLE, XR_NULL_HANDLE };
        std::array<XrSpace, 2> m_gripSpaces{ XR_NULL_HANDLE, XR_NULL_HANDLE };
        std::array<XrPath, 2> m_handPaths{ XR_NULL_PATH, XR_NULL_PATH };

        XrTime m_predictedDisplayTime = 0;
        bool m_sessionRunning = false;
        bool m_frameBegun = false;
        bool m_initialized = false;
        bool m_extensionsReady = false;
        void* m_javaVm = nullptr;
        void* m_activityGlobalRef = nullptr;
        std::string m_lastError;
    };

    OpenXRRuntime& GetOpenXRRuntime();
}

#endif // UNLEASHED_RECOMP_OPENXR
