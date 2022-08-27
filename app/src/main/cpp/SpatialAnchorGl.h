#pragma once

#include <vector>

#if defined(ANDROID)
#include <GLES3/gl3.h>

#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID 1
#elif defined(WIN32)
#include "Render/GlWrapperWin32.h"

#include <unknwn.h>
#define XR_USE_GRAPHICS_API_OPENGL 1
#define XR_USE_PLATFORM_WIN32 1
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_oculus.h>
#include <openxr/openxr_oculus_helpers.h>
#include <openxr/openxr_platform.h>

#include "OVR_Math.h"

#define NUM_EYES 2

struct OvrGeometry {
    OvrGeometry() {
        Clear();
    }
    virtual void Create() = 0;
    void Clear();
    void Destroy();
    void CreateVAO();
    void DestroyVAO();
    static constexpr int MAX_VERTEX_ATTRIB_POINTERS = 3;
    struct VertexAttribPointer {
        GLint Index;
        GLint Size;
        GLenum Type;
        GLboolean Normalized;
        GLsizei Stride;
        const GLvoid* Pointer;
    };
    GLuint VertexBuffer;
    GLuint IndexBuffer;
    GLuint VertexArrayObject;
    int VertexCount;
    int IndexCount;
    VertexAttribPointer VertexAttribs[MAX_VERTEX_ATTRIB_POINTERS];
};

struct OvrAxes : OvrGeometry {
    void Create();
};

struct OvrStage : OvrGeometry {
    void Create();
};

struct OvrECGPlot : OvrGeometry {
    void Create();
};

struct ovrProgram {
    static constexpr int MAX_PROGRAM_UNIFORMS = 8;
    static constexpr int MAX_PROGRAM_TEXTURES = 8;

    void Clear();
    bool Create(const char* vertexSource, const char* fragmentSource);
    void Destroy();
    GLuint Program;
    GLuint VertexShader;
    GLuint FragmentShader;
    // These will be -1 if not used by the program.
    GLint UniformLocation[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint UniformBinding[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint Textures[MAX_PROGRAM_TEXTURES]; // Texture%i
};

struct ovrFramebuffer {
    void Clear();
    bool Create(
        const GLenum colorFormat,
        const int width,
        const int height,
        const int multisamples,
        const int swapChainLength,
        const GLuint* colorTextures);
    void Destroy();
    void Bind(int element) const;
    static void Unbind();
    static void Resolve();
    int Width;
    int Height;
    int Multisamples;
    int SwapChainLength;
    struct Element {
        GLuint ColorTexture;
        GLuint DepthTexture;
        GLuint FrameBufferObject;
    };
    Element* Elements;
};

struct ovrScene {
    void Clear();
    void Create();
    void Destroy();
    bool IsCreated() const;
    void SetClearColor(const float* c);
    void CreateVAOs();
    void DestroyVAOs();
    bool CreatedScene;
    bool CreatedVAOs;
    GLuint SceneMatrices;
    ovrProgram StageProgram;
    OvrStage Stage;
    ovrProgram AxesProgram;
    OvrAxes Axes;
    ovrProgram ECGPlotProgram;
    float ClearColor[4];

    std::vector<XrSpace> SpaceList;
};

struct ovrAppRenderer {
    void Clear();
    void Create(
        GLenum format,
        int width,
        int height,
        int numMultiSamples,
        int swapChainLength,
        GLuint* colorTextures);
    void Destroy();

    struct FrameIn {
        int SwapChainIndex;
        OVR::Matrix4f View[NUM_EYES];
        OVR::Matrix4f Proj[NUM_EYES];
        bool HasStage;
        OVR::Posef StagePose;
        OVR::Vector3f StageScale;
    };

    void RenderFrame(FrameIn frameIn);

    ovrFramebuffer Framebuffer;
    ovrScene Scene;
};
