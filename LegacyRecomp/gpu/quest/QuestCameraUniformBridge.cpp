#include "QuestCameraUniformBridge.h"
#include <cassert>
#include <algorithm>
#include <utility>
#include <SDL.h>
#include <iomanip>

void QuestCameraUniformBridge::BeginFrame(uint64_t serial){m_frameSerial=serial;m_eyeActive=false;}
void QuestCameraUniformBridge::SetEye(Eye eye){m_eye=eye;m_eyeActive=true;}
void QuestCameraUniformBridge::ClearEye(){m_eyeActive=false;}
void QuestCameraUniformBridge::EnsureTraceOpen(){
    if(m_trace.is_open())return;
    char* base=SDL_GetPrefPath("LegacyRecomp","QuestVR");
    if(base){m_tracePath=std::string(base)+"quest-camera-trace.txt";SDL_free(base);}
    else m_tracePath="quest-camera-trace.txt";
    m_trace.open(m_tracePath,std::ios::out|std::ios::trunc);
    if(m_trace){m_trace<<"# LegacyRecomp Quest camera constant trace v1\n";m_trace.flush();SDL_Log("Quest camera trace: %s",m_tracePath.c_str());}
}
std::string QuestCameraUniformBridge::TracePath(){EnsureTraceOpen();return m_tracePath;}
void QuestCameraUniformBridge::ObserveGuestWrite(uint32_t index,const void* source,size_t bytes){
    if(!source||bytes==0||bytes%sizeof(uint32_t))return;
    const auto* words=static_cast<const uint32_t*>(source);
    const uint32_t count=static_cast<uint32_t>(bytes/sizeof(uint32_t));
    GuestWrite w{index,count,++m_writeSerial};
    if(m_writeObserver)m_writeObserver(w,words,w.floatCount);
    EnsureTraceOpen();
    if(m_trace&&m_traceEventCount<50000){
        m_trace<<"WRITE frame="<<m_frameSerial<<" seq="<<w.serial<<" first_float="<<index<<" count="<<count<<" data=";
        for(uint32_t i=0;i<count;++i)m_trace<<std::hex<<std::setw(8)<<std::setfill('0')<<words[i]<<",";
        m_trace<<std::dec<<"\n";
        ++m_traceEventCount;
    }
    if(count){
        const uint32_t first=index/4, last=std::min<uint32_t>(255,(index+count-1)/4);
        for(uint32_t r=first;r<=last&&r<256;++r)m_dirtyRegisters[r]=true;
    }
}
bool QuestCameraUniformBridge::ConfigureMatrixRegister(uint32_t reg){
    if(reg>=256)return false;m_matrixRegister=reg;m_matrixRegisterConfigured=true;return true;
}
void QuestCameraUniformBridge::DisableMatrixOverride(){m_matrixRegisterConfigured=false;}
void QuestCameraUniformBridge::SetEyeMatrix(Eye eye,const Matrix4& matrix){m_eyeMatrices[static_cast<size_t>(eye)]=matrix;}
void QuestCameraUniformBridge::SetObservers(GuestWriteObserver writes,DrawObserver draws){m_writeObserver=std::move(writes);m_drawObserver=std::move(draws);}
void QuestCameraUniformBridge::TraceDraw(uintptr_t shader,const uint32_t* constants,size_t count){
    bool any=false;for(bool dirty:m_dirtyRegisters)any|=dirty;
    if(!any)return;
    EnsureTraceOpen();if(!m_trace)return;
    if(m_traceEventCount>=50000){m_dirtyRegisters.fill(false);return;}
    m_trace<<"DRAW frame="<<m_frameSerial<<" shader=0x"<<std::hex<<shader<<std::dec<<" eye="<<(m_eyeActive?int(m_eye):-1)<<"\n";
    for(uint32_t r=0;r<256;++r)if(m_dirtyRegisters[r]){
        const uint32_t first=r*4;
        if(first+4<=count)m_trace<<"REG c"<<r<<" "<<std::hex<<std::setw(8)<<std::setfill('0')<<constants[first]<<" "<<std::setw(8)<<constants[first+1]<<" "<<std::setw(8)<<constants[first+2]<<" "<<std::setw(8)<<constants[first+3]<<std::dec<<"\n";
        m_dirtyRegisters[r]=false;
    }
    m_trace.flush();
}
QuestCameraUniformBridge::DrawScope QuestCameraUniformBridge::BeginDraw(uintptr_t shader,uint32_t* constants,size_t count){
    if(!constants)return {};
    TraceDraw(shader,constants,count);
    if(!m_eyeActive)return {};
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
