/************************************************************************************

Filename	:	SpatialAnchor.cpp
Content		:	This sample is derived from VrCubeWorld_SurfaceView.
                When used in room scale mode, it draws a "carpet" under the
                user to indicate where it is safe to walk around.
Created		:	July, 2020
Authors		:	Cass Everitt

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
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
#include <android/asset_manager.h>

#include "AttysHRVGl.h"

#include "util.h"
#include "spline.hpp"
#include "VeraMoBd.h"
#include "utf8-utils.h"
#include "Iir.h"
#include "attysjava2cpp.h"

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

typedef void(GL_APIENTRY *PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)(
        GLenum target,
        GLsizei samples,
        GLenum internalformat,
        GLsizei width,
        GLsizei height);

typedef void(GL_APIENTRY *PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)(
        GLenum target,
        GLenum attachment,
        GLenum textarget,
        GLuint texture,
        GLint level,
        GLsizei samples);

#endif

#if !defined(GL_OVR_multiview)

typedef void(GL_APIENTRY *PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)(
        GLenum target,
        GLenum attachment,
        GLuint texture,
        GLint level,
        GLint baseViewIndex,
        GLsizei numViews);

#endif

#if !defined(GL_OVR_multiview_multisampled_render_to_texture)

typedef void(GL_APIENTRY *PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)(
        GLenum target,
        GLenum attachment,
        GLuint texture,
        GLint level,
        GLsizei samples,
        GLint baseViewIndex,
        GLsizei numViews);

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
    const char *allExtensions = (const char *) glGetString(GL_EXTENSIONS);
    if (allExtensions != nullptr) {
        glExtensions.multi_view = strstr(allExtensions, "GL_OVR_multiview2") &&
                                  strstr(allExtensions,
                                         "GL_OVR_multiview_multisampled_render_to_texture");

        glExtensions.EXT_texture_border_clamp =
                strstr(allExtensions, "GL_EXT_texture_border_clamp") ||
                strstr(allExtensions, "GL_OES_texture_border_clamp");
        glExtensions.EXT_sRGB_write_control = strstr(allExtensions, "GL_EXT_sRGB_write_control");
    }
}

static const char *GlFrameBufferStatusString(GLenum status) {
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

/**
 * Vector utils
 */
void crossProduct(const float v_A[3], const float v_B[3], float c_P[3]) {
    c_P[0] = v_A[1] * v_B[2] - v_A[2] * v_B[1];
    c_P[1] = v_A[2] * v_B[0] - v_A[0] * v_B[2];
    c_P[2] = v_A[0] * v_B[1] - v_A[1] * v_B[0];
}

void vecDiff(const float v_A[3], const float v_B[3], float c_P[3]) {
    c_P[0] = v_A[0] - v_B[0];
    c_P[1] = v_A[1] - v_B[1];
    c_P[2] = v_A[2] - v_B[2];
}



/*
================================================================================

OvrGeometry

================================================================================
*/

static const std::chrono::time_point<std::chrono::steady_clock> start_ts = std::chrono::steady_clock::now();





////////////// SKYBOX /////////////////////

static const char* SKYBOX_VERTEX_SHADER = R"SHADER_SRC(
        #define NUM_VIEWS 2
        #define VIEW_ID gl_ViewID_OVR
        #extension GL_OVR_multiview2 : require
        layout(num_views=NUM_VIEWS) in;
        in vec3 vertexPosition;
        in vec4 vertexColor;
        uniform mat4 ModelMatrix;
        uniform SceneMatrices
        {
        	uniform mat4 ViewMatrix[NUM_VIEWS];
        	uniform mat4 ProjectionMatrix[NUM_VIEWS];
        } sm;
        out vec4 fragmentColor;
        out vec3 texCoords;
        void main()
        {
            mat3 view = mat3(sm.ViewMatrix[VIEW_ID]);
            vec3 shiftedVertexPos = vertexPosition;
            shiftedVertexPos.y = shiftedVertexPos.y - 0.15;
        	gl_Position = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * ( vec4( shiftedVertexPos, 1.0 ) ) ) );
        	fragmentColor = vertexColor;
            texCoords = vertexPosition;
        }
)SHADER_SRC";

static const char* SKYBOX_FRAGMENT_SHADER = R"SHADER_SRC(
        in vec4 fragmentColor;
        in vec3 texCoords;
        uniform samplerCube skybox;
        out vec4 outColor;
        void main()
        {
           vec4 c = texture(skybox, texCoords);
           float a = 1.0 + texCoords.y;
           if (a < 0.0) {
             a = 0.0;
           }
           if (a > 1.0) {
             a = 1.0;
           }
           outColor = vec4(c.xyz,a);
        }
)SHADER_SRC";

