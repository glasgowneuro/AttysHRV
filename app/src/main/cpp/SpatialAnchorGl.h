#pragma once

#include <vector>

#include <GLES3/gl3.h>

#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID 1

#include <openxr/openxr.h>
#include <openxr/openxr_oculus.h>
#include <openxr/openxr_oculus_helpers.h>
#include <openxr/openxr_platform.h>
#include <chrono>

#include "OVR_Math.h"

#include "Iir.h"
#include "spline.hpp"

#define NUM_EYES 2

class OvrGeometry {
public:
    static constexpr int MAX_PROGRAM_UNIFORMS = 8;
    static constexpr int MAX_PROGRAM_TEXTURES = 8;
    static constexpr int MAX_VERTEX_ATTRIB_POINTERS = 3;

    OvrGeometry() {
        Clear();
    }

    void Clear();

    bool Create(const char *vertexSource, const char *fragmentSource);

    void Destroy();

    virtual void CreateGeometry() = 0;
    virtual void draw() = 0;

    GLuint Program;
    GLuint VertexShader;
    GLuint FragmentShader;
    // These will be -1 if not used by the program.
    GLint UniformLocation[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint UniformBinding[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint Textures[MAX_PROGRAM_TEXTURES]; // Texture%i

    void CreateVAO();
    void DestroyVAO();

    struct VertexAttribPointer {
        std::string Name;
        GLint Index;
        GLint Size;
        GLenum Type;
        GLboolean Normalized;
        GLsizei Stride;
        const GLvoid *Pointer;
    };

    GLuint VertexBuffer;
    GLuint IndexBuffer;
    GLuint VertexArrayObject;
    int VertexCount;
    int IndexCount;
    VertexAttribPointer VertexAttribs[MAX_VERTEX_ATTRIB_POINTERS];
    bool hasVAO = false;
};


struct OvrAxes : OvrGeometry {
    void CreateGeometry();
    virtual void draw();
};

struct OvrECGPlot : OvrGeometry {
    static constexpr int nPoints = 500;
    float offset = 0;

    struct ovrAxesVertices {
        float positions[nPoints][3];
        unsigned char colors[nPoints][4];
    };

    ovrAxesVertices axesVertices = {};

    unsigned short axesIndices[(nPoints*2)+1] = {};

    void CreateGeometry();
    void draw();
};

struct OvrHRPlot : OvrGeometry {
    static const int QUAD_GRID_SIZE = 200;
    static const int NR_VERTICES = (QUAD_GRID_SIZE+1)*(QUAD_GRID_SIZE+1);
    static const int NR_TRIANGLES = 2*QUAD_GRID_SIZE*QUAD_GRID_SIZE;
    static const int NR_INDICES = 3*NR_TRIANGLES;
    constexpr static const double scale = 50.0f;
    constexpr static const double delta = 2.0/(double)QUAD_GRID_SIZE;

    struct HRVertices {
        float vertices[NR_VERTICES][3] = {};
        float normals[NR_VERTICES][3] = {};
    };

    HRVertices hrVertices = {};

    Iir::RBJ::LowPass lp;

    unsigned short indices[NR_INDICES] = {};

    double hrShiftBuffer[QUAD_GRID_SIZE+1] = {};

    void CreateGeometry();
    void draw();
    int frameCtr = 0;
    int fps = 0;
    std::chrono::time_point<std::chrono::steady_clock> start_fps_ts;
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
    bool CreatedScene;
    GLuint SceneMatrices;
    OvrAxes Axes;
    OvrECGPlot ECGPlot;
    OvrHRPlot HrPlot;
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
    float t;
};

extern ovrScene* scenePtr;
