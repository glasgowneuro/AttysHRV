/************************************************************************************

Filename	:	SpatialAnchor.cpp
Content		:	This sample is derived from VrCubeWorld_SurfaceView.
                When used in room scale mode, it draws a "carpet" under the
                user to indicate where it is safe to walk around.
Created		:	July, 2020
Authors		:	Cass Everitt

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <ctime>

#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h> // for native window JNI
#include <android/input.h>

#include <atomic>
#include <thread>

#include <sys/system_properties.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include "SpatialAnchorGl.h"

#include "util.h"

using namespace OVR;

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

#ifndef GL_FRAMEBUFFER_SRGB_EXT
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9
#endif

#if !defined(GL_EXT_multisampled_render_to_texture)
typedef void(GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples);
#endif

#if !defined(GL_OVR_multiview)
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint baseViewIndex,
    GLsizei numViews);
#endif

#if !defined(GL_OVR_multiview_multisampled_render_to_texture)
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLsizei samples,
    GLint baseViewIndex,
    GLsizei numViews);
#endif

#define DEBUG 1

#define OVR_LOG_TAG "OculusECG"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

namespace {
struct OpenGLExtensions_t {
    bool multi_view; // GL_OVR_multiview, GL_OVR_multiview2
    bool EXT_texture_border_clamp; // GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
    bool EXT_sRGB_write_control;
};

OpenGLExtensions_t glExtensions;
} // namespace

static void EglInitExtensions() {
    glExtensions = {};
    const char* allExtensions = (const char*)glGetString(GL_EXTENSIONS);
    if (allExtensions != nullptr) {
        glExtensions.multi_view = strstr(allExtensions, "GL_OVR_multiview2") &&
            strstr(allExtensions, "GL_OVR_multiview_multisampled_render_to_texture");

        glExtensions.EXT_texture_border_clamp =
            strstr(allExtensions, "GL_EXT_texture_border_clamp") ||
            strstr(allExtensions, "GL_OES_texture_border_clamp");
        glExtensions.EXT_sRGB_write_control = strstr(allExtensions, "GL_EXT_sRGB_write_control");
    }
}

static const char* GlFrameBufferStatusString(GLenum status) {
    switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            return "GL_FRAMEBUFFER_UNDEFINED";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_UNSUPPORTED:
            return "GL_FRAMEBUFFER_UNSUPPORTED";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
        default:
            return "unknown";
    }
}

#ifdef CHECK_GL_ERRORS

static const char* GlErrorString(GLenum error) {
    switch (error) {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
        default:
            return "unknown";
    }
}

static void GLCheckErrors(int line) {
    for (int i = 0; i < 10; i++) {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            break;
        }
        ALOGE("GL error on line %d: %s", line, GlErrorString(error));
    }
}

#define GL(func) \
    func;        \
    GLCheckErrors(__LINE__);

#else // CHECK_GL_ERRORS

#define GL(func) func;

#endif // CHECK_GL_ERRORS

/*
================================================================================

OvrGeometry

================================================================================
*/

static std::vector<double> dataBuffer;
static std::vector<float> hrBuffer;