void OvrSkybox::loadTextures(const std::vector<std::string> &faces) const {
    const int textureTarget[] = {
            GL_TEXTURE_CUBE_MAP_POSITIVE_X,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
    };
    if (nullptr == aAssetManager) {
        ALOGE("Cannit load textures. aAssetManager == NULL.");
        return;
    }
    for(int i=0; i < faces.size(); i++) {
        ALOGV("Loading asset %s.", faces[i].c_str());
        AAsset *asset = AAssetManager_open(aAssetManager, faces[i].c_str(), AASSET_MODE_BUFFER);
        std::vector<unsigned char> tmp;
        if (!asset) {
            ALOGE("Asset %s does not exist.", faces[i].c_str());
            return;
        }
        const size_t assetLength = AAsset_getLength(asset);
        tmp.resize(assetLength);
        const long int actualNumberOfBytes = AAsset_read(asset, tmp.data(), assetLength);
        AAsset_close(asset);
        if (assetLength != actualNumberOfBytes) {
            ALOGE("Asset read %s: expected %ld bytes but only got %ld bytes.",
                  faces[i].c_str(), AAsset_getLength(asset), actualNumberOfBytes);
            return;
        }
        int width, height, max_colour;
        sscanf((const char*)(tmp.data()),  // NOLINT(cert-err34-c)
               "P6 %d %d %d",
               &width, &height, &max_colour);
        ALOGV("Cubemap %s loaded: %d x %d, 0..%d",faces[i].c_str(),width,height,max_colour);
        auto it = tmp.begin();
        for(int j = 0; j < 3; j++) {
            it = std::find(it,tmp.end(),0x0a);
            it++;
        }
        tmp.erase(tmp.begin(), it);
//        ALOGV("Cubemap %s data starts with: %0x,%0x. Asset size = %ld, want: %d",
//              faces[i].c_str(),tmp[0],tmp[1],tmp.size(),width*height*3);
        glTexImage2D(
                textureTarget[i],
                0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, tmp.data());
    }
}

void OvrSkybox::CreateGeometry() {
    constexpr int nvertices = 36;

    struct OvrSkyBoxVertices {
        float positions[nvertices][3];
        unsigned char colors[nvertices][4];
    };

    const float vertices[nvertices][3] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
    };

    VertexCount = nvertices;
    IndexCount = nvertices;

    OvrSkyBoxVertices skyBoxVertices = {};
    unsigned short axesIndices[nvertices];
    for(int i = 0; i < nvertices; i++) {
        skyBoxVertices.positions[i][0] = vertices[i][0];
        skyBoxVertices.positions[i][1] = vertices[i][1];
        skyBoxVertices.positions[i][2] = vertices[i][2];
        skyBoxVertices.colors[i][0] = 255;
        skyBoxVertices.colors[i][1] = 255;
        skyBoxVertices.colors[i][2] = 255;
        skyBoxVertices.colors[i][2] = 255;
        axesIndices[i] = i;
    }

    VertexAttribs[0].Index = 0;
    VertexAttribs[0].Name = "vertexPosition";
    VertexAttribs[0].Size = 3;
    VertexAttribs[0].Type = GL_FLOAT;
    VertexAttribs[0].Normalized = false;
    VertexAttribs[0].Stride = sizeof(skyBoxVertices.positions[0]);
    VertexAttribs[0].Pointer = (const GLvoid *) offsetof(OvrSkyBoxVertices, positions);

    VertexAttribs[1].Index = 1;
    VertexAttribs[1].Name = "vertexColor";
    VertexAttribs[1].Size = 4;
    VertexAttribs[1].Normalized = true;
    VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
    VertexAttribs[1].Stride = sizeof(skyBoxVertices.colors[0]);
    VertexAttribs[1].Pointer = (const GLvoid *) offsetof(OvrSkyBoxVertices, colors);

    glGenTextures( 1, &texid );
    ALOGV("Skybox texture ID = %d",texid);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture( GL_TEXTURE_CUBE_MAP, texid );

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    const std::vector<std::string> faces = {
            "right.ppm",
            "left.ppm",
            "top.ppm",
            "bottom.ppm",
            "front.ppm",
            "back.ppm"
    };

    loadTextures(faces);

    glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );

    GL(glGenBuffers(1, &VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(skyBoxVertices), &skyBoxVertices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(axesIndices), axesIndices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    CreateVAO();
}

void OvrSkybox::draw() {
    GL(glActiveTexture(GL_TEXTURE11));
    GL(glBindTexture(GL_TEXTURE_CUBE_MAP, texid));
    GL(glBindVertexArray(VertexArrayObject));
    GL(glUniform1i(glGetUniformLocation(Program, "skybox"), 11));

    GL(glDepthMask(GL_FALSE));
    GL(glEnable ( GL_DEPTH_TEST ));
    GL(glEnable ( GL_BLEND ));
    GL(glBlendFunc ( GL_SRC_ALPHA , GL_ONE_MINUS_SRC_ALPHA));

    GL(glDrawElements(GL_TRIANGLES, IndexCount, GL_UNSIGNED_SHORT, nullptr));

    GL(glDepthMask(GL_TRUE));

    GL(glBindVertexArray(0));
    GL(glBindTexture(GL_TEXTURE_CUBE_MAP, 0));
}











//////////////// AXES ////////////////////////


static const char* AXES_VERTEX_SHADER = R"SHADER_SRC(
        #define NUM_VIEWS 2
        #define VIEW_ID gl_ViewID_OVR
        #extension GL_OVR_multiview2 : require
        layout(num_views=NUM_VIEWS) in;
        in vec3 vertexPosition;
        in vec4 vertexColor;
        uniform mat4 ModelMatrix;
        uniform SceneMatrices
        {
        	uniform mat4 ViewMatrix[NUM_VIEWS];
        	uniform mat4 ProjectionMatrix[NUM_VIEWS];
        } sm;
        out vec4 fragmentColor;
        void main()
        {
        	gl_Position = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * ( vec4( vertexPosition, 1.0 ) ) ) );
        	fragmentColor = vertexColor;
        }
)SHADER_SRC";


static const char* AXES_FRAGMENT_SHADER = R"SHADER_SRC(
        in lowp vec4 fragmentColor;
        out lowp vec4 outColor;
        void main()
        {
        	outColor = fragmentColor;
        }
)SHADER_SRC";


