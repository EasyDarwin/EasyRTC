#include "easyrtc_encoder_gl.h"
#include "easyrtc_common.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/surface_texture.h>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <vector>

#define STB_EASY_FONT_IMPLEMENTATION
#include "third_party/stb_easy_font.h"
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

namespace {
static const char *kVs =
        "attribute vec2 aPos;\n"
        "attribute vec2 aTex;\n"
        "varying vec2 vTex;\n"
        "uniform mat4 uTexMat;\n"
        "void main(){\n"
        "  gl_Position=vec4(aPos,0.0,1.0);\n"
        "  vec4 t=uTexMat*vec4(aTex,0.0,1.0);\n"
        "  vTex=t.xy;\n"
        "}\n";

static const char *kFsOes =
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float;\n"
        "varying vec2 vTex;\n"
        "uniform samplerExternalOES uTex;\n"
        "void main(){ gl_FragColor=texture2D(uTex,vTex); }\n";

static const char *kFs2d =
        "precision mediump float;\n"
        "varying vec2 vTex;\n"
        "uniform sampler2D uTex;\n"
        "void main(){ gl_FragColor=texture2D(uTex,vTex); }\n";

// Overlay shader for timestamp text (no texture, per-vertex color)
static const char *kVsOverlay =
        "attribute vec2 aPos;\n"
        "attribute vec4 aColor;\n"
        "varying vec4 vColor;\n"
        "uniform vec2 uScale;\n"
        "uniform vec2 uOffset;\n"
        "void main(){\n"
        "  gl_Position=vec4(aPos*uScale+uOffset,0.0,1.0);\n"
        "  vColor=aColor;\n"
        "}\n";

static const char *kFsOverlay =
        "precision mediump float;\n"
        "varying vec4 vColor;\n"
        "void main(){ gl_FragColor=vColor; }\n";

GLuint compileShader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint linkProgram(const char *vsSrc, const char *fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, "aTex");
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

// Format: "05-01 08:11:20.324"
static void formatTimestamp(char *buf, size_t bufSize) {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto tt = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    struct tm tmBuf;
    localtime_r(&tt, &tmBuf);
    snprintf(buf, bufSize, "%02d-%02d %02d:%02d:%02d.%03lld",
             tmBuf.tm_mon + 1, tmBuf.tm_mday,
             tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
             static_cast<long long>(ms.count()));
}

bool initEglForEncoder(EncoderGlBridge* bridge) {
    if (!bridge || !bridge->encoderWindow) {
        return false;
    }

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("[CRITICAL] EncoderRotate EGL: eglGetDisplay failed");
        return false;
    }
    if (!eglInitialize(display, nullptr, nullptr)) {
        LOGE("[CRITICAL] EncoderRotate EGL: eglInitialize failed");
        return false;
    }

    const EGLint configAttrs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint numConfig = 0;
    if (!eglChooseConfig(display, configAttrs, &config, 1, &numConfig) || numConfig < 1) {
        LOGE("[CRITICAL] EncoderRotate EGL: eglChooseConfig failed");
        eglTerminate(display);
        return false;
    }

    EGLSurface surface = eglCreateWindowSurface(display, config, bridge->encoderWindow, nullptr);
    if (surface == EGL_NO_SURFACE) {
        LOGE("[CRITICAL] EncoderRotate EGL: eglCreateWindowSurface failed");
        eglTerminate(display);
        return false;
    }