void OvrAxes::CreateGeometry() {
    struct ovrAxesVertices {
        float positions[6][3];
        unsigned char colors[6][4];
    };

    static const ovrAxesVertices axesVertices = {
        // positions
        {{0, 0, 0}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {0, 0, 0}, {0, 0, 1}},
        // colors
        {{255, 0, 0, 255},
         {255, 0, 0, 255},
         {0, 255, 0, 255},
         {0, 255, 0, 255},
         {0, 0, 255, 255},
         {0, 0, 255, 255}},
    };

    static const unsigned short axesIndices[6] = {
        0,
        1, // x axis - red
        2,
        3, // y axis - green
        4,
        5 // z axis - blue
    };

    VertexCount = 6;
    IndexCount = 6;

    VertexAttribs[0].Index = 0;
    VertexAttribs[1].Name = "vertexPosition";
    VertexAttribs[0].Size = 3;
    VertexAttribs[0].Type = GL_FLOAT;
    VertexAttribs[0].Normalized = false;
    VertexAttribs[0].Stride = sizeof(axesVertices.positions[0]);
    VertexAttribs[0].Pointer = (const GLvoid*)offsetof(ovrAxesVertices, positions);

    VertexAttribs[1].Index = 1;
    VertexAttribs[1].Name = "vertexColor";
    VertexAttribs[1].Size = 4;
    VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
    VertexAttribs[1].Normalized = true;
    VertexAttribs[1].Stride = sizeof(axesVertices.colors[0]);
    VertexAttribs[1].Pointer = (const GLvoid*)offsetof(ovrAxesVertices, colors);

    GL(glGenBuffers(1, &VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(axesVertices), &axesVertices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(axesIndices), axesIndices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    CreateVAO();
}

void OvrAxes::draw() {
    GL(glBindVertexArray(VertexArrayObject));
    GL(glDrawElements(GL_LINES, IndexCount, GL_UNSIGNED_SHORT, nullptr));
    GL(glBindVertexArray(0));
}

void OvrECGPlot::CreateGeometry() {
    ALOGV("OvrECGPlot::Create()");
    VertexCount = nPoints;
    IndexCount = (nPoints*2)+1;

    ALOGE("Creating ECG plot with %d vertices.",VertexCount);
    for(int i = 0; i < nPoints; i++) {
        axesIndices[i*2] = i;
        axesIndices[i*2+1 ] = i+1;

        axesVertices.colors[i][0] = 0;
        axesVertices.colors[i][1] = 255;
        axesVertices.colors[i][2] = 255;
        axesVertices.colors[i][3] = 255;

        axesVertices.positions[i][0] = -2 + (float)i / (float)nPoints * 4.0f;
        axesVertices.positions[i][1] = (float)sin(i/10.0) * 0.1f;
        // ALOGV("pos = %f,%f", axesVertices.positions[i][0],axesVertices.positions[i][1]);
        axesVertices.positions[i][2] = 0;
    }

    VertexAttribs[0].Index = 0;
    VertexAttribs[0].Name = "vertexPosition";
    VertexAttribs[0].Size = 3;
    VertexAttribs[0].Type = GL_FLOAT;
    VertexAttribs[0].Normalized = false;
    VertexAttribs[0].Stride = sizeof(axesVertices.positions[0]);
    VertexAttribs[0].Pointer = (const GLvoid*)offsetof(ovrAxesVertices, positions);

    VertexAttribs[1].Index = 1;
    VertexAttribs[1].Name = "vertexColor";
    VertexAttribs[1].Size = 4;
    VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
    VertexAttribs[1].Normalized = true;
    VertexAttribs[1].Stride = sizeof(axesVertices.colors[0]);
    VertexAttribs[1].Pointer = (const GLvoid*)offsetof(ovrAxesVertices, colors);

    GL(glGenBuffers(1, &VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(axesVertices), &axesVertices, GL_STREAM_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(axesIndices), axesIndices, GL_STREAM_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    CreateVAO();
}

void OvrECGPlot::draw() {

    for(auto &v:dataBuffer) {
        for (int i = 0; i < (nPoints-1); i++) {
            axesVertices.positions[i][1] = axesVertices.positions[i + 1][1];
        }
        axesVertices.positions[nPoints-1][1] = (float)v;
        // ALOGV("OvrECGPlot::draw, buffersz=%u, %f", (unsigned int) dataBuffer.size(),v);
    }
    dataBuffer.clear();

#ifdef FAKE_DATA
    for(int i = 0; i < nPoints; i++) {
        axesVertices.positions[i][0] = -1 + (float) i / (float) nPoints * 2.0f;
        axesVertices.positions[i][1] = (float) sin(i / 10.0 + offset) * 0.1;
    }
    offset += 0.1;
#endif

    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(axesVertices), &axesVertices, GL_STREAM_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glBindVertexArray(VertexArrayObject));
    GL(glDrawElements(GL_LINES, IndexCount, GL_UNSIGNED_SHORT, nullptr));
    GL(glBindVertexArray(0));
}

void OvrHRPlot::CreateGeometry() {
    VertexCount = NR_VERTICES;
    IndexCount = NR_INDICES;

    for (auto &v: hrShiftBuffer) {
        v = 0;
    }

    for (int y=0; y<=QUAD_GRID_SIZE; y++) {
        for (int x=0; x<=QUAD_GRID_SIZE; x++) {
            int vertexPosition = y*(QUAD_GRID_SIZE+1) + x;
            hrVertices.vertices[vertexPosition][0] = ( (float)x*delta - 1.0f ) * scale ;
            hrVertices.vertices[vertexPosition][1]= 0;
            hrVertices.vertices[vertexPosition][2] = ( (float)y*delta - 1.0f ) * scale;
            hrVertices.normals[vertexPosition][0] = 0;
            hrVertices.normals[vertexPosition][1] = 1;
            hrVertices.normals[vertexPosition][2] = 0;
        }
    }

    // Generate indices into vertex list
    for (int y=0; y<QUAD_GRID_SIZE; y++) {
        for (int x=0; x<QUAD_GRID_SIZE; x++) {
            int indexPosition = y*QUAD_GRID_SIZE + x;
            // tri 0
            indices[6*indexPosition  ] = y    *(QUAD_GRID_SIZE+1) + x;    //bl
            indices[6*indexPosition+1] = (y+1)*(QUAD_GRID_SIZE+1) + x + 1;//tr
            indices[6*indexPosition+2] = y    *(QUAD_GRID_SIZE+1) + x + 1;//br
            // tri 1
            indices[6*indexPosition+3] = y    *(QUAD_GRID_SIZE+1) + x;    //bl
            indices[6*indexPosition+4] = (y+1)*(QUAD_GRID_SIZE+1) + x;    //tl
            indices[6*indexPosition+5] = (y+1)*(QUAD_GRID_SIZE+1) + x + 1;//tr
        }
    }

    VertexAttribs[0].Index = 0;
    VertexAttribs[0].Name = "vertexPosition";
    VertexAttribs[0].Size = 3;
    VertexAttribs[0].Type = GL_FLOAT;
    VertexAttribs[0].Normalized = false;
    VertexAttribs[0].Stride = 3 * sizeof(float);
    VertexAttribs[0].Pointer = (const GLvoid*) offsetof(HRVertices,vertices);

    VertexAttribs[1].Index = 1;
    VertexAttribs[1].Name = "vertexNormal";
    VertexAttribs[1].Size = 3;
    VertexAttribs[1].Type = GL_FLOAT;
    VertexAttribs[1].Normalized = false;
    VertexAttribs[1].Stride = 3 * sizeof(float);
    VertexAttribs[1].Pointer = (const GLvoid*) offsetof(HRVertices,normals);

    GL(glGenBuffers(1, &VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(HRVertices), &hrVertices, GL_STREAM_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STREAM_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    CreateVAO();
}

void OvrHRPlot::draw() {
    if (!(hrBuffer.empty())) {

        if (hrShiftBuffer[0] == 0) {
            for (auto &v: hrShiftBuffer) {
                v = hrBuffer[0];
            }
        }

        for (auto &v: hrBuffer) {
            for (int i = 0; i < QUAD_GRID_SIZE; i++) {
                hrShiftBuffer[i] = hrShiftBuffer[i + 1];
            }
            hrShiftBuffer[QUAD_GRID_SIZE] = v;
        }

        hrBuffer.clear();

        float min = 1000;
        float max = 0;
        for (auto &v: hrShiftBuffer) {
            if (v < min) min = v;
            if (v > max) max = v;
        }
        float n = max - min;
        //ALOGV("before: min = %f, max = %f, norm = %f", min, max, n);
        if (n < 10) {
            n = 10;
        }
        //ALOGV("after: min = %f, max = %f, norm = %f", min, max, n);
        for (int x = 0; x <= QUAD_GRID_SIZE; x++) {
            for (int y = 0; y <= QUAD_GRID_SIZE; y++) {
                int vertexPosition = y * (QUAD_GRID_SIZE + 1) + x;
                int xc = x - (QUAD_GRID_SIZE / 2);
                int yc = y - (QUAD_GRID_SIZE / 2);
                int r = QUAD_GRID_SIZE - (int)sqrt(yc*yc + xc*xc);
                if (r < 0) r = 0;
                hrVertices.vertices[vertexPosition][1] = (hrShiftBuffer[r] - min) / n * 5;
            }
        }
    }

#ifdef FAKE_DATA
    for (int x=0; x<=QUAD_GRID_SIZE; x++) {
        float t = offset;
        for (int y=0; y<=QUAD_GRID_SIZE; y++) {
            int vertexPosition = y*(QUAD_GRID_SIZE+1) + x;
            hrVertices.vertices[vertexPosition][1]= sin(t)*1;
            t = t + 0.1f;
        }
    }
    offset += 0.1;
#endif

    for (int x=0; x<QUAD_GRID_SIZE; x++) {
        for (int y=0; y<QUAD_GRID_SIZE; y++) {
            int vertexPosition1 = y*(QUAD_GRID_SIZE+1) + x;
            int vertexPosition2 = (y+1)*(QUAD_GRID_SIZE+1) + x;
            int vertexPosition3 = y*(QUAD_GRID_SIZE+1) + x + 1;
            float a[3];
            float b[3];
            diff(hrVertices.vertices[vertexPosition1],hrVertices.vertices[vertexPosition2],a);
            diff(hrVertices.vertices[vertexPosition1],hrVertices.vertices[vertexPosition3],b);
            crossProduct(a,
                         b,
                         hrVertices.normals[vertexPosition1]);
/*
            if (x == 0) {
                ALOGV("cross prod = %d,%f,%f,%f",
                      y,
                      hrVertices.normals[vertexPosition1][0],
                      hrVertices.normals[vertexPosition1][1],
                      hrVertices.normals[vertexPosition1][2]);
            }
*/
        }
    }

    GL(glDepthMask(GL_FALSE));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glDisable(GL_CULL_FACE));
    GL(glEnable(GL_BLEND));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(HRVertices), &hrVertices, GL_STREAM_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glBindVertexArray(VertexArrayObject));
    GL(glDrawElements(GL_TRIANGLES, IndexCount, GL_UNSIGNED_SHORT, nullptr));
    GL(glBindVertexArray(0));

    GL(glDepthMask(GL_TRUE));
    GL(glDisable(GL_BLEND));
}


//////////////////////////////////////////////////////////////////////////////////////////


void OvrGeometry::Clear() {
    VertexBuffer = 0;
    IndexBuffer = 0;
    VertexArrayObject = 0;
    VertexCount = 0;
    IndexCount = 0;
    for (int i = 0; i < MAX_VERTEX_ATTRIB_POINTERS; i++) {
        memset(&VertexAttribs[i], 0, sizeof(VertexAttribs[i]));
        VertexAttribs[i].Index = -1;
    }

    Program = 0;
    VertexShader = 0;
    FragmentShader = 0;
    memset(UniformLocation, 0, sizeof(UniformLocation));
    memset(UniformBinding, 0, sizeof(UniformBinding));
    memset(Textures, 0, sizeof(Textures));
}

void OvrGeometry::CreateVAO() {
    GL(glGenVertexArrays(1, &VertexArrayObject));
    GL(glBindVertexArray(VertexArrayObject));

    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));

    for (auto & VertexAttrib : VertexAttribs) {
        if (VertexAttrib.Index != -1) {
            GL(glEnableVertexAttribArray(VertexAttrib.Index));
            GL(glVertexAttribPointer(
                VertexAttrib.Index,
                VertexAttrib.Size,
                VertexAttrib.Type,
                VertexAttrib.Normalized,
                VertexAttrib.Stride,
                VertexAttrib.Pointer));
        }
    }

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBindVertexArray(0));
    hasVAO = true;
}

