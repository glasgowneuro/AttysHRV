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

static const char* defaultgreeting = "Connecting to Attys";

#define NUM_EYES 2

constexpr int SAMPLINGRATE = 250;
constexpr int NOTCH_CENTER = 50;

class OvrGeometry {
public:
    static constexpr int MAX_PROGRAM_UNIFORMS = 8;
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


struct OvrBackground : OvrGeometry {
    void CreateGeometry();
    virtual void draw();
    GLuint background_vao = 0;
};

struct OvrHRText : OvrGeometry {
    static constexpr int nPoints = 500;
    static constexpr float fontsize = 36;
    static constexpr float leadOffThreshold = 0.1; // 100mV
    struct AxesVertices {
        float positions[nPoints][3];
        float text2D[nPoints][2];
        unsigned char colors[nPoints][4];
    };
    GLuint texid = 0;
    AxesVertices axesVertices = {};
    unsigned short axesIndices[nPoints];
    void CreateGeometry();
    virtual void draw();
    void add_text(const char *text,
                  unsigned char r, unsigned char g, unsigned char b,
                  float x, float y,
                  bool centered = true);
    void updateHR(float hr);
    void attysDataCallBack(float);
    double lastHR = 0;
    enum Status {none, connecting, receiving, lefterr, righterr, running};
    Status status = none;
    void reportStatus(const Status s);
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

    static constexpr int iirorder = 2;

    Iir::Butterworth::BandStop<iirorder> iirnotch;
    Iir::Butterworth::HighPass<iirorder> iirhp;
    void attysDataCallBack(float);
};

struct OvrHRPlot : OvrGeometry {
    static constexpr int QUAD_GRID_SIZE = 200;
    static constexpr double minHRdiff = 10;
    const double spline_pred_sec = 1.5;
    const double maxtime = 30.0; // sec
    static constexpr int NR_VERTICES = (QUAD_GRID_SIZE+1)*(QUAD_GRID_SIZE+1);
    static constexpr int NR_TRIANGLES = 2*QUAD_GRID_SIZE*QUAD_GRID_SIZE;
    static constexpr int NR_INDICES = 3*NR_TRIANGLES;
    static constexpr double scale = 50.0f;
    static constexpr const double delta = 2.0/(double)QUAD_GRID_SIZE;

    struct HRVertices {
        float vertices[NR_VERTICES][3] = {};
        float normals[NR_VERTICES][3] = {};
    };

    struct DropAnim {
        double centerX = (QUAD_GRID_SIZE+1)/2;
        double centerY = (QUAD_GRID_SIZE+1)/2;
        double spatialFreq = 100;
        double temporalFreq = 1;
        float calcHeight(int x, int y, double t) {
            const double xc = (double) x - centerX;
            const double yc = (double) y - centerY;
            const double maxr = sqrt((QUAD_GRID_SIZE) * (QUAD_GRID_SIZE));
            const double r = sqrt(yc * yc + xc * xc) / maxr;
            return sin(r * spatialFreq + t * temporalFreq);
        }
    };

    HRVertices hrVertices = {};

    unsigned short indices[NR_INDICES] = {};

    void CreateGeometry();
    void draw();
    int frameCtr = 0;
    int fps = 0;
    std::chrono::time_point<std::chrono::steady_clock> start_fps_ts;
    double minHR = 1000;
    double maxHR = 0;
    std::vector<double> hrBuffer;
    std::vector<double> hrTs;
    cubic_spline hrSpline;
    void addHR(float hr);
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
    OvrHRText HrText;
    OvrBackground background;
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