    const EGLint ctxAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttrs);
    if (context == EGL_NO_CONTEXT) {
        LOGE("[CRITICAL] EncoderRotate EGL: eglCreateContext failed");
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGE("[CRITICAL] EncoderRotate EGL: eglMakeCurrent failed");
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bridge->width, bridge->height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("[CRITICAL] EncoderRotate EGL: FBO incomplete status=0x%x", status);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }

    bridge->eglDisplay = reinterpret_cast<void*>(display);
    bridge->eglContext = reinterpret_cast<void*>(context);
    bridge->eglSurface = reinterpret_cast<void*>(surface);
    bridge->colorTex = tex;
    bridge->fbo = fbo;

    GLuint oes = 0;
    glGenTextures(1, &oes);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, oes);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    bridge->cameraOesTex = oes;

    bridge->oesProgram = linkProgram(kVs, kFsOes);
    bridge->blitProgram = linkProgram(kVs, kFs2d);
    if (!bridge->oesProgram || !bridge->blitProgram) {
        LOGE("[CRITICAL] EncoderRotate EGL: shader link failed");
        return false;
    }
    bridge->oesPosLoc = 0;
    bridge->oesTexLoc = 1;
    bridge->oesMatLoc = glGetUniformLocation(bridge->oesProgram, "uTexMat");
    bridge->blitPosLoc = 0;
    bridge->blitTexLoc = 1;
    bridge->blitMatLoc = glGetUniformLocation(bridge->blitProgram, "uTexMat");

    // --- Overlay program for timestamp text ---
    bridge->overlayProgram = linkProgram(kVsOverlay, kFsOverlay);
    if (bridge->overlayProgram) {
        bridge->overlayPosLoc = glGetAttribLocation(bridge->overlayProgram, "aPos");
        bridge->overlayColorLoc = glGetAttribLocation(bridge->overlayProgram, "aColor");
        bridge->overlayScaleLoc = glGetUniformLocation(bridge->overlayProgram, "uScale");
        bridge->overlayOffsetLoc = glGetUniformLocation(bridge->overlayProgram, "uOffset");

        // Pre-allocate VBO and IBO with reasonable initial sizes
        glGenBuffers(1, &bridge->overlayVbo);
        glGenBuffers(1, &bridge->overlayIbo);
    }

    // Release current context from creator thread; render thread will own it.
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    return true;
}

void teardownEgl(EncoderGlBridge* bridge) {
    if (!bridge) return;
    EGLDisplay display = reinterpret_cast<EGLDisplay>(bridge->eglDisplay);
    EGLContext context = reinterpret_cast<EGLContext>(bridge->eglContext);
    EGLSurface surface = reinterpret_cast<EGLSurface>(bridge->eglSurface);

    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    if (bridge->fbo) {
        GLuint fbo = bridge->fbo;
        glDeleteFramebuffers(1, &fbo);
        bridge->fbo = 0;
    }
    if (bridge->colorTex) {
        GLuint tex = bridge->colorTex;
        glDeleteTextures(1, &tex);
        bridge->colorTex = 0;
    }
    if (bridge->cameraOesTex) {
        GLuint oes = bridge->cameraOesTex;
        glDeleteTextures(1, &oes);
        bridge->cameraOesTex = 0;
    }
    if (bridge->oesProgram) {
        glDeleteProgram(bridge->oesProgram);
        bridge->oesProgram = 0;
    }
    if (bridge->blitProgram) {
        glDeleteProgram(bridge->blitProgram);
        bridge->blitProgram = 0;
    }
    if (bridge->overlayProgram) {
        glDeleteProgram(bridge->overlayProgram);
        bridge->overlayProgram = 0;
    }
    if (bridge->overlayVbo) {
        GLuint vbo = bridge->overlayVbo;
        glDeleteBuffers(1, &vbo);
        bridge->overlayVbo = 0;
    }
    if (bridge->overlayIbo) {
        GLuint ibo = bridge->overlayIbo;
        glDeleteBuffers(1, &ibo);
        bridge->overlayIbo = 0;
    }
    if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
    }
    if (display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
    }
    if (display != EGL_NO_DISPLAY) {
        eglTerminate(display);
    }
    bridge->eglDisplay = nullptr;
    bridge->eglContext = nullptr;
    bridge->eglSurface = nullptr;
}
}