void OvrAxes::CreateGeometry() {
    struct ovrAxesVertices {
        float positions[6][3];
        unsigned char colors[6][4];
    };

    static const ovrAxesVertices axesVertices = {
            // positions
            {{0,   0, 0}, {1,   0, 0}, {0, 0,   0}, {0, 1,   0}, {0, 0, 0}, {0, 0, 1}},
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
    VertexAttribs[0].Pointer = (const GLvoid *) offsetof(ovrAxesVertices, positions);

    VertexAttribs[1].Index = 1;
    VertexAttribs[1].Name = "vertexColor";
    VertexAttribs[1].Size = 4;
    VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
    VertexAttribs[1].Normalized = true;
    VertexAttribs[1].Stride = sizeof(axesVertices.colors[0]);
    VertexAttribs[1].Pointer = (const GLvoid *) offsetof(ovrAxesVertices, colors);

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

//////////////////////////////HR TEXT/////////////////////////////////////////////////////

static const char* HRTEXT_VERTEX_SHADER = R"SHADER_SRC(
        #define NUM_VIEWS 2
        #define VIEW_ID gl_ViewID_OVR
        #extension GL_OVR_multiview2 : require
        layout(num_views=NUM_VIEWS) in;
        in vec3 vertexPosition;
        in vec4 vertexColor;
        in vec2 texCoord;
        uniform mat4 ModelMatrix;
        uniform SceneMatrices
        {
        	uniform mat4 ViewMatrix[NUM_VIEWS];
        	uniform mat4 ProjectionMatrix[NUM_VIEWS];
        } sm;
        out vec4 fragmentColor;
        out vec2 fragmentTexCoord;
        void main()
        {
        	gl_Position = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * ( vec4( vertexPosition, 1.0 ) ) ) );
        	fragmentColor = vertexColor;
        	fragmentTexCoord = texCoord;
        }
)SHADER_SRC";

static const char* HRTEXT_FRAGMENT_SHADER = R"SHADER_SRC(
        in vec4 fragmentColor;
        in lowp vec2 fragmentTexCoord;
        uniform sampler2D Texture0;
        out vec4 outColor;
        void main()
        {
           vec4 color = texture( Texture0, fragmentTexCoord );
           color.a = color.a * 0.5;
           outColor = vec4(fragmentColor.rgb,color.a);
        }
)SHADER_SRC";

void OvrHRText::CreateGeometry() {
    VertexCount = 0;
    IndexCount = 0;

    VertexAttribs[0].Index = 0;
    VertexAttribs[0].Name = "vertexPosition";
    VertexAttribs[0].Size = 3;
    VertexAttribs[0].Type = GL_FLOAT;
    VertexAttribs[0].Normalized = false;
    VertexAttribs[0].Stride = sizeof(axesVertices.positions[0]);
    VertexAttribs[0].Pointer = (const GLvoid *) offsetof(AxesVertices, positions);

    VertexAttribs[2].Index = 1;
    VertexAttribs[2].Name = "vertexColor";
    VertexAttribs[2].Size = 4;
    VertexAttribs[2].Normalized = true;
    VertexAttribs[2].Type = GL_UNSIGNED_BYTE;
    VertexAttribs[2].Stride = sizeof(axesVertices.colors[0]);
    VertexAttribs[2].Pointer = (const GLvoid *) offsetof(AxesVertices, colors);

    VertexAttribs[1].Index = 2;
    VertexAttribs[1].Name = "texCoord";
    VertexAttribs[1].Size = 2;
    VertexAttribs[1].Type = GL_FLOAT;
    VertexAttribs[1].Stride = sizeof(axesVertices.text2D[0]);
    VertexAttribs[1].Pointer = (const GLvoid *) offsetof(AxesVertices, text2D);

    glGenTextures( 1, &texid );
    glActiveTexture(GL_TEXTURE0);
    glBindTexture( GL_TEXTURE_2D, texid );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_ALPHA, (GLsizei)font.tex_width, (GLsizei)font.tex_height,
                  0, GL_ALPHA, GL_UNSIGNED_BYTE, font.tex_data );
    glGenerateMipmap(GL_TEXTURE_2D);

    GL(glGenBuffers(1, &VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(axesVertices), &axesVertices, GL_DYNAMIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(axesIndices), axesIndices, GL_DYNAMIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    add_text(defaultgreeting, 255, 255, 255, 0, 0);

    CreateVAO();

    registerAttysHRCallback([this](float hr){ updateHR(hr); });
    registerAttysDataCallback([this](float v){ attysDataCallBack(v); });
}

void OvrHRText::draw() {
    // just updating it
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(axesVertices), &axesVertices, GL_DYNAMIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    // just updating it
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(axesIndices), axesIndices, GL_DYNAMIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    GL(glUniform1i(glGetUniformLocation(Program, "Texture0"), 0));
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindVertexArray(VertexArrayObject));
    GL(glBindTexture(GL_TEXTURE_2D, texid));

    GL(glDepthMask(GL_FALSE));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glDisable(GL_CULL_FACE));
    GL(glEnable(GL_BLEND));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL(glDrawElements(GL_TRIANGLES, IndexCount, GL_UNSIGNED_SHORT, nullptr));

    GL(glDepthMask(GL_TRUE));
    GL(glDisable(GL_BLEND));

    GL(glBindVertexArray(0));
    GL(glBindTexture(GL_TEXTURE_2D, 0));
}

