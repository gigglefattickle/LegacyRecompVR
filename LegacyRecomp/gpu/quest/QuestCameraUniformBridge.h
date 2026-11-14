#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <fstream>
#include <string>
#include <mutex>
#include <utility>

class QuestCameraUniformBridge {
public:
    enum class Eye : uint8_t { Left = 0, Right = 1 };
    using Matrix4 = std::array<float, 16>;
    struct GuestWrite { uint32_t firstFloat=0; uint32_t floatCount=0; uint64_t serial=0; };
    using GuestWriteObserver = std::function<void(const GuestWrite&, const uint32_t*, size_t)>;
    using DrawObserver = std::function<void(uintptr_t, Eye, const uint32_t*, size_t)>;
    void BeginFrame(uint64_t frameSerial);
    void SetEye(Eye eye);
    void ClearEye();
    void ObserveGuestWrite(uint32_t floatIndex, const void* source, size_t byteCount);
    bool ConfigureMatrixRegister(uint32_t registerIndex);
    void DisableMatrixOverride();
    void SetEyeMatrix(Eye eye, const Matrix4& matrix);
    void SetObservers(GuestWriteObserver writes, DrawObserver draws);
    std::string TracePath();
    void FlushTrace();
    class DrawScope {
    public:
        DrawScope()=default;
        DrawScope(QuestCameraUniformBridge*,uint32_t*,size_t,uintptr_t);
        DrawScope(const DrawScope&)=delete;
        DrawScope& operator=(const DrawScope&)=delete;
        DrawScope(DrawScope&&) noexcept;
        DrawScope& operator=(DrawScope&&) noexcept;
        ~DrawScope();
        explicit operator bool() const { return m_owner != nullptr; }
    private:
        void Restore();
        QuestCameraUniformBridge* m_owner=nullptr;
        uint32_t* m_constants=nullptr;
        std::array<uint32_t,16> m_original{};
        size_t m_floatCount=0;
        uint32_t m_firstFloat=0;
    };
    DrawScope BeginDraw(uintptr_t shaderKey,uint32_t* constants,size_t floatCount);
    bool HasEye() const { return m_eyeActive; }
    Eye ActiveEye() const { return m_eye; }
    bool HasMatrixMapping() const { return m_matrixRegisterConfigured; }
private:
    friend class DrawScope;
    void RestoreDraw(uint32_t*,size_t,uint32_t,const std::array<uint32_t,16>&);
    void EnsureTraceOpen();
    void TraceDraw(uintptr_t, const uint32_t*, size_t);
    std::mutex m_traceMutex;
    std::ofstream m_trace;
    std::string m_tracePath;
    std::array<bool,256> m_dirtyRegisters{};
    uint64_t m_traceEventCount=0;
    uint64_t m_frameSerial=0, m_writeSerial=0;
    Eye m_eye=Eye::Left;
    bool m_eyeActive=false, m_matrixRegisterConfigured=false;
    uint32_t m_matrixRegister=0;
    std::array<Matrix4,2> m_eyeMatrices{};
    GuestWriteObserver m_writeObserver;
    DrawObserver m_drawObserver;
};