void OvrGeometry::DestroyVAO() {
    if (hasVAO) {
        GL(glDeleteVertexArrays(1, &VertexArrayObject));
        hasVAO = false;
    }
}

struct ovrUniform {
    enum Index {
        MODEL_MATRIX,
        VIEW_ID,
        SCENE_MATRICES,
        COLOR_SCALE,
        COLOR_BIAS,
        TIME_S,
    };
    enum Type {
        VECTOR4,
        MATRIX4X4,
        INTEGER,
        BUFFER,
        FLOAT,
    };

    Index index;
    Type type;
    const char* name;
};

static ovrUniform ProgramUniforms[] = {
    {ovrUniform::Index::MODEL_MATRIX, ovrUniform::Type::MATRIX4X4, "ModelMatrix"},
    {ovrUniform::Index::VIEW_ID, ovrUniform::Type::INTEGER, "ViewID"},
    {ovrUniform::Index::SCENE_MATRICES, ovrUniform::Type::BUFFER, "SceneMatrices"},
    {ovrUniform::Index::COLOR_SCALE, ovrUniform::Type::VECTOR4, "ColorScale"},
    {ovrUniform::Index::COLOR_BIAS, ovrUniform::Type::VECTOR4, "ColorBias"},
    {ovrUniform::Index::TIME_S, ovrUniform::Type::FLOAT, "time"},
};

