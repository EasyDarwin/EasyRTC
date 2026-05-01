#ifndef EASYRTC_ENCODER_GL_H
#define EASYRTC_ENCODER_GL_H

#include <android/native_window.h>
#include <android/surface_texture.h>
#include <memory>
#include <string>

struct EncoderGlBridge {
    bool initialized = false;
    int rotation = 0;
    int width = 0;
    int height = 0;
    ANativeWindow* encoderWindow = nullptr;
    void* eglDisplay = nullptr;
    void* eglContext = nullptr;
    void* eglSurface = nullptr;
    unsigned int fbo = 0;
    unsigned int colorTex = 0;
    unsigned int cameraOesTex = 0;
    unsigned int oesProgram = 0;
    int oesPosLoc = -1;
    int oesTexLoc = -1;
    int oesMatLoc = -1;
    unsigned int blitProgram = 0;
    int blitPosLoc = -1;
    int blitTexLoc = -1;
    int blitMatLoc = -1;
    float inputTransform[16] = {
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f
    };

    // Text overlay
    unsigned int overlayProgram = 0;
    int overlayPosLoc = -1;
    int overlayColorLoc = -1;
    int overlayScaleLoc = -1;
    int overlayOffsetLoc = -1;
    unsigned int overlayVbo = 0;
    unsigned int overlayIbo = 0;
    int overlayQuadCount = 0;

    // Device ID for top-left overlay
    std::string deviceId;
};

std::shared_ptr<EncoderGlBridge> encoderGlCreate(ANativeWindow* encoderWindow,
                                                 int width,
                                                 int height,
                                                 int rotation);
bool encoderGlRenderTestFrame(const std::shared_ptr<EncoderGlBridge>& bridge);
bool encoderGlRenderFrame(const std::shared_ptr<EncoderGlBridge>& bridge, long long timestampNs);
bool encoderGlUpdateTexImage(ASurfaceTexture* st, float matrix4x4[16], int64_t* timestampNs);
void encoderGlSetInputTransform(const std::shared_ptr<EncoderGlBridge>& bridge, const float* matrix4x4);
bool encoderGlMakeCurrent(const std::shared_ptr<EncoderGlBridge>& bridge);
void encoderGlRelease(std::shared_ptr<EncoderGlBridge>& bridge);

#endif
