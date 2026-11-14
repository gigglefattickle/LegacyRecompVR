#include "QuestCameraUniformBridge.h"
#include <cassert>
#include <utility>

void QuestCameraUniformBridge::BeginFrame(uint64_t serial){m_frameSerial=serial;m_eyeActive=false;}
void QuestCameraUniformBridge::SetEye(Eye eye){m_eye=eye;m_eyeActive=true;}
void QuestCameraUniformBridge::ClearEye(){m_eyeActive=false;}
void QuestCameraUniformBridge::ObserveGuestWrite(uint32_t index,const void* source,size_t bytes){
    if(!source||bytes==0||bytes%sizeof(uint32_t))return;
    GuestWrite w{index,static_cast<uint32_t>(bytes/sizeof(uint32_t)),++m_writeSerial};
    if(m_writeObserver)m_writeObserver(w,static_cast<const uint32_t*>(source),w.floatCount);
}
bool QuestCameraUniformBridge::ConfigureMatrixRegister(uint32_t reg){
    if(reg>=256)return false;m_matrixRegister=reg;m_matrixRegisterConfigured=true;return true;
}
void QuestCameraUniformBridge::DisableMatrixOverride(){m_matrixRegisterConfigured=false;}
void QuestCameraUniformBridge::SetEyeMatrix(Eye eye,const Matrix4& matrix){m_eyeMatrices[static_cast<size_t>(eye)]=matrix;}
void QuestCameraUniformBridge::SetObservers(GuestWriteObserver writes,DrawObserver draws){m_writeObserver=std::move(writes);m_drawObserver=std::move(draws);}
QuestCameraUniformBridge::DrawScope QuestCameraUniformBridge::BeginDraw(uintptr_t shader,uint32_t* constants,size_t count){
    if(!constants||!m_eyeActive)return {};
    const uint32_t first=m_matrixRegister*4;
    if(m_matrixRegisterConfigured&&first+16<=count){
        DrawScope scope(this,constants,count,shader);
        std::memcpy(constants+first,m_eyeMatrices[static_cast<size_t>(m_eye)].data(),sizeof(Matrix4));
        if(m_drawObserver)m_drawObserver(shader,m_eye,constants,count);
        return scope;
    }
    if(m_drawObserver)m_drawObserver(shader,m_eye,constants,count);
    return {};
}
QuestCameraUniformBridge::DrawScope::DrawScope(QuestCameraUniformBridge* owner,uint32_t* c,size_t count,uintptr_t)
    :m_owner(owner),m_constants(c),m_floatCount(count),m_firstFloat(owner->m_matrixRegister*4){
    std::memcpy(m_original.data(),c+m_firstFloat,sizeof(m_original));
}
QuestCameraUniformBridge::DrawScope::DrawScope(DrawScope&& other) noexcept{*this=std::move(other);}
QuestCameraUniformBridge::DrawScope& QuestCameraUniformBridge::DrawScope::operator=(DrawScope&& other) noexcept{
    if(this!=&other){Restore();m_owner=std::exchange(other.m_owner,nullptr);m_constants=std::exchange(other.m_constants,nullptr);m_original=other.m_original;m_floatCount=std::exchange(other.m_floatCount,0);m_firstFloat=std::exchange(other.m_firstFloat,0);}return *this;
}
QuestCameraUniformBridge::DrawScope::~DrawScope(){Restore();}
void QuestCameraUniformBridge::DrawScope::Restore(){if(m_owner){m_owner->RestoreDraw(m_constants,m_floatCount,m_firstFloat,m_original);m_owner=nullptr;m_constants=nullptr;}}
void QuestCameraUniformBridge::RestoreDraw(uint32_t* c,size_t count,uint32_t first,const std::array<uint32_t,16>& original){
    assert(c&&first+original.size()<=count);std::memcpy(c+first,original.data(),sizeof(original));
}