static const char* programVersion = "#version 300 es\n";

bool OvrGeometry::Create(const char* vertexSource, const char* fragmentSource) {
    GLint r;

    GL(VertexShader = glCreateShader(GL_VERTEX_SHADER));

    const char* vertexSources[3] = {programVersion, "", vertexSource};
    GL(glShaderSource(VertexShader, 3, vertexSources, 0));
    GL(glCompileShader(VertexShader));
    GL(glGetShaderiv(VertexShader, GL_COMPILE_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetShaderInfoLog(VertexShader, sizeof(msg), 0, msg));
        ALOGE("vertex shader compile failed");
        ALOGE("%s\n%s\n", vertexSource, msg);
        return false;
    }

    const char* fragmentSources[2] = {programVersion, fragmentSource};
    GL(FragmentShader = glCreateShader(GL_FRAGMENT_SHADER));
    GL(glShaderSource(FragmentShader, 2, fragmentSources, 0));
    GL(glCompileShader(FragmentShader));
    GL(glGetShaderiv(FragmentShader, GL_COMPILE_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetShaderInfoLog(FragmentShader, sizeof(msg), 0, msg));
        ALOGE("fragment shader compile failed");
        ALOGE("%s\n%s\n", fragmentSource, msg);
        return false;
    }

    GL(Program = glCreateProgram());
    GL(glAttachShader(Program, VertexShader));
    GL(glAttachShader(Program, FragmentShader));

    GL(glLinkProgram(Program));
    GL(glGetProgramiv(Program, GL_LINK_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetProgramInfoLog(Program, sizeof(msg), 0, msg));
        ALOGE("Linking program failed: %s\n", msg);
        return false;
    }

    int numBufferBindings = 0;

    memset(UniformLocation, -1, sizeof(UniformLocation));
    for (size_t i = 0; i < sizeof(ProgramUniforms) / sizeof(ProgramUniforms[0]); i++) {
        const int uniformIndex = ProgramUniforms[i].index;
        if (ProgramUniforms[i].type == ovrUniform::Type::BUFFER) {
            GL(UniformLocation[uniformIndex] =
                   glGetUniformBlockIndex(Program, ProgramUniforms[i].name));
            UniformBinding[uniformIndex] = numBufferBindings++;
            GL(glUniformBlockBinding(
                Program, UniformLocation[uniformIndex], UniformBinding[uniformIndex]));
        } else {
            GL(UniformLocation[uniformIndex] =
                   glGetUniformLocation(Program, ProgramUniforms[i].name));
            UniformBinding[uniformIndex] = UniformLocation[uniformIndex];
        }
    }

    GL(glUseProgram(Program));

    // Get the texture locations.
    for (int i = 0; i < MAX_PROGRAM_TEXTURES; i++) {
        char name[32];
        sprintf(name, "Texture%i", i);
        Textures[i] = glGetUniformLocation(Program, name);
        if (Textures[i] != -1) {
            GL(glUniform1i(Textures[i], i));
        }
    }

    CreateGeometry();

    // Bind the vertex attribute locations.
    for (auto &a:VertexAttribs) {
        GL(glBindAttribLocation(Program, a.Index, a.Name.c_str()));
    }

    GL(glUseProgram(0));

    return true;
}