void OvrHRText::add_text(const char *text,
                         unsigned char r, unsigned char g, unsigned char b,
                         float x, float y, bool centered) {
    VertexCount = 0;
    IndexCount = 0;
    unsigned char a = 255;
    if (centered) {
        float xoff = 0;
        for (int i = 0; i < strlen(text); ++i) {
            texture_glyph_t *glyph = nullptr;
            uint32_t codepoint = ftgl::utf8_to_utf32(text + i);
            glyph = font.glyphs[codepoint >> 8][codepoint & 0xff];
            if (glyph != nullptr) {
                xoff += (float)(glyph->advance_x)/fontsize;
            }
        }
        x -= xoff / 2;
    }
    for (int i = 0; i < strlen(text); ++i) {
        texture_glyph_t *glyph = nullptr;
        uint32_t codepoint = ftgl::utf8_to_utf32(text + i);
        glyph = font.glyphs[codepoint >> 8][codepoint & 0xff];
        if (glyph != nullptr) {
            float x0 = x + (float)(glyph->offset_x)/fontsize;
            float y0 = y + (float)(glyph->offset_y)/fontsize;
            float x1 = x0 + (float)(glyph->width)/fontsize;
            float y1 = y0 - (float)(glyph->height)/fontsize;
            float s0 = glyph->s0;
            float t0 = glyph->t0;
            float s1 = glyph->s1;
            float t1 = glyph->t1;

            struct OneVertex {
                float x, y, z;
                float s, t;
                unsigned char r, g, b, a;
            };
            GLuint index = VertexCount;
            axesIndices[IndexCount++] = index;
            axesIndices[IndexCount++] = index + 1;
            axesIndices[IndexCount++] = index + 2;
            axesIndices[IndexCount++] = index;
            axesIndices[IndexCount++] = index + 2;
            axesIndices[IndexCount++] = index + 3;
            std::vector<OneVertex> oneVertex;
            //ALOGV("Glyph: VC=%d, IC=%d (%f,%f),(%f,%f)",VertexCount,IndexCount,x0,y0,x1,y1);
            oneVertex.push_back({x0, y0, 0, s0, t0, r, g, b, a});
            oneVertex.push_back({x0, y1, 0, s0, t1, r, g, b, a});
            oneVertex.push_back({x1, y1, 0, s1, t1, r, g, b, a});
            oneVertex.push_back({x1, y0, 0, s1, t0, r, g, b, a});
            for (auto &v: oneVertex) {
                axesVertices.positions[VertexCount][0] = v.x;
                axesVertices.positions[VertexCount][1] = v.y;
                axesVertices.positions[VertexCount][2] = v.z;
                axesVertices.text2D[VertexCount][0] = v.s;
                axesVertices.text2D[VertexCount][1] = v.t;
                axesVertices.colors[VertexCount][0] = v.r;
                axesVertices.colors[VertexCount][1] = v.g;
                axesVertices.colors[VertexCount][2] = v.b;
                axesVertices.colors[VertexCount][3] = v.a;
                VertexCount++;
            }
            x += (float)(glyph->advance_x)/fontsize;
        } else {
            ALOGE("Glyph is nullptr");
        }
    }
    // ALOGV("Glyph: HR Text index count: %d. Vertex count: %d.",IndexCount,VertexCount);
}

void OvrHRText::updateHR(float hr) {
    if (hr < 30) return;
    lastHR = hr;
    ALOGV("Updating HR to %d.", (int) round(lastHR));
    char tmp[256];
    sprintf(tmp, "%d BPM", (int) round(lastHR));
    add_text(tmp, 255, 255, 255, 0, 0);
}

void OvrHRText::attysDataCallBack(float v) {
    if (instructionShown) return;
    add_text("Deep breaths and create waves", 255, 255, 255, 0, 0);
    instructionShown = true;
}

//////////////////////////////////////////////////////////////////////

void OvrECGPlot::CreateGeometry() {
    ALOGV("OvrECGPlot::Create()");
    VertexCount = nPoints;
    IndexCount = (nPoints * 2) + 1;

    ALOGE("Creating ECG plot with %d vertices.", VertexCount);
    for (int i = 0; i < nPoints; i++) {
        axesIndices[i * 2] = i;
        axesIndices[i * 2 + 1] = i + 1;

        axesVertices.colors[i][0] = 0;
        axesVertices.colors[i][1] = 255;
        axesVertices.colors[i][2] = 255;
        axesVertices.colors[i][3] = 255;

        axesVertices.positions[i][0] = -2 + (float) i / (float) nPoints * 4.0f;
        axesVertices.positions[i][1] = (float) sin(i / 10.0) * 0.1f;
        // ALOGV("pos = %f,%f", axesVertices.positions[i][0],axesVertices.positions[i][1]);
        axesVertices.positions[i][2] = 0;
    }

    VertexAttribs[0].Index = 0;
    VertexAttribs[0].Name = "vertexPosition";
    VertexAttribs[0].Size = 3;
    VertexAttribs[0].Type = GL_FLOAT;
    VertexAttribs[0].Normalized = false;
    VertexAttribs[0].Stride = sizeof(axesVertices.positions[0]);
    VertexAttribs[0].Pointer = (const GLvoid *) offsetof(ovrAxesVertices, positions);

    VertexAttribs[1].Index = 1;
    VertexAttribs[1].Name = "vertexColor";
    VertexAttribs[1].Size = 4;
    VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
    VertexAttribs[1].Normalized = true;
    VertexAttribs[1].Stride = sizeof(axesVertices.colors[0]);
    VertexAttribs[1].Pointer = (const GLvoid *) offsetof(ovrAxesVertices, colors);

    GL(glGenBuffers(1, &VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(axesVertices), &axesVertices, GL_STREAM_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(axesIndices), axesIndices, GL_STREAM_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    CreateVAO();

    iirhp.setup(SAMPLINGRATE,0.5);

    registerAttysDataCallback([this](float v) { attysDataCallBack(v); });
}

void OvrECGPlot::attysDataCallBack(float v) {
    double v2 = iirhp.filter(v);
    for (int i = 0; i < (nPoints - 1); i++) {
        axesVertices.positions[i][1] = axesVertices.positions[i + 1][1];
    }
    axesVertices.positions[nPoints - 1][1] = (float) v2 * 1000;
}

void OvrECGPlot::draw() {

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


/////////////////////////////////////

const char* HRPLOT_VERTEX_SHADER = R"SHADER_SRC(
        #define NUM_VIEWS 2
        #define VIEW_ID gl_ViewID_OVR
        #extension GL_OVR_multiview2 : require
        layout(num_views=NUM_VIEWS) in;
        in vec3 vertexPosition;
        in vec3 vertexNormal;
        out vec3 fragNormal;
        out vec3 fragPosition;
        out vec3 fragOrigPosition;
        uniform mat4 ModelMatrix;
        uniform SceneMatrices
        {
        	uniform mat4 ViewMatrix[NUM_VIEWS];
        	uniform mat4 ProjectionMatrix[NUM_VIEWS];
        } sm;
        void main()
        {
           gl_Position = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * ( vec4( vertexPosition, 1.0 ) ) ) );
           mat3 normalMatrix = transpose(inverse(mat3(ModelMatrix)));
           fragNormal = normalize(normalMatrix * vertexNormal);
           fragPosition = vec3(ModelMatrix * vec4(vertexPosition, 1));
           fragOrigPosition = vertexPosition;
        }
)SHADER_SRC";

