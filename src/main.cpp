/*
 * ============================================================
 *  Vulkan 三角形 - 完整教學範例
 *  每個步驟都有詳細的中文註解，對應學習計劃的各個階段。
 * ============================================================
 *
 *  Vulkan 物件建立順序（也是銷毀的反向順序）：
 *
 *  Instance → DebugMessenger → Surface → PhysicalDevice →
 *  LogicalDevice → Swapchain → ImageViews →
 *  RenderPass → PipelineLayout → GraphicsPipeline →
 *  Framebuffers → CommandPool → CommandBuffers →
 *  SyncObjects (Semaphores + Fences)
 */

#define GLFW_INCLUDE_VULKAN   // 讓 GLFW 自動 #include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <set>
#include <optional>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <limits>
#include <algorithm>

// ─────────────────────────────────────────────────────────────
//  常數設定
// ─────────────────────────────────────────────────────────────
const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;
const int      MAX_FRAMES_IN_FLIGHT = 2;  // 同時在 GPU 上處理的最大幀數

// 要啟用的 Validation Layers（Debug build 時才啟用）
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// 需要 GPU 支援的 Device Extensions
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,        // 用於呈現到螢幕
    "VK_KHR_portability_subset"             // macOS/MoltenVK 需要
};

// 在 Debug 模式下啟用 Validation Layers，Release 模式關閉
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

// ─────────────────────────────────────────────────────────────
//  Helper：讀取 SPIR-V 二進位檔案
// ─────────────────────────────────────────────────────────────
static std::vector<char> readFile(const std::string& filename) {
    // ate = 從檔案末尾開始，方便取得檔案大小
    // binary = 以二進位模式讀取，避免換行符被轉換
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

// ─────────────────────────────────────────────────────────────
//  Debug Callback（Validation Layer 的錯誤訊息會透過這裡輸出）
// ─────────────────────────────────────────────────────────────
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData)
{
    // 只印出 Warning 以上的訊息，過濾掉純資訊
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[Validation] " << pCallbackData->pMessage << "\n";
    }
    // 回傳 VK_FALSE 代表不要中斷呼叫（這是 Validation Layer callback 的慣例）
    return VK_FALSE;
}

// ─────────────────────────────────────────────────────────────
//  輔助結構：Queue Family 索引
//  不同的操作（繪圖、呈現）需要在不同的 Queue 上執行
// ─────────────────────────────────────────────────────────────
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;  // 支援 Graphics 的 Queue
    std::optional<uint32_t> presentFamily;   // 支援呈現到 Surface 的 Queue

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// ─────────────────────────────────────────────────────────────
//  輔助結構：Swapchain 支援細節
//  建立 Swapchain 前，先查詢 GPU 支援什麼格式和能力
// ─────────────────────────────────────────────────────────────
struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;   // 最大/最小 image 數量、解析度範圍
    std::vector<VkSurfaceFormatKHR> formats;        // 顏色格式（例如 BGRA8_SRGB）
    std::vector<VkPresentModeKHR>   presentModes;   // VSync 模式
};

// ─────────────────────────────────────────────────────────────
//  主程式類別
// ─────────────────────────────────────────────────────────────
class HelloTriangleApplication {
public:
    void run() {
        initWindow();    // Day 1：建立 GLFW 視窗
        initVulkan();    // Day 2-7：初始化所有 Vulkan 物件
        mainLoop();      // Day 7：主渲染迴圈
        cleanup();       // Day 7：清理所有資源
    }

private:
    // ── 視窗 ────────────────────────────────────────────────
    GLFWwindow* window = nullptr;

    // ── Day 2：Vulkan 核心物件 ───────────────────────────────
    VkInstance               instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice         physicalDevice = VK_NULL_HANDLE; // 不需要手動銷毀
    VkDevice                 device         = VK_NULL_HANDLE;
    VkQueue                  graphicsQueue  = VK_NULL_HANDLE; // 不需要手動銷毀
    VkQueue                  presentQueue   = VK_NULL_HANDLE; // 不需要手動銷毀