void OvrGeometry::Destroy() {
    DestroyVAO();

    GL(glDeleteBuffers(1, &IndexBuffer));
    GL(glDeleteBuffers(1, &VertexBuffer));

    Clear();

    if (Program != 0) {
        GL(glDeleteProgram(Program));
        Program = 0;
    }
    if (VertexShader != 0) {
        GL(glDeleteShader(VertexShader));
        VertexShader = 0;
    }
    if (FragmentShader != 0) {
        GL(glDeleteShader(FragmentShader));
        FragmentShader = 0;
    }
}

static const char STAGE_VERTEX_SHADER[] =
    "#define NUM_VIEWS 2\n"
    "#define VIEW_ID gl_ViewID_OVR\n"
    "#extension GL_OVR_multiview2 : require\n"
    "layout(num_views=NUM_VIEWS) in;\n"
    "in vec3 vertexPosition;\n"
    "uniform mat4 ModelMatrix;\n"
    "uniform SceneMatrices\n"
    "{\n"
    "	uniform mat4 ViewMatrix[NUM_VIEWS];\n"
    "	uniform mat4 ProjectionMatrix[NUM_VIEWS];\n"
    "} sm;\n"
    "void main()\n"
    "{\n"
    "	gl_Position = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * ( vec4( vertexPosition, 1.0 ) ) ) );\n"
    "}\n";

static const char STAGE_FRAGMENT_SHADER[] =
    "out lowp vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "	outColor = vec4( 0.5, 0.5, 1.0, 0.5 );\n"
    "}\n";

static const char AXES_VERTEX_SHADER[] =
    "#define NUM_VIEWS 2\n"
    "#define VIEW_ID gl_ViewID_OVR\n"
    "#extension GL_OVR_multiview2 : require\n"
    "layout(num_views=NUM_VIEWS) in;\n"
    "in vec3 vertexPosition;\n"
    "in vec4 vertexColor;\n"
    "uniform mat4 ModelMatrix;\n"
    "uniform SceneMatrices\n"
    "{\n"
    "	uniform mat4 ViewMatrix[NUM_VIEWS];\n"
    "	uniform mat4 ProjectionMatrix[NUM_VIEWS];\n"
    "} sm;\n"
    "out vec4 fragmentColor;\n"
    "void main()\n"
    "{\n"
    "	gl_Position = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * ( vec4( vertexPosition, 1.0 ) ) ) );\n"
    "	fragmentColor = vertexColor;\n"
    "}\n";

static const char AXES_FRAGMENT_SHADER[] =
    "in lowp vec4 fragmentColor;\n"
    "out lowp vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "	outColor = fragmentColor;\n"
    "}\n";


/*
================================================================================

ovrFramebuffer

================================================================================
*/

void ovrFramebuffer::Clear() {
    Width = 0;
    Height = 0;
    Multisamples = 0;
    SwapChainLength = 0;
    Elements = nullptr;
}

static void* GlGetExtensionProc(const char* functionName) {
    return (void*)eglGetProcAddress(functionName);
}