const char* HRPLOT_FRAGMENT_SHADER = R"SHADER_SRC(
in vec3 fragNormal;
in vec3 fragPosition;
in vec3 fragOrigPosition;
out lowp vec4 outColor;
const float pi = 3.14159;
uniform highp float time;
float wave(float x, float y, float t, float speed, vec2 direction) {
    float theta = dot(direction, vec2(x, y));
    return (sin(theta * pi + t * speed) + 2.0) / 3.0;
}
void main()
{
    vec3 light_position = vec3(5.0, 10.0, -50.0);
    vec3 diffuse_light_color = vec3(0.0, 1.0, 1.0);
    vec3 specular_light_color = vec3(1.0, 1.0, 16.0 / 255.0);
    vec3 slow_light_color = vec3(0.0, 0.05, 0.2);
    float shininess = 30.0;

    // Calculate a vector from the fragment location to the light source
    vec3 to_light = light_position - fragPosition;
    to_light = normalize( to_light );

    // The vertex's normal vector is being interpolated across the primitive
    // which can make it un-normalized. So normalize the vertex's normal vector.
    vec3 vertex_normal = normalize( fragNormal );

    // Calculate the cosine of the angle between the vertex's normal vector
    // and the vector going to the light.
    float cos_angle = dot(vertex_normal, to_light);
    cos_angle = clamp(cos_angle, 0.0, 1.0);

    // Scale the color of this fragment based on its angle to the light.
    vec3 diffuse_color = diffuse_light_color * cos_angle;

    // Calculate the reflection vector
    vec3 reflection = 2.0 * dot(vertex_normal,to_light) * vertex_normal - to_light;

    // Calculate a vector from the fragment location to the camera.
    // The camera is at the origin, so negating the vertex location gives the vector
    vec3 to_camera = -1.0 * fragPosition;

    // Calculate the cosine of the angle between the reflection vector
    // and the vector going to the camera.
    reflection = normalize( reflection );
    to_camera = normalize( to_camera );
    cos_angle = dot(reflection, to_camera);
    cos_angle = clamp(cos_angle, 0.0, 1.0);
    cos_angle = pow(cos_angle, shininess);

    // The specular color is from the light source, not the object
    vec3 specular_color = vec3(0.0, 0.0, 0.0);
    if (cos_angle > 0.0) {
        specular_color = specular_light_color * cos_angle;
        diffuse_color = diffuse_color * (1.0 - cos_angle);
    }

    // moving blue shadows
    float v1 = wave(fragPosition.x, fragPosition.z, time, -0.7, vec2(0.03,0.07));
    float v2 = wave(fragPosition.x, fragPosition.z, time, 0.51, vec2(0.03,-0.03));
    float vSlow = (v1+v2)/6.0+0.75;
    float vSlow2 = (v1+v2);

    float theta = abs(dot(normalize(fragPosition),fragNormal));
    float trans = max(1.0 - theta, 0.0);
    float a = trans + 0.5;
    if (length(fragOrigPosition) > 50.0) {
        a = 0.0;
    }
    outColor = vec4(diffuse_color * vSlow + specular_color + slow_light_color * vSlow2, a);
}
)SHADER_SRC";