std::shared_ptr<EncoderGlBridge> encoderGlCreate(ANativeWindow* encoderWindow,
                                                  int width,
                                                  int height,
                                                  int rotation) {
    auto bridge = std::make_shared<EncoderGlBridge>();
    bridge->encoderWindow = encoderWindow;
    bridge->width = width;
    bridge->height = height;
    bridge->rotation = rotation;
    bridge->initialized = (encoderWindow != nullptr && width > 0 && height > 0) &&
                          initEglForEncoder(bridge.get());
    LOGI("[CRITICAL] EncoderRotate GL bridge create: window=%p size=%dx%d rot=%d init=%d",
         encoderWindow, width, height, rotation, bridge->initialized ? 1 : 0);
    LOGI("[CRITICAL] EncoderRotate GL bridge cameraOesTex=%u", bridge->cameraOesTex);
    return bridge;
}

bool encoderGlRenderTestFrame(const std::shared_ptr<EncoderGlBridge>& bridge) {
    if (!bridge || !bridge->initialized) {
        return false;
    }
    EGLDisplay display = reinterpret_cast<EGLDisplay>(bridge->eglDisplay);
    EGLSurface surface = reinterpret_cast<EGLSurface>(bridge->eglSurface);
    EGLContext context = reinterpret_cast<EGLContext>(bridge->eglContext);
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT) {
        return false;
    }
    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGE("[CRITICAL] EncoderRotate render: eglMakeCurrent failed");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, bridge->fbo);
    const float r = (bridge->rotation % 360 == 90) ? 0.0f : 0.15f;
    const float g = (bridge->rotation % 360 == 180) ? 0.0f : 0.35f;
    const float b = (bridge->rotation % 360 == 270) ? 0.0f : 0.55f;
    glViewport(0, 0, bridge->width, bridge->height);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, bridge->width, bridge->height);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    const EGLBoolean swapOk = eglSwapBuffers(display, surface);
    if (!swapOk) {
        LOGE("[CRITICAL] EncoderRotate render: eglSwapBuffers failed");
        return false;
    }
    return true;
}

bool encoderGlMakeCurrent(const std::shared_ptr<EncoderGlBridge>& bridge) {
    if (!bridge || !bridge->initialized) {
        return false;
    }
    EGLDisplay display = reinterpret_cast<EGLDisplay>(bridge->eglDisplay);
    EGLSurface surface = reinterpret_cast<EGLSurface>(bridge->eglSurface);
    EGLContext context = reinterpret_cast<EGLContext>(bridge->eglContext);
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT) {
        return false;
    }
    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGE("[CRITICAL] EncoderRotate render: eglMakeCurrent failed in makeCurrent");
        return false;
    }
    return true;
}

// Text scale factor applied to the NDC transform (1.0 = original size)
static const float kTextScale = 3.0f;