bool ovrFramebuffer::Create(
    const GLenum colorFormat,
    const int width,
    const int height,
    const int multisamples,
    const int swapChainLength,
    const GLuint* colorTextures) {
    auto glFramebufferTextureMultiviewOVR =
        (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)GlGetExtensionProc(
            "glFramebufferTextureMultiviewOVR");
    auto glFramebufferTextureMultisampleMultiviewOVR =
        (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)GlGetExtensionProc(
            "glFramebufferTextureMultisampleMultiviewOVR");

    Width = width;
    Height = height;
    Multisamples = multisamples;
    SwapChainLength = swapChainLength;

    Elements = new Element[SwapChainLength];

    for (int i = 0; i < SwapChainLength; i++) {
        Element& el = Elements[i];
        // Create the color buffer texture.
        el.ColorTexture = colorTextures[i];
        GLenum colorTextureTarget = GL_TEXTURE_2D_ARRAY;
        GL(glBindTexture(colorTextureTarget, el.ColorTexture));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
        GLfloat borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        GL(glTexParameterfv(colorTextureTarget, GL_TEXTURE_BORDER_COLOR, borderColor));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL(glBindTexture(colorTextureTarget, 0));

        // Create the depth buffer texture.
        GL(glGenTextures(1, &el.DepthTexture));
        GL(glBindTexture(GL_TEXTURE_2D_ARRAY, el.DepthTexture));
        GL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT24, width, height, 2));
        GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));

        // Create the frame buffer.
        GL(glGenFramebuffers(1, &el.FrameBufferObject));
        GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, el.FrameBufferObject));
        if (multisamples > 1 && (glFramebufferTextureMultisampleMultiviewOVR != nullptr)) {
            GL(glFramebufferTextureMultisampleMultiviewOVR(
                GL_DRAW_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                el.DepthTexture,
                0 /* level */,
                multisamples /* samples */,
                0 /* baseViewIndex */,
                2 /* numViews */));
            GL(glFramebufferTextureMultisampleMultiviewOVR(
                GL_DRAW_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                el.ColorTexture,
                0 /* level */,
                multisamples /* samples */,
                0 /* baseViewIndex */,
                2 /* numViews */));
        } else if (glFramebufferTextureMultiviewOVR) {
            GL(glFramebufferTextureMultiviewOVR(
                GL_DRAW_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                el.DepthTexture,
                0 /* level */,
                0 /* baseViewIndex */,
                2 /* numViews */));
            GL(glFramebufferTextureMultiviewOVR(
                GL_DRAW_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                el.ColorTexture,
                0 /* level */,
                0 /* baseViewIndex */,
                2 /* numViews */));
        }

        GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
        GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
        if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
            ALOGE(
                "Incomplete frame buffer object: %s",
                GlFrameBufferStatusString(renderFramebufferStatus));
            return false;
        }
    }

    return true;
}

void ovrFramebuffer::Destroy() {
    for (int i = 0; i < SwapChainLength; i++) {
        Element& el = Elements[i];
        GL(glDeleteFramebuffers(1, &el.FrameBufferObject));
        GL(glDeleteTextures(1, &el.DepthTexture));
    }
    delete[] Elements;
    Clear();
}

void ovrFramebuffer::Bind(int element) const {
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, Elements[element].FrameBufferObject));
}

void ovrFramebuffer::Unbind() {
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
}

void ovrFramebuffer::Resolve() {
    // Discard the depth buffer, so the tiler won't need to write it back out to memory.
    const GLenum depthAttachment[1] = {GL_DEPTH_ATTACHMENT};
    glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, depthAttachment);
    // We now let the resolve happen implicitly.
}

/*
================================================================================

ovrScene

================================================================================
*/

void ovrScene::SetClearColor(const float* c) {
    for (int i = 0; i < 4; i++) {
        ClearColor[i] = c[i];
    }
}

void ovrScene::Clear() {
    CreatedScene = false;
    SceneMatrices = 0;

    Axes.Clear();
    ECGPlot.Clear();
    HrPlot.Clear();
}

bool ovrScene::IsCreated() const {
    return CreatedScene;
}