void OvrHRPlot::CreateGeometry() {
    VertexCount = NR_VERTICES;
    IndexCount = NR_INDICES;

    for (int y = 0; y <= QUAD_GRID_SIZE; y++) {
        for (int x = 0; x <= QUAD_GRID_SIZE; x++) {
            int vertexPosition = y * (QUAD_GRID_SIZE + 1) + x;
            hrVertices.vertices[vertexPosition][0] = (float)(((double) x * delta - 1.0) * scale);
            hrVertices.vertices[vertexPosition][1] = 0;
            hrVertices.vertices[vertexPosition][2] = (float)(((double) y * delta - 1.0) * scale);
            hrVertices.normals[vertexPosition][0] = 0;
            hrVertices.normals[vertexPosition][1] = 1;
            hrVertices.normals[vertexPosition][2] = 0;
        }
    }

    // Generate indices into vertex list
    for (int y = 0; y < QUAD_GRID_SIZE; y++) {
        for (int x = 0; x < QUAD_GRID_SIZE; x++) {
            int indexPosition = y * QUAD_GRID_SIZE + x;
            // tri 0
            indices[6 * indexPosition] = y * (QUAD_GRID_SIZE + 1) + x;    //bl
            indices[6 * indexPosition + 1] = (y + 1) * (QUAD_GRID_SIZE + 1) + x + 1;//tr
            indices[6 * indexPosition + 2] = y * (QUAD_GRID_SIZE + 1) + x + 1;//br
            // tri 1
            indices[6 * indexPosition + 3] = y * (QUAD_GRID_SIZE + 1) + x;    //bl
            indices[6 * indexPosition + 4] = (y + 1) * (QUAD_GRID_SIZE + 1) + x;    //tl
            indices[6 * indexPosition + 5] = (y + 1) * (QUAD_GRID_SIZE + 1) + x + 1;//tr
        }
    }

    VertexAttribs[0].Index = 0;
    VertexAttribs[0].Name = "vertexPosition";
    VertexAttribs[0].Size = 3;
    VertexAttribs[0].Type = GL_FLOAT;
    VertexAttribs[0].Normalized = false;
    VertexAttribs[0].Stride = 3 * sizeof(float);
    VertexAttribs[0].Pointer = (const GLvoid *) offsetof(HRVertices, vertices);

    VertexAttribs[1].Index = 1;
    VertexAttribs[1].Name = "vertexNormal";
    VertexAttribs[1].Size = 3;
    VertexAttribs[1].Type = GL_FLOAT;
    VertexAttribs[1].Normalized = false;
    VertexAttribs[1].Stride = 3 * sizeof(float);
    VertexAttribs[1].Pointer = (const GLvoid *) offsetof(HRVertices, normals);

    GL(glGenBuffers(1, &VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(HRVertices), &hrVertices, GL_STREAM_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STREAM_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    CreateVAO();

    registerAttysHRCallback([this](float v){ addHR(v); });
}

void OvrHRPlot::addHR(float hr) {
    auto current_ts = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = current_ts - start_ts;
    double t = d.count();
    ALOGV("hrUpdate: t=%f, hr=%f",t,hr);
    hrTs.push_back(t);
    hrBuffer.push_back(hr);
    if (hrBuffer.size() > 60) {
        hrBuffer.erase(hrBuffer.begin());
        hrTs.erase(hrTs.begin());
    }
    if (hrTs.size() > 2) {
        hrSpline.calc(hrTs, hrBuffer);
        ALOGV("Prediction: hr(%f)=%f",t+1,hrSpline(t+1));
    }
}

void OvrHRPlot::draw() {
    const int shiftbuffersize = QUAD_GRID_SIZE * 10;
    double hrnorm = -1;
    double hrShiftBuffer[shiftbuffersize] = {};
    const double hrDecayConstant = 0.05;

    const std::chrono::time_point<std::chrono::steady_clock> current_ts = std::chrono::steady_clock::now();
    const std::chrono::duration<double> d = current_ts - start_ts;
    const double t = d.count();

    // leaky min and max boundaries
    if (fps > 0) {
        // gettimg them closer after an artefact
        if (maxHR > minHR) {
            if (maxHR > 30) {
                maxHR = maxHR - hrDecayConstant * maxHR / fps;
            }
            minHR = minHR - hrDecayConstant * (minHR - maxHR) / fps;
            // ALOGV("minHR = %f, maxHR = %f",minHR,maxHR);
        }
    }

    if (hrBuffer.size() > 2) {
        for (int i = 0; i < shiftbuffersize; i++) {
            double dt = t - (double) i / (double) shiftbuffersize * maxtime;
            double hrInterpol = 0;
            if (dt < (hrSpline.getLowerBound() - spline_pred_sec)) {
                hrInterpol = hrSpline(hrSpline.getLowerBound() - spline_pred_sec);
            } else if (dt > (hrSpline.getUpperBound() + spline_pred_sec) ) {
                hrInterpol = hrSpline(hrSpline.getUpperBound() + spline_pred_sec);
            } else {
                hrInterpol = hrSpline(dt);
            }
            if ((hrInterpol < minHR) && (hrInterpol > 30)) minHR = hrInterpol;
            if ((hrInterpol > maxHR) && (hrInterpol < 180)) maxHR = hrInterpol;
            hrShiftBuffer[i] = hrInterpol;
        }
        hrnorm = maxHR - minHR;
        //ALOGV("before: minHR = %f, maxHR = %f, norm = %f", minHR, maxHR, hrnorm);
        if (hrnorm < minHRdiff) {
            hrnorm = minHRdiff;
        }
    }

    WavesAnim wavesAnim[3];
    wavesAnim[0].temporalFreq = 7/4.0;
    wavesAnim[1].temporalFreq = 4/2.0;
    wavesAnim[2].temporalFreq = 5/3.0;

    //windDir += (((float)random() / (float)RAND_MAX) - 0.5f) / fps / 100;
    auto wd = (float)(windDir - M_PI/7.0f);
    for(auto &da:wavesAnim) {
        wd += (float)(M_PI/7.0f);
        da.updateSpatialFreq(wd,windSpeed);
    }

    //ALOGV("after: minHR = %f, maxHR = %f, norm = %f", minHR, maxHR, hrnorm);
    for (int x = 0; x <= QUAD_GRID_SIZE; x++) {
        for (int y = 0; y <= QUAD_GRID_SIZE; y++) {
            int vertexPosition = y * (QUAD_GRID_SIZE + 1) + x;
            const double xc = (double) x - (QUAD_GRID_SIZE / 2.0);
            const double yc = (double) y - (QUAD_GRID_SIZE / 2.0);
            const double maxr = sqrt((QUAD_GRID_SIZE) * (QUAD_GRID_SIZE));
            int r = (int) (round(sqrt(yc * yc + xc * xc) / maxr * (double) shiftbuffersize));
            float h = 0;
            if ( (r < shiftbuffersize) && (hrnorm > 0) ) {
                if (hrShiftBuffer[r] > 0) {
                    h += (float) ((hrShiftBuffer[r] - minHR) / hrnorm * 5.0);
                }
            }
            for(auto &da:wavesAnim) {
                h += da.calcHeight(x, y, t) * 0.1f;
            }
            hrVertices.vertices[vertexPosition][1] = h;
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

    for (int x = 0; x < QUAD_GRID_SIZE; x++) {
        for (int y = 0; y < QUAD_GRID_SIZE; y++) {
            int vertexPosition1 = y * (QUAD_GRID_SIZE + 1) + x;
            int vertexPosition2 = (y + 1) * (QUAD_GRID_SIZE + 1) + x + 1;
            int vertexPosition3 = y * (QUAD_GRID_SIZE + 1) + x + 1;
            float a[3];
            float b[3];
            float c1[3];
            vecDiff(hrVertices.vertices[vertexPosition1], hrVertices.vertices[vertexPosition2], a);
            vecDiff(hrVertices.vertices[vertexPosition1], hrVertices.vertices[vertexPosition3], b);
            crossProduct(a,
                         b,
                         c1);

            float c2[3];
            vertexPosition1 = y * (QUAD_GRID_SIZE + 1) + x;
            vertexPosition2 = (y + 1) * (QUAD_GRID_SIZE + 1) + x;
            vertexPosition3 = y * (QUAD_GRID_SIZE + 1) + x + 1;
            vecDiff(hrVertices.vertices[vertexPosition1], hrVertices.vertices[vertexPosition2], a);
            vecDiff(hrVertices.vertices[vertexPosition1], hrVertices.vertices[vertexPosition3], b);
            crossProduct(a,
                         b,
                         c2);

            hrVertices.normals[vertexPosition1][0] = (c1[0] + c2[0]) / 2;
            hrVertices.normals[vertexPosition1][1] = (c1[1] + c2[1]) / 2;
            hrVertices.normals[vertexPosition1][2] = (c1[2] + c2[2]) / 2;

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

    // calculating the samplingrate
    frameCtr++;
    auto current_fps_ts = std::chrono::steady_clock::now();
    std::chrono::duration<double> d2 = current_fps_ts - start_fps_ts;
    if (floor(d2.count()) > 0) {
        fps = frameCtr;
        start_fps_ts = current_fps_ts;
        frameCtr = 0;
        ALOGV("fps = %d", fps);
    }
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
}

void OvrGeometry::CreateVAO() {
    GL(glGenVertexArrays(1, &VertexArrayObject));
    GL(glBindVertexArray(VertexArrayObject));

    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));

    for (auto &VertexAttrib: VertexAttribs) {
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
    const char *name;
};

static ovrUniform ProgramUniforms[] = {
        {ovrUniform::Index::MODEL_MATRIX,   ovrUniform::Type::MATRIX4X4, "ModelMatrix"},
        {ovrUniform::Index::VIEW_ID,        ovrUniform::Type::INTEGER,   "ViewID"},
        {ovrUniform::Index::SCENE_MATRICES, ovrUniform::Type::BUFFER,    "SceneMatrices"},
        {ovrUniform::Index::COLOR_SCALE,    ovrUniform::Type::VECTOR4,   "ColorScale"},
        {ovrUniform::Index::COLOR_BIAS,     ovrUniform::Type::VECTOR4,   "ColorBias"},
        {ovrUniform::Index::TIME_S,         ovrUniform::Type::FLOAT,     "time"},
};

static const char *programVersion = "#version 300 es\n";

bool OvrGeometry::Create(const char *vertexSource, const char *fragmentSource) {
    GLint r;

    GL(VertexShader = glCreateShader(GL_VERTEX_SHADER));

    const char *vertexSources[3] = {programVersion, "", vertexSource};
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

    const char *fragmentSources[2] = {programVersion, fragmentSource};
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

    CreateGeometry();

    // Bind the vertex attribute locations.
    for (auto &a: VertexAttribs) {
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

static void *GlGetExtensionProc(const char *functionName) {
    return (void *) eglGetProcAddress(functionName);
}

bool ovrFramebuffer::Create(
        const GLenum colorFormat,
        const int width,
        const int height,
        const int multisamples,
        const int swapChainLength,
        const GLuint *colorTextures) {
    auto glFramebufferTextureMultiviewOVR =
            (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) GlGetExtensionProc(
                    "glFramebufferTextureMultiviewOVR");
    auto glFramebufferTextureMultisampleMultiviewOVR =
            (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC) GlGetExtensionProc(
                    "glFramebufferTextureMultisampleMultiviewOVR");

    Width = width;
    Height = height;
    Multisamples = multisamples;
    SwapChainLength = swapChainLength;

    Elements = new Element[SwapChainLength];

    for (int i = 0; i < SwapChainLength; i++) {
        Element &el = Elements[i];
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
        Element &el = Elements[i];
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

void ovrScene::SetClearColor(const float *c) {
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

    if (!ovrSkybox.Create(SKYBOX_VERTEX_SHADER,SKYBOX_FRAGMENT_SHADER)) {
        ALOGE("Failed to compile Skybox program");
    }

    // Axes
    if (!Axes.Create(AXES_VERTEX_SHADER, AXES_FRAGMENT_SHADER)) {
        ALOGE("Failed to compile axes program");
    }

    if (!HrText.Create(HRTEXT_VERTEX_SHADER, HRTEXT_FRAGMENT_SHADER)) {
        ALOGE("Failed to compile hrtext program");
    }

    // ECG
    if (!ECGPlot.Create(AXES_VERTEX_SHADER, AXES_FRAGMENT_SHADER)) {
        ALOGE("Failed to compile plot program");
    }

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
    ovrSkybox.Destroy();
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
        GLuint *colorTextures) {
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
}

void ovrAppRenderer::Destroy() {
    Framebuffer.Destroy();
}

void ovrAppRenderer::RenderFrame(ovrAppRenderer::FrameIn frameIn) {
    // Update the scene matrices.
    GL(glBindBuffer(GL_UNIFORM_BUFFER, Scene.SceneMatrices));
    GL(auto *sceneMatrices = (Matrix4f *) glMapBufferRange(
            GL_UNIFORM_BUFFER,
            0,
            4 * sizeof(Matrix4f) /* 2 view + 2 proj matrices */,
               GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

    if (sceneMatrices != nullptr) {
        memcpy((char *) sceneMatrices, &frameIn.View, 4 * sizeof(Matrix4f));
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


    // skybox
    GL(glUseProgram(Scene.ovrSkybox.Program));
    GL(glBindBufferBase(
            GL_UNIFORM_BUFFER,
            Scene.ovrSkybox.UniformBinding[ovrUniform::Index::SCENE_MATRICES],
            Scene.SceneMatrices));
    if (Scene.ovrSkybox.UniformLocation[ovrUniform::Index::VIEW_ID] >=
        0) // NOTE: will not be present when multiview path is enabled.
    {
        GL(glUniform1i(Scene.ovrSkybox.UniformLocation[ovrUniform::Index::VIEW_ID], 0));
    }
    if (Scene.Axes.UniformLocation[ovrUniform::Index::MODEL_MATRIX] >= 0) {
        const Matrix4f rot = Matrix4f::RotationY(M_PI/2.0f) * Matrix4f::Scaling(10.0, 10.0, 10.0);
        GL(glUniformMatrix4fv(
                Scene.Axes.UniformLocation[ovrUniform::Index::MODEL_MATRIX],
                1,
                GL_TRUE,
                &rot.M[0][0]));
    }
    Scene.ovrSkybox.draw();
    GL(glUseProgram(0));


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
        const Matrix4f stagePoseMat = Matrix4f::Translation(0, -1, -2);
        const Matrix4f m1 = stagePoseMat * scale;
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
        const Matrix4f stagePoseMat = Matrix4f::Translation(0, -0.5, -1);
        const Matrix4f m1 = stagePoseMat * scale;
        GL(glUniformMatrix4fv(
                Scene.ECGPlot.UniformLocation[ovrUniform::Index::MODEL_MATRIX],
                1,
                GL_TRUE,
                &m1.M[0][0]));
    }
    Scene.ECGPlot.draw();
    GL(glUseProgram(0));


    // HRPlot
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
        const Matrix4f scale = Matrix4f::Scaling(0.2, 0.1, 0.2);
        const Matrix4f stagePoseMat = Matrix4f::Translation(0, -1, 0);
        const Matrix4f m1 = stagePoseMat * scale;
        GL(glUniformMatrix4fv(
                Scene.HrPlot.UniformLocation[ovrUniform::Index::MODEL_MATRIX],
                1,
                GL_TRUE,
                &m1.M[0][0]));
    }
    auto current_ts = std::chrono::steady_clock::now();
    std::chrono::duration<double> d = current_ts - start_ts;
    t = (float) (d.count());
    //ALOGV("time = %f",t);
    GL(glUniform1f(Scene.HrPlot.UniformLocation[ovrUniform::Index::TIME_S], t));
    Scene.HrPlot.draw();
    GL(glUseProgram(0));


    // HR Text
    GL(glUseProgram(Scene.HrText.Program));
    GL(glBindBufferBase(
            GL_UNIFORM_BUFFER,
            Scene.HrText.UniformBinding[ovrUniform::Index::SCENE_MATRICES],
            Scene.SceneMatrices));
    if (Scene.HrText.UniformLocation[ovrUniform::Index::VIEW_ID] >=
        0) // NOTE: will not be present when multiview path is enabled.
    {
        GL(glUniform1i(Scene.HrText.UniformLocation[ovrUniform::Index::VIEW_ID], 0));
    }
    if (Scene.HrText.UniformLocation[ovrUniform::Index::MODEL_MATRIX] >= 0) {
        const Matrix4f scale = Matrix4f::Scaling(0.1, 0.1, 0.1);
        const Matrix4f stagePoseMat = Matrix4f::Translation(0.0, -0.5, -0.75);
        const Matrix4f rot = Matrix4f::RotationX(-M_PI/2.0);
        const Matrix4f m1 = stagePoseMat * scale * rot;
        GL(glUniformMatrix4fv(
                Scene.HrText.UniformLocation[ovrUniform::Index::MODEL_MATRIX],
                1,
                GL_TRUE,
                &m1.M[0][0]));
    }
    Scene.HrText.draw();
    GL(glUseProgram(0));


    Framebuffer.Unbind();
}