// Renders a line of text at pixel coordinates (x,y) into the current framebuffer.
// coord space: x right, y down, origin top-left of the encoded frame.
static void renderTextLine(const std::shared_ptr<EncoderGlBridge>& bridge,
                           const char *text, float pixelX, float pixelY,
                           const unsigned char color[4]) {
    if (!bridge || !bridge->overlayProgram || !bridge->overlayVbo || !bridge->overlayIbo || !text) {
        return;
    }

    // Estimate vertex buffer size (~270 bytes per char)
    const int maxQuads = 256;
    const int vbufBytes = maxQuads * 4 * 16; // 4 verts/quad, 16 bytes/vert
    std::vector<char> vbuf(vbufBytes);

    int quadCount = stb_easy_font_print(pixelX, pixelY,
                                        const_cast<char*>(text),
                                        const_cast<unsigned char*>(color),
                                        vbuf.data(), vbufBytes);
    if (quadCount <= 0) return;

    // Upload vertex data to VBO
    glBindBuffer(GL_ARRAY_BUFFER, bridge->overlayVbo);
    glBufferData(GL_ARRAY_BUFFER, quadCount * 4 * 16, vbuf.data(), GL_DYNAMIC_DRAW);

    // Build index buffer (convert quads to triangles: 0-1-2, 0-2-3)
    const int idxCount = quadCount * 6;
    std::vector<unsigned short> indices(idxCount);
    for (int q = 0; q < quadCount; ++q) {
        unsigned short base = static_cast<unsigned short>(q * 4);
        int i = q * 6;
        indices[i + 0] = base + 0;
        indices[i + 1] = base + 1;
        indices[i + 2] = base + 2;
        indices[i + 3] = base + 0;
        indices[i + 4] = base + 2;
        indices[i + 5] = base + 3;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bridge->overlayIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxCount * sizeof(unsigned short), indices.data(), GL_DYNAMIC_DRAW);

    // Setup and draw
    glUseProgram(bridge->overlayProgram);

    // Pixel → NDC transform (scaled for readability):
    const float scaleX = 2.0f / static_cast<float>(bridge->width) * kTextScale;
    const float scaleY = -2.0f / static_cast<float>(bridge->height) * kTextScale; // flip Y
    const float offsetX = -1.0f;
    const float offsetY = 1.0f;
    glUniform2f(bridge->overlayScaleLoc, scaleX, scaleY);
    glUniform2f(bridge->overlayOffsetLoc, offsetX, offsetY);

    glEnableVertexAttribArray(bridge->overlayPosLoc);
    glBindBuffer(GL_ARRAY_BUFFER, bridge->overlayVbo);
    glVertexAttribPointer(bridge->overlayPosLoc, 2, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void*>(0));

    glEnableVertexAttribArray(bridge->overlayColorLoc);
    glVertexAttribPointer(bridge->overlayColorLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, reinterpret_cast<void*>(12));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bridge->overlayIbo);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawElements(GL_TRIANGLES, idxCount, GL_UNSIGNED_SHORT, nullptr);

    glDisable(GL_BLEND);
    glDisableVertexAttribArray(bridge->overlayPosLoc);
    glDisableVertexAttribArray(bridge->overlayColorLoc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    bridge->overlayQuadCount = quadCount;
}

// Render timestamp at top-right
static void renderTimestampOverlay(const std::shared_ptr<EncoderGlBridge>& bridge) {
    char text[64];
    formatTimestamp(text, sizeof(text));

    // Compute text width for right-alignment.
    // Pixel coordinates are scaled by kTextScale in the shader, so the
    // visible pixel range is [0, width/kTextScale] not [0, width].
    int textW = stb_easy_font_width(text);
    const float pad = 12.0f;
    float pixelX = static_cast<float>(bridge->width) / kTextScale - static_cast<float>(textW) - pad / kTextScale;
    float pixelY = pad / kTextScale;
    if (pixelX < 0.0f) pixelX = 0.0f;

    unsigned char colorWhite[4] = {255, 255, 255, 255};
    renderTextLine(bridge, text, pixelX, pixelY, colorWhite);
}

// Render device ID (last 6 chars) at top-left
static void renderDeviceIdOverlay(const std::shared_ptr<EncoderGlBridge>& bridge) {
    if (bridge->deviceId.empty()) return;

    // Show only last 6 characters of the device ID
    std::string shortId = bridge->deviceId;
    if (shortId.size() > 6) {
        shortId = shortId.substr(shortId.size() - 6);
    }

    const float pad = 12.0f;
    unsigned char colorGreen[4] = {0, 255, 0, 255};
    renderTextLine(bridge, shortId.c_str(), pad / kTextScale, pad / kTextScale, colorGreen);
}

bool encoderGlRenderFrame(const std::shared_ptr<EncoderGlBridge>& bridge, long long timestampNs) {
    if (!bridge || !bridge->initialized) {
        return false;
    }
    EGLDisplay display = reinterpret_cast<EGLDisplay>(bridge->eglDisplay);
    EGLSurface surface = reinterpret_cast<EGLSurface>(bridge->eglSurface);
    EGLContext context = reinterpret_cast<EGLContext>(bridge->eglContext);
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT) {
        return false;
    }
    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGE("[CRITICAL] EncoderRotate render: eglMakeCurrent failed");
        return false;
    }

    const GLfloat verts[] = {-1.f,-1.f, 1.f,-1.f, -1.f,1.f, 1.f,1.f};
    const GLfloat tex[]   = { 0.f,1.f, 1.f,1.f,  0.f,0.f, 1.f,0.f};
    const GLfloat identity[16] = {
            1.f,0.f,0.f,0.f,
            0.f,1.f,0.f,0.f,
            0.f,0.f,1.f,0.f,
            0.f,0.f,0.f,1.f
    };

    glBindFramebuffer(GL_FRAMEBUFFER, bridge->fbo);
    glViewport(0, 0, bridge->width, bridge->height);
    glUseProgram(bridge->oesProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, bridge->cameraOesTex);
    glUniform1i(glGetUniformLocation(bridge->oesProgram, "uTex"), 0);
    glUniformMatrix4fv(bridge->oesMatLoc, 1, GL_FALSE, bridge->inputTransform);
    glVertexAttribPointer(bridge->oesPosLoc, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(bridge->oesPosLoc);
    glVertexAttribPointer(bridge->oesTexLoc, 2, GL_FLOAT, GL_FALSE, 0, tex);
    glEnableVertexAttribArray(bridge->oesTexLoc);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, bridge->width, bridge->height);
    glUseProgram(bridge->blitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bridge->colorTex);
    glUniform1i(glGetUniformLocation(bridge->blitProgram, "uTex"), 0);
    glUniformMatrix4fv(bridge->blitMatLoc, 1, GL_FALSE, identity);
    glVertexAttribPointer(bridge->blitPosLoc, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glEnableVertexAttribArray(bridge->blitPosLoc);
    glVertexAttribPointer(bridge->blitTexLoc, 2, GL_FLOAT, GL_FALSE, 0, tex);
    glEnableVertexAttribArray(bridge->blitTexLoc);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Draw timestamp overlay on top of the video frame
    renderTimestampOverlay(bridge);

    // Draw device ID overlay
    renderDeviceIdOverlay(bridge);

    if (!eglSwapBuffers(display, surface)) {
        LOGE("[CRITICAL] EncoderRotate render: eglSwapBuffers failed");
        return false;
    }
    if ((timestampNs % 120) == 0) {
        LOGD("EncoderRotate frame rendered ts=%lld", timestampNs);
    }
    return true;
}

bool encoderGlUpdateTexImage(ASurfaceTexture* st, float matrix4x4[16], int64_t* timestampNs) {
    if (!st) {
        return false;
    }
    int r = ASurfaceTexture_updateTexImage(st);
    if (r != 0) {
        LOGW("[CRITICAL] EncoderRotate updateTexImage failed: %d", r);
        return false;
    }
    if (matrix4x4) {
        ASurfaceTexture_getTransformMatrix(st, matrix4x4);
    }
    if (timestampNs) {
        *timestampNs = ASurfaceTexture_getTimestamp(st);
    }
    return true;
}

void encoderGlSetInputTransform(const std::shared_ptr<EncoderGlBridge>& bridge, const float* matrix4x4) {
    if (!bridge || !matrix4x4) {
        return;
    }
    for (int i = 0; i < 16; ++i) {
        bridge->inputTransform[i] = matrix4x4[i];
    }
}

void encoderGlRelease(std::shared_ptr<EncoderGlBridge>& bridge) {
    if (!bridge) {
        return;
    }
    LOGI("[CRITICAL] EncoderRotate GL bridge release: window=%p", bridge->encoderWindow);
    teardownEgl(bridge.get());
    bridge.reset();
}