void ovrScene::Create() {
    // Setup the scene matrices.
    GL(glGenBuffers(1, &SceneMatrices));
    GL(glBindBuffer(GL_UNIFORM_BUFFER, SceneMatrices));
    GL(glBufferData(
            GL_UNIFORM_BUFFER,
            2 * sizeof(Matrix4f) /* 2 view matrices */ +
            2 * sizeof(Matrix4f) /* 2 projection matrices */,
            nullptr,
            GL_STATIC_DRAW));
    GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));
    GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));

    // Axes
    if (!Axes.Create(AXES_VERTEX_SHADER, AXES_FRAGMENT_SHADER)) {
        ALOGE("Failed to compile axes program");
    }

    // ECG
    if (!ECGPlot.Create(AXES_VERTEX_SHADER, AXES_FRAGMENT_SHADER)) {
        ALOGE("Failed to compile plot program");
    }

    const char HRPLOT_VERTEX_SHADER[] =
            "#define NUM_VIEWS 2\n"
            "#define VIEW_ID gl_ViewID_OVR\n"
            "#extension GL_OVR_multiview2 : require\n"
            "layout(num_views=NUM_VIEWS) in;\n"
            "in vec3 vertexPosition;\n"
            "in vec3 vertexNormal;\n"
            "out vec3 normal;\n"
            "out vec3 fragPos;\n"
            "out vec4 modelView;\n"
            "uniform mat4 ModelMatrix;\n"
            "uniform SceneMatrices\n"
            "{\n"
            "	uniform mat4 ViewMatrix[NUM_VIEWS];\n"
            "	uniform mat4 ProjectionMatrix[NUM_VIEWS];\n"
            "} sm;\n"
            "void main()\n"
            "{\n"
            "	modelView = sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * ( vec4( vertexPosition, 1.0 ) ) );\n"
            "	gl_Position = sm.ProjectionMatrix[VIEW_ID] * modelView;\n"
            "   normal = normalize(vertexNormal);\n"
            "   fragPos = vertexPosition;\n"
            "}\n";

    const char HRPLOT_FRAGMENT_SHADER[] =
            "in vec3 normal;\n"
            "in vec3 fragPos;\n"
            "in vec4 modelView;\n"
            "out lowp vec4 outColor;\n"
            "const float pi = 3.14159;\n"
            "uniform float time;\n"
            "float wave(float x, float y, float t, float speed, vec2 direction) {\n"
            "   float theta = dot(direction, vec2(x, y));\n"
            "   return (sin(theta * pi + t * speed) + 2.0) / 3.0;\n"
            "}\n"
            "void main()\n"
            "{\n"
            "   vec3 lightPos = vec3(-10.0, 30.0, -10.0);\n"
            "   vec3 lightDir = normalize(lightPos - fragPos);\n"
            "   float diffuse = max(dot(normal, lightDir), 0.0);\n"
            "   float v1 = wave(fragPos.x, fragPos.z, time, 5.0, vec2(0.5,0.25));\n"
            "   vec4 texColor1 = vec4( 0.0, v1, v1, 1.0);\n"
            "   float v2 = wave(fragPos.x, fragPos.z, time, -4.0, vec2(0.5,-0.25));\n"
            "   vec4 texColor2 = vec4( 0.0, v2, v2, 1.0);\n"
            "   float v3 = wave(fragPos.x, fragPos.z, time, -0.7, vec2(0.03,0.07));\n"
            "   vec4 texColor3 = vec4( 0.0, v3, v3, 1.0);\n"
            "   float v4 = wave(fragPos.x, fragPos.z, time, 0.51, vec2(0.03,-0.03));\n"
            "   vec4 texColor4 = vec4( 0.0, v4, v3, 1.0);\n"
            "   vec4 texColor = mix(mix(texColor1,texColor2,0.5),mix(texColor3,texColor4,0.5),0.9);\n"
            "   float trans = abs( dot( normalize(modelView.xyz), normalize(vec3(1,0,1)) ) );\n"
            "   vec4 diffuseColour = vec4( 0.0, diffuse, diffuse, trans );\n"
            "	outColor = mix(texColor,diffuseColour,0.5);\n"
            "}\n";

    // HRPlot
    if (!HrPlot.Create(HRPLOT_VERTEX_SHADER, HRPLOT_FRAGMENT_SHADER)) {
        ALOGE("Failed to compile HRPlot program");
    }

    CreatedScene = true;

    float c[] = {0.0, 0.0, 0.0, 0.0};
    SetClearColor(c);
}

void ovrScene::Destroy() {
    GL(glDeleteBuffers(1, &SceneMatrices));
    Axes.Destroy();
    ECGPlot.Destroy();
    HrPlot.Destroy();
    CreatedScene = false;
}

/*
================================================================================

ovrAppRenderer

================================================================================
*/

void ovrAppRenderer::Clear() {
    Framebuffer.Clear();
    Scene.Clear();
}

void ovrAppRenderer::Create(
    GLenum format,
    int width,
    int height,
    int numMultiSamples,
    int swapChainLength,
    GLuint* colorTextures) {
    EglInitExtensions();
    Framebuffer.Create(format, width, height, numMultiSamples, swapChainLength, colorTextures);
    if (glExtensions.EXT_sRGB_write_control) {
        // This app was originally written with the presumption that
        // its swapchains and compositor front buffer were RGB.
        // In order to have the colors the same now that its compositing
        // to an sRGB front buffer, we have to write to an sRGB swapchain
        // but with the linear->sRGB conversion disabled on write.
        GL(glDisable(GL_FRAMEBUFFER_SRGB_EXT));
    }
    start_ts = std::chrono::steady_clock::now();
}

void ovrAppRenderer::Destroy() {
    Framebuffer.Destroy();
}