    // ── Day 3：視窗系統整合 ──────────────────────────────────
    VkSurfaceKHR             surface    = VK_NULL_HANDLE;
    VkSwapchainKHR           swapchain  = VK_NULL_HANDLE;
    std::vector<VkImage>     swapchainImages;     // Swapchain 管理，不需手動銷毀
    VkFormat                 swapchainImageFormat;
    VkExtent2D               swapchainExtent;
    std::vector<VkImageView> swapchainImageViews;

    // ── Day 4-5：渲染管線 ────────────────────────────────────
    VkRenderPass     renderPass     = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       graphicsPipeline = VK_NULL_HANDLE;

    // ── Day 6：Framebuffer & Command Buffer ──────────────────
    std::vector<VkFramebuffer>   swapchainFramebuffers;
    VkCommandPool                commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    // ── Day 7：同步物件 ──────────────────────────────────────
    // 每一幀需要自己的 Semaphore 和 Fence，避免幀之間互相干擾
    std::vector<VkSemaphore> imageAvailableSemaphores;  // Swapchain Image 準備好了
    std::vector<VkSemaphore> renderFinishedSemaphores;  // 渲染完成了
    std::vector<VkFence>     inFlightFences;            // CPU 等 GPU 完成上一幀
    uint32_t currentFrame = 0;  // 當前是第幾個 "in-flight frame"

    // ============================================================
    //  STEP 0：建立視窗
    //
    //  做什麼：用 GLFW 開一個視窗。
    //  為什麼：Vulkan 本身不管視窗，它只負責跟 GPU 溝通。
    //          GLFW 是跨平台的視窗函式庫，負責建立視窗、
    //          接收鍵盤滑鼠事件，並幫我們取得 Vulkan Surface。
    // ============================================================
    void initWindow() {
        glfwInit();
        // 設定 GLFW 錯誤 callback，方便診斷問題
        glfwSetErrorCallback([](int error, const char* desc) {
            std::cerr << "[GLFW Error " << error << "] " << desc << "\n";
        });
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // 不要建立 OpenGL context
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);    // 暫時禁止調整視窗大小
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Triangle", nullptr, nullptr);
        if (!window) throw std::runtime_error("Failed to create GLFW window");
    }

    // ============================================================
    //  初始化所有 Vulkan 物件（按照依賴順序）
    // ============================================================
    void initVulkan() {
        createInstance();           // Day 2
        setupDebugMessenger();      // Day 2
        createSurface();            // Day 3
        pickPhysicalDevice();       // Day 2
        createLogicalDevice();      // Day 2
        createSwapchain();          // Day 3
        createImageViews();         // Day 3
        createRenderPass();         // Day 4
        createGraphicsPipeline();   // Day 4-5
        createFramebuffers();       // Day 6
        createCommandPool();        // Day 6
        createCommandBuffers();     // Day 6
        createSyncObjects();        // Day 7
    }

    // ============================================================
    //  STEP 1：建立 VkInstance
    //
    //  做什麼：初始化 Vulkan，建立程式與 Vulkan 執行環境之間的連線。
    //  為什麼：就像 main() 是 C++ 的入口，Instance 是 Vulkan 的入口。
    //          沒有它就什麼都做不了。這裡同時告訴 Vulkan：
    //          「我要用哪些 Extensions」和「我要啟用 Validation Layer」。
    // ============================================================
    void createInstance() {
        // 先確認 Validation Layers 可用
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested but not available!");
        }

        // 應用程式基本資訊（可以幫助驅動做最佳化）
        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = "Vulkan Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "No Engine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // macOS (MoltenVK) 需要這個 flag
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

        // 需要哪些 Extensions（GLFW 幫我們查詢必要的）
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // 要啟用哪些 Layers
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            // 讓 Instance 建立/銷毀時也能有 Debug 訊息
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext             = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }
        std::cout << "✓ VkInstance created\n";
    }

    // 取得必要的 Instance Extensions
    std::vector<const char*> getRequiredExtensions() {
        uint32_t     glfwExtensionCount = 0;
        const char** glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            // Debug Messenger 需要這個 Extension
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        // macOS (MoltenVK) 需要這兩個
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        return extensions;
    }

    // 確認需要的 Validation Layer 是否可用
    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool found = false;
            for (const auto& prop : availableLayers) {
                if (strcmp(layerName, prop.layerName) == 0) { found = true; break; }
            }
            if (!found) return false;
        }
        return true;
    }

    // ============================================================
    //  STEP 2：設置 Debug Messenger
    //
    //  做什麼：設定一個 callback，讓 Validation Layer 的錯誤訊息
    //          印到 console。
    //  為什麼：Vulkan 的錯誤預設是靜默的。Validation Layer 能偵測
    //          到所有用法錯誤（如忘記銷毀物件、參數錯誤等），
    //          但需要這個 Messenger 才能把訊息傳給我們。
    //          開發期間必備，Release 時可以關掉（零效能開銷）。
    // ============================================================
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        populateDebugMessengerCreateInfo(createInfo);

        // vkCreateDebugUtilsMessengerEXT 是 Extension 函式，需要動態載入
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (!func || func(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to setup debug messenger!");
        }
        std::cout << "✓ Debug Messenger setup\n";
    }

    // ============================================================
    //  STEP 3：建立 Window Surface
    //
    //  做什麼：建立 Vulkan 與 OS 視窗之間的橋樑。
    //  為什麼：Vulkan 是平台無關的 API，它不知道你的視窗是
    //          macOS 的 NSWindow 還是 Windows 的 HWND。
    //          Surface 是一個抽象層，讓 Vulkan 可以把畫面輸出
    //          到任何平台的視窗。GLFW 幫我們隱藏了平台差異。
    //
    //  注意：Surface 需要在選 GPU 之前建立，因為我們要確認
    //        GPU 能夠輸出到這個 Surface（不是所有 GPU 都支援）。
    // ============================================================
    void createSurface() {
        VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
        if (result != VK_SUCCESS) {
            std::cerr << "glfwCreateWindowSurface returned: " << result << "\n";
            throw std::runtime_error("Failed to create window surface!");
        }
        std::cout << "\u2713 Window Surface created\n";
    }

    // ============================================================
    //  STEP 4：選擇 Physical Device（GPU）
    //
    //  做什麼：列出電腦上所有支援 Vulkan 的 GPU，選一張來用。
    //  為什麼：一台電腦可能有多張 GPU（例如 Intel 內顯 + Nvidia 獨顯）。
    //          Vulkan 不會自動幫你選，你要自己決定要用哪一張。
    //          這裡的「符合需求」是指：支援我們需要的 Queue Family、
    //          支援 Swapchain Extension、能輸出到我們的 Surface。
    //
    //  PhysicalDevice 代表真實的硬體，之後用 LogicalDevice 來操作它。
    // ============================================================
    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) throw std::runtime_error("No Vulkan-capable GPU found!");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& dev : devices) {
            if (isDeviceSuitable(dev)) {
                physicalDevice = dev;
                break;
            }
        }
        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("No suitable GPU found!");
        }

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        std::cout << "✓ Physical Device: " << props.deviceName << "\n";
    }

    // 判斷這張 GPU 是否符合我們的需求
    bool isDeviceSuitable(VkPhysicalDevice dev) {
        QueueFamilyIndices indices = findQueueFamilies(dev);
        bool extensionsSupported   = checkDeviceExtensionSupport(dev);
        bool swapchainAdequate     = false;

        if (extensionsSupported) {
            SwapchainSupportDetails details = querySwapchainSupport(dev);
            // 至少需要一個 format 和一個 present mode
            swapchainAdequate = !details.formats.empty() && !details.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapchainAdequate;
    }

    // 確認 GPU 支援我們需要的 Extensions
    bool checkDeviceExtensionSupport(VkPhysicalDevice dev) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> available(extensionCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, available.data());

        std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& ext : available) required.erase(ext.extensionName);
        return required.empty();
    }

    // 找到符合條件的 Queue Family 索引
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            // 找支援 Graphics 操作的 Queue
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            // 找支援呈現到我們的 Surface 的 Queue
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
            if (presentSupport) indices.presentFamily = i;

            if (indices.isComplete()) break;
        }
        return indices;
    }

    // ============================================================
    //  STEP 5：建立 Logical Device
    //
    //  做什麼：從選定的 GPU 建立一個「操作介面」，並取得 Queue。
    //  為什麼：PhysicalDevice 只是代表硬體的 handle，無法直接操作。
    //          LogicalDevice 才是你實際下指令的對象，相當於
    //          「與那張 GPU 建立連線，並宣告我要用它的哪些功能」。
    //
    //          Queue 是 GPU 的工作隊列。你把 Command Buffer 丟進去，
    //          GPU 就會依序執行。這裡我們需要兩種 Queue：
    //          - Graphics Queue：執行繪圖指令
    //          - Present Queue：把畫面送到螢幕
    //          （在大多數 GPU 上這兩個是同一個 Queue Family）
    // ============================================================
    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        // 每個 Queue Family 建立一個 Queue（去重複：若 graphics == present）
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };

        float queuePriority = 1.0f;  // Queue 的優先權（0.0 ~ 1.0）
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount       = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};  // 目前不需要任何特殊 Feature

        VkDeviceCreateInfo createInfo{};
        createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos       = queueCreateInfos.data();
        createInfo.pEnabledFeatures        = &deviceFeatures;
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }

        // 取得建立好的 Queue Handle
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(),  0, &presentQueue);
        std::cout << "✓ Logical Device created\n";
    }

    // ============================================================
    //  STEP 6：建立 Swapchain
    //
    //  做什麼：建立一組 Image 緩衝區，GPU 畫完一張就換下一張，
    //          畫好的再送到螢幕顯示（就是雙重/三重緩衝）。
    //  為什麼：GPU 渲染速度和螢幕刷新率不同步。
    //          如果直接把畫面寫進「正在顯示的 Image」，
    //          就會出現撕裂（Tearing）。Swapchain 讓 GPU 畫到
    //          背景 Image，畫完後再跟螢幕上的 Image 交換，
    //          確保使用者永遠看到完整的一幀。
    //
    //  這裡有三個選擇要做：
    //  - Format：像素的顏色格式（例如 BGRA8）
    //  - PresentMode：交換策略（VSync / 三重緩衝 / 立即）
    //  - Extent：解析度（跟視窗大小一致）
    // ============================================================
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice dev) {
        SwapchainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentModeCount,
                                                      details.presentModes.data());
        }
        return details;
    }

    // 選擇顏色格式：優先選 BGRA8_SRGB + SRGB_NONLINEAR（最常見的組合）
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return f;
            }
        }
        return formats[0];  // 退而求其次
    }

    // 選擇呈現模式：
    //   FIFO = 標準 VSync（保證可用）
    //   MAILBOX = 三重緩衝（效能更好但耗電）
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
        for (const auto& mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
        }
        return VK_PRESENT_MODE_FIFO_KHR;  // 保證可用的 fallback
    }

    // 選擇 Swap Extent（解析度）
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;  // 視窗已指定大小
        }
        // 高 DPI 螢幕（Retina）需要查詢實際像素數
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        VkExtent2D actual = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
        actual.width  = std::clamp(actual.width,
                                   capabilities.minImageExtent.width,
                                   capabilities.maxImageExtent.width);
        actual.height = std::clamp(actual.height,
                                   capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
        return actual;
    }

    void createSwapchain() {
        SwapchainSupportDetails details  = querySwapchainSupport(physicalDevice);
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(details.formats);
        VkPresentModeKHR   presentMode   = chooseSwapPresentMode(details.presentModes);
        VkExtent2D         extent        = chooseSwapExtent(details.capabilities);

        // Image 數量：最少數量 + 1（避免等待）
        uint32_t imageCount = details.capabilities.minImageCount + 1;
        if (details.capabilities.maxImageCount > 0)
            imageCount = std::min(imageCount, details.capabilities.maxImageCount);

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface          = surface;
        createInfo.minImageCount    = imageCount;
        createInfo.imageFormat      = surfaceFormat.format;
        createInfo.imageColorSpace  = surfaceFormat.colorSpace;
        createInfo.imageExtent      = extent;
        createInfo.imageArrayLayers = 1;  // 非 VR 用 1
        createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;  // 作為顏色輸出

        // 若 Graphics Queue 和 Present Queue 不同，需要設定共享模式
        QueueFamilyIndices indices  = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices   = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 效能較好
        }

        createInfo.preTransform   = details.capabilities.currentTransform;  // 不旋轉
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;     // 不透明視窗
        createInfo.presentMode    = presentMode;
        createInfo.clipped        = VK_TRUE;  // 被遮擋的像素不需要計算
        createInfo.oldSwapchain   = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain!");
        }

        // 取得 Swapchain 裡的 Image Handles
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

        swapchainImageFormat = surfaceFormat.format;
        swapchainExtent      = extent;
        std::cout << "✓ Swapchain created (" << imageCount << " images, "
                  << extent.width << "x" << extent.height << ")\n";
    }

    // ============================================================
    //  STEP 7：建立 Image Views
    //
    //  做什麼：為每個 Swapchain Image 建立一個 ImageView。
    //  為什麼：VkImage 只是 GPU 記憶體裡的原始資料。
    //          Vulkan 需要 ImageView 來描述「怎麼解讀這塊記憶體」：
    //          它是 2D 貼圖嗎？Cubemap？要用哪些 Mipmap 層級？
    //          是顏色還是深度資料？沒有 ImageView 就無法在
    //          Pipeline 或 Framebuffer 裡使用 Image。
    // ============================================================
    void createImageViews() {
        swapchainImageViews.resize(swapchainImages.size());

        for (size_t i = 0; i < swapchainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image    = swapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format   = swapchainImageFormat;

            // 顏色通道的對應（這裡保持原始對應）
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            // 這個 ImageView 的用途：顏色目標、單一 Mipmap、單一 Layer
            createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel   = 0;
            createInfo.subresourceRange.levelCount     = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount     = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image view!");
            }
        }
        std::cout << "✓ Image Views created\n";
    }

    // ============================================================
    //  STEP 8：建立 Render Pass
    //
    //  做什麼：描述「一次渲染過程」的結構——用哪些 Image 當輸出，
    //          渲染前要清空嗎？渲染後 Image 要變成什麼狀態？
    //  為什麼：Vulkan GPU 的記憶體系統很複雜，同一塊記憶體在不同
    //          用途下需要不同的「Layout」（內部排列方式）。
    //          例如：要寫入時用 COLOR_ATTACHMENT_OPTIMAL，
    //                要呈現到螢幕時用 PRESENT_SRC_KHR。
    //          Render Pass 讓 Vulkan 提前知道這些轉換，
    //          可以在驅動層做最佳化（尤其是 Tile-based GPU 如 M1）。
    //
    //  Attachment = 渲染輸出目標（這裡只有一個：顏色 Image）
    //  Subpass    = Render Pass 內的子階段（我們只有一個）
    // ============================================================
    void createRenderPass() {
        // Attachment 描述：我們唯一的 Attachment 是 Swapchain 的 Color Image
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = swapchainImageFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;   // 不做 MSAA
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;    // 渲染前清空
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;   // 渲染後保存
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;          // 渲染前不在意佈局
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;    // 渲染後轉成可呈現

        // Subpass 引用上面的 Attachment
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;  // 第 0 個 Attachment
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // 最佳顏色寫入佈局

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorAttachmentRef;

        // Subpass Dependency：確保 Swapchain Image 在我們寫入前已準備好
        VkSubpassDependency dependency{};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;  // 隱式的「前一個操作」
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments    = &colorAttachment;
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies   = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }
        std::cout << "✓ Render Pass created\n";
    }

    // ============================================================
    //  STEP 9：建立 Graphics Pipeline
    //
    //  做什麼：把「如何畫東西」的所有設定編譯成一個不可變的物件。
    //  為什麼：OpenGL 允許你在執行時隨時改 Pipeline 狀態（例如
    //          glEnable、glBlendFunc），但這讓驅動很難做最佳化。
    //          Vulkan 要求你把所有狀態一次說清楚，編譯成一個
    //          Pipeline 物件。建立很慢，但執行時切換非常快。
    //
    //  Pipeline 包含的設定：
    //  - Shader 程式（Vertex + Fragment）
    //  - 頂點格式（Vertex Input）
    //  - 拓樸結構（三角形 / 線段 / 點）
    //  - Viewport & Scissor（繪製區域）
    //  - Rasterizer（填充 / 線框、面剔除）
    //  - Multisampling（MSAA）
    //  - Color Blending（Alpha 透明混合）
    //  - Pipeline Layout（Shader 的 Uniform 變數介面）
    // ============================================================

    // 把 SPIR-V 二進位資料包裝成 VkShaderModule
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        // SPIR-V 需要 uint32_t* 對齊，所以要 reinterpret_cast
        createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }
        return shaderModule;
    }

    void createGraphicsPipeline() {
        // ── 載入 SPIR-V Shader（路徑相對於專案根目錄）────────────
        // __FILE__ = .../vulkan-sample/src/main.cpp，去掉 /src/main.cpp 得到根目錄
        std::string projectRoot = std::string(__FILE__);
        projectRoot = projectRoot.substr(0, projectRoot.rfind("/src/"));
        auto vertCode = readFile(projectRoot + "/shaders/shader.vert.spv");
        auto fragCode = readFile(projectRoot + "/shaders/shader.frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertCode);
        VkShaderModule fragShaderModule = createShaderModule(fragCode);

        // Shader Stage：把 Shader 模組綁定到 Pipeline 的哪個階段
        VkPipelineShaderStageCreateInfo vertStageInfo{};
        vertStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vertStageInfo.module = vertShaderModule;
        vertStageInfo.pName  = "main";  // Shader 的入口函式名稱

        VkPipelineShaderStageCreateInfo fragStageInfo{};
        fragStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStageInfo.module = fragShaderModule;
        fragStageInfo.pName  = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};

        // ── Vertex Input：頂點資料格式 ───────────────────────────
        // 我們的頂點直接寫在 Shader 裡，所以這裡是空的
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount   = 0;  // 沒有 Vertex Buffer
        vertexInputInfo.vertexAttributeDescriptionCount = 0;

        // ── Input Assembly：拓樸結構 ─────────────────────────────
        // 每 3 個頂點組成一個三角形
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // ── Viewport & Scissor：繪製區域 ─────────────────────────
        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(swapchainExtent.width);
        viewport.height   = static_cast<float>(swapchainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchainExtent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        // ── Rasterizer：三角形如何轉成像素 ───────────────────────
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable        = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;   // 填充（非線框）
        rasterizer.lineWidth               = 1.0f;
        rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;  // 剔除背面
        rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable         = VK_FALSE;

        // ── Multisampling：MSAA（目前關閉）────────────────────────
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable  = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // ── Color Blending：Alpha 混合（目前關閉）────────────────
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;  // 不做混合

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable     = VK_FALSE;
        colorBlending.attachmentCount   = 1;
        colorBlending.pAttachments      = &colorBlendAttachment;

        // ── Pipeline Layout：Shader 的 Uniform/Descriptor 設定 ───
        // 我們沒有任何 Descriptor，建立空的 Layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        // ── 組合所有設定，建立 Graphics Pipeline ─────────────────
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = 2;
        pipelineInfo.pStages             = shaderStages;
        pipelineInfo.pVertexInputState   = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = nullptr;  // 不使用深度測試
        pipelineInfo.pColorBlendState    = &colorBlending;
        pipelineInfo.pDynamicState       = nullptr;
        pipelineInfo.layout              = pipelineLayout;
        pipelineInfo.renderPass          = renderPass;
        pipelineInfo.subpass             = 0;  // 使用 Render Pass 的第 0 個 Subpass
        pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;  // 不繼承其他 Pipeline

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                      nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        // Shader Module 建立 Pipeline 後就可以銷毀了
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        std::cout << "✓ Graphics Pipeline created\n";
    }

    // ============================================================
    //  STEP 10：建立 Framebuffers
    //
    //  做什麼：把 Render Pass 的 Attachment 與實際的 ImageView 綁在一起。
    //  為什麼：Render Pass 只是「抽象描述」要用幾個 Attachment、
    //          格式是什麼。Framebuffer 才是「具體指定」這次渲染
    //          要輸出到哪個 ImageView（即 Swapchain 的哪張 Image）。
    //          因為 Swapchain 有 N 張 Image，我們需要 N 個 Framebuffer，
    //          每幀選對應的那個來用。
    // ============================================================
    void createFramebuffers() {
        swapchainFramebuffers.resize(swapchainImageViews.size());

        for (size_t i = 0; i < swapchainImageViews.size(); i++) {
            VkImageView attachments[] = { swapchainImageViews[i] };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass      = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments    = attachments;
            framebufferInfo.width           = swapchainExtent.width;
            framebufferInfo.height          = swapchainExtent.height;
            framebufferInfo.layers          = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                                    &swapchainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
        std::cout << "✓ Framebuffers created (" << swapchainFramebuffers.size() << ")\n";
    }

    // ============================================================
    //  STEP 11：建立 Command Pool 和 Command Buffers
    //
    //  做什麼：建立一個記憶體池（Command Pool），再從裡面分配
    //          Command Buffer，用來錄製每幀要執行的 GPU 指令。
    //  為什麼：你不能直接「呼叫」GPU 函式。Vulkan 的模型是：
    //          1. 先把指令「錄製」到 Command Buffer（CPU 端）
    //          2. 再把整個 Command Buffer「提交」到 Queue（送給 GPU）
    //          3. GPU 非同步地執行這些指令
    //
    //          這樣做的好處是：錄製可以多執行緒並行，
    //          提交是一次性操作，大幅減少 CPU 與 GPU 的溝通開銷。
    //
    //  Command Pool = 分配 Command Buffer 記憶體的池子
    //  Command Buffer = 裝 GPU 指令的容器（每幀一個，可重複錄製）
    // ============================================================
    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }
        std::cout << "✓ Command Pool created\n";
    }

    void createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = commandPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // 可直接提交到 Queue
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
        std::cout << "✓ Command Buffers allocated\n";
    }

    // 錄製 Command Buffer（每幀都會呼叫）
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // ONE_TIME_SUBMIT_BIT = 錄製一次執行一次（也可以 SIMULTANEOUS_USE）
        beginInfo.flags = 0;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        // 開始 Render Pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = renderPass;
        renderPassInfo.framebuffer       = swapchainFramebuffers[imageIndex];  // 這幀的 Framebuffer
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchainExtent;

        // Clear Color：渲染前把 Framebuffer 清成這個顏色（深藍色背景）
        VkClearValue clearColor = {{{0.05f, 0.05f, 0.1f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues    = &clearColor;

        // INLINE = Subpass 的指令直接嵌入（不使用 Secondary Command Buffer）
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // 綁定 Graphics Pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        // 畫三角形！
        // 參數：vertexCount=3, instanceCount=1, firstVertex=0, firstInstance=0
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

    // ============================================================
    //  STEP 12：建立同步物件
    //
    //  做什麼：建立 Semaphore 和 Fence，用來協調 CPU 和 GPU 的時序。
    //  為什麼：vkQueueSubmit 之後，GPU 是非同步跑的，CPU 不會等它。
    //          如果你馬上又提交下一幀，可能覆蓋到 GPU 還在用的資源。
    //          需要兩種同步工具：
    //
    //  Semaphore（GPU-GPU 同步）：
    //    - imageAvailableSemaphore：等 Swapchain 給我一張空的 Image
    //    - renderFinishedSemaphore：等渲染完成，再通知 Present Queue
    //
    //  Fence（GPU → CPU 同步）：
    //    - inFlightFence：CPU 等 GPU 跑完上一幀，才能重用 Command Buffer
    //
    //  關鍵差異：Semaphore 只有 GPU 能等，Fence 讓 CPU 也能等 GPU。
    // ============================================================
    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        // renderFinishedSemaphore 需要按 swapchain image 索引，不是按 frame-in-flight
        // 因為 swapchain 會 hold 住它，直到那張 image 被重新 acquire
        renderFinishedSemaphores.resize(swapchainImages.size());
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始狀態為已觸發（避免第一幀永遠等待）

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create sync objects!");
            }
        }
        for (size_t i = 0; i < swapchainImages.size(); i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create render finished semaphore!");
            }
        }
        std::cout << "✓ Sync objects created\n";
    }

    // ============================================================
    //  STEP 13：主渲染迴圈
    //
    //  做什麼：每幀重複執行同一套流程，直到使用者關閉視窗。
    //  為什麼：即使畫面沒有變化，GPU 也需要持續送出新的幀給螢幕。
    //
    //  每幀的 5 個步驟（見 drawFrame）：
    //    1. 等待 CPU：確保 GPU 已完成上一幀（Fence）
    //    2. 取 Image：從 Swapchain 拿一張空的 Image 來畫（Semaphore）
    //    3. 錄製指令：把「清空背景、綁 Pipeline、畫三角形」寫進 Command Buffer
    //    4. 提交 GPU：把 Command Buffer 送到 Graphics Queue
    //    5. 呈現畫面：GPU 畫完後，把 Image 送到螢幕（Present Queue）
    // ============================================================
    void mainLoop() {
        std::cout << "\n🔺 Rendering triangle... (Close window to exit)\n";
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        // 等待 GPU 完成所有工作才能安全清理
        vkDeviceWaitIdle(device);
    }

    void drawFrame() {
        // 1. 等待上一幀完成（確保 Command Buffer 可以重新錄製）
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        // 2. 從 Swapchain 取得下一個可用的 Image 索引
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                              imageAvailableSemaphores[currentFrame],  // 取得後觸發這個 Semaphore
                              VK_NULL_HANDLE, &imageIndex);

        // 3. 重置並錄製 Command Buffer
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        // 4. 提交 Command Buffer 到 Graphics Queue
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // 等待 imageAvailableSemaphore 觸發，才能執行 COLOR_ATTACHMENT_OUTPUT 階段
        VkSemaphore          waitSemaphores[]  = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[]      = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = waitSemaphores;
        submitInfo.pWaitDstStageMask    = waitStages;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &commandBuffers[currentFrame];

        // 完成後觸發 renderFinishedSemaphore（用 imageIndex 索引，不是 currentFrame）
        VkSemaphore signalSemaphores[]  = {renderFinishedSemaphores[imageIndex]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = signalSemaphores;

        // inFlightFences[currentFrame] 在 GPU 完成後觸發（讓 CPU 知道可以繼續）
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        // 5. 呈現（把渲染好的 Image 顯示到螢幕）
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = signalSemaphores;  // 等渲染完成

        VkSwapchainKHR swapchains[] = {swapchain};
        presentInfo.swapchainCount  = 1;
        presentInfo.pSwapchains     = swapchains;
        presentInfo.pImageIndices   = &imageIndex;

        vkQueuePresentKHR(presentQueue, &presentInfo);

        // 切換到下一幀
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // ============================================================
    //  STEP 14：清理所有資源
    //
    //  做什麼：按照建立順序的**相反**方向銷毀所有 Vulkan 物件。
    //  為什麼：Vulkan 物件之間有依賴關係。如果先銷毀父物件，
    //          子物件就會變成懸空指標，導致 crash 或記憶體錯誤。
    //          Validation Layer 會在這裡告訴你有沒有忘記銷毀的物件。
    //
    //  規則：vkCreate* 建立的就要 vkDestroy*，順序是 LIFO（後進先出）。
    // ============================================================
    void cleanup() {
        // 同步物件
        for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        }
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(device, commandPool, nullptr);

        for (auto framebuffer : swapchainFramebuffers)
            vkDestroyFramebuffer(device, framebuffer, nullptr);

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);

        for (auto imageView : swapchainImageViews)
            vkDestroyImageView(device, imageView, nullptr);

        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func) func(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
        std::cout << "✓ Cleanup complete\n";
    }
};

// ─────────────────────────────────────────────────────────────
//  程式入口
// ─────────────────────────────────────────────────────────────
int main() {
    HelloTriangleApplication app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