void ovrAppRenderer::RenderFrame(ovrAppRenderer::FrameIn frameIn) {
    // Update the scene matrices.
    GL(glBindBuffer(GL_UNIFORM_BUFFER, Scene.SceneMatrices));
    GL(Matrix4f* sceneMatrices = (Matrix4f*)glMapBufferRange(
           GL_UNIFORM_BUFFER,
           0,
           4 * sizeof(Matrix4f) /* 2 view + 2 proj matrices */,
           GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

    if (sceneMatrices != nullptr) {
        memcpy((char*)sceneMatrices, &frameIn.View, 4 * sizeof(Matrix4f));
    }

    GL(glUnmapBuffer(GL_UNIFORM_BUFFER));
    GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));

    // Render the eye images.
    Framebuffer.Bind(frameIn.SwapChainIndex);

    GL(glEnable(GL_SCISSOR_TEST));
    GL(glDepthMask(GL_TRUE));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glEnable(GL_CULL_FACE));
    GL(glCullFace(GL_BACK));
    GL(glDisable(GL_BLEND));
    GL(glViewport(0, 0, Framebuffer.Width, Framebuffer.Height));
    GL(glScissor(0, 0, Framebuffer.Width, Framebuffer.Height));
    GL(glClearColor(
        Scene.ClearColor[0], Scene.ClearColor[1], Scene.ClearColor[2], Scene.ClearColor[3]));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    GL(glLineWidth(3.0));
    // "tracking space" axes (could be LOCAL or LOCAL_FLOOR)
    GL(glUseProgram(Scene.Axes.Program));
    GL(glBindBufferBase(
        GL_UNIFORM_BUFFER,
        Scene.Axes.UniformBinding[ovrUniform::Index::SCENE_MATRICES],
        Scene.SceneMatrices));
    if (Scene.Axes.UniformLocation[ovrUniform::Index::VIEW_ID] >=
        0) // NOTE: will not be present when multiview path is enabled.
    {
        GL(glUniform1i(Scene.Axes.UniformLocation[ovrUniform::Index::VIEW_ID], 0));
    }
    if (Scene.Axes.UniformLocation[ovrUniform::Index::MODEL_MATRIX] >= 0) {
        const Matrix4f scale = Matrix4f::Scaling(0.1, 0.1, 0.1);
        const Matrix4f stagePoseMat = Matrix4f::Translation(0,-1,-2);
        const Matrix4f m1 = stagePoseMat  * scale;
        GL(glUniformMatrix4fv(
            Scene.Axes.UniformLocation[ovrUniform::Index::MODEL_MATRIX],
            1,
            GL_TRUE,
            &m1.M[0][0]));
    }
    Scene.Axes.draw();
    GL(glUseProgram(0));

    // ECG Plot
    GL(glUseProgram(Scene.ECGPlot.Program));
    GL(glBindBufferBase(
            GL_UNIFORM_BUFFER,
            Scene.ECGPlot.UniformBinding[ovrUniform::Index::SCENE_MATRICES],
            Scene.SceneMatrices));
    if (Scene.ECGPlot.UniformLocation[ovrUniform::Index::VIEW_ID] >=
        0) // NOTE: will not be present when multiview path is enabled.
    {
        GL(glUniform1i(Scene.ECGPlot.UniformLocation[ovrUniform::Index::VIEW_ID], 0));
    }
    if (Scene.ECGPlot.UniformLocation[ovrUniform::Index::MODEL_MATRIX] >= 0) {
        const Matrix4f scale = Matrix4f::Scaling(0.1, 0.1, 0.1);
        const Matrix4f stagePoseMat = Matrix4f::Translation(0,-0.9,0);
        const Matrix4f m1 = stagePoseMat  * scale;
        GL(glUniformMatrix4fv(
                Scene.ECGPlot.UniformLocation[ovrUniform::Index::MODEL_MATRIX],
                1,
                GL_TRUE,
                &m1.M[0][0]));
    }
    Scene.ECGPlot.draw();
    GL(glUseProgram(0));

    // HRStage
    GL(glUseProgram(Scene.HrPlot.Program));
    GL(glBindBufferBase(
            GL_UNIFORM_BUFFER,
            Scene.HrPlot.UniformBinding[ovrUniform::Index::SCENE_MATRICES],
            Scene.SceneMatrices));
    if (Scene.HrPlot.UniformLocation[ovrUniform::Index::VIEW_ID] >=
        0) // NOTE: will not be present when multiview path is enabled.
    {
        GL(glUniform1i(Scene.HrPlot.UniformLocation[ovrUniform::Index::VIEW_ID], 0));
    }
    if (Scene.HrPlot.UniformLocation[ovrUniform::Index::MODEL_MATRIX] >= 0) {
        const Matrix4f scale = Matrix4f::Scaling(0.1, 0.1, 0.1);
        const Matrix4f stagePoseMat = Matrix4f::Translation(0,-1,0);
        const Matrix4f m1 = stagePoseMat  * scale;
        GL(glUniformMatrix4fv(
                Scene.HrPlot.UniformLocation[ovrUniform::Index::MODEL_MATRIX],
                1,
                GL_TRUE,
                &m1.M[0][0]));
    }
    auto end_ts = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = end_ts - start_ts;
    float t = (float)(d.count());
    //ALOGV("time = %f",t);
    GL(glUniform1f(Scene.HrPlot.UniformLocation[ovrUniform::Index::TIME_S], t));
    Scene.HrPlot.draw();
    GL(glUseProgram(0));

    Framebuffer.Unbind();
}

extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_oculusecg_ANativeActivity_dataUpdate(JNIEnv *env, jclass clazz,
                                                            jlong instance,
                                                            jfloat data) {
    dataBuffer.push_back(data);
}


extern "C"
JNIEXPORT void JNICALL
Java_tech_glasgowneuro_oculusecg_ANativeActivity_hrUpdate(JNIEnv *env, jclass clazz,
                                                          jlong inst,
                                                          jfloat v) {
    hrBuffer.push_back(v);
}