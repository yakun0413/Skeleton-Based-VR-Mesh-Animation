/************************************************************************************
 Filename    :   Win32_GLAppUtil.h
 Content     :   OpenGL and Application/Window setup functionality for RoomTiny
 Created     :   October 20th, 2014
 Author      :   Tom Heath
 Copyright   :   Copyright 2014 Oculus, LLC. All Rights reserved.
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 *************************************************************************************/

#ifndef OVR_Win32_GLAppUtil_h
#define OVR_Win32_GLAppUtil_h

#include "GL/CAPI_GLE.h"
#include "Extras/OVR_Math.h"
#include "OVR_CAPI_GL.h"
#include <assert.h>
#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <vector>
#include <sstream>
#include <iomanip>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include "assimp/mesh.h"
#include "../Common/mesh.h"
#include "../Common/shader.h"
#include "../Common/camara.h"
#include "../Common/filesystem.h"

using namespace OVR;
using namespace std;

#ifndef VALIDATE
#define VALIDATE(x, msg) if (!(x)) { MessageBoxA(NULL, (msg), "OculusRoomTiny", MB_ICONERROR | MB_OK); exit(-1); }
#endif

#ifndef OVR_DEBUG_LOG
#define OVR_DEBUG_LOG(x)
#endif

//Locate a line in the txt file to start reading
ifstream& seek_to_line(ifstream& in, int line) //Position the open file in, to the line line.
{
int i;
char buf[1024];
in.seekg(0, ios::beg);  //Navigate to the beginning of the file.
for (i = 0; i < line; i++)
{
in.getline(buf, sizeof(buf));//Read line
}
return in;
}

//---------------------------------------------------------------------------------------
struct DepthBuffer
{
GLuint        texId;

DepthBuffer(Sizei size)
{
glGenTextures(1, &texId);
glBindTexture(GL_TEXTURE_2D, texId);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

GLenum internalFormat = GL_DEPTH_COMPONENT24;
GLenum type = GL_UNSIGNED_INT;
if (GLE_ARB_depth_buffer_float)
{
internalFormat = GL_DEPTH_COMPONENT32F;
type = GL_FLOAT;
}

glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, size.w, size.h, 0, GL_DEPTH_COMPONENT, type, NULL);
}
~DepthBuffer()
{
if (texId)
{
glDeleteTextures(1, &texId);
texId = 0;
}
}
};

//--------------------------------------------------------------------------
struct TextureBuffer
{
GLuint              texId;
GLuint              fboId;
Sizei               texSize;

TextureBuffer(bool rendertarget, Sizei size, int mipLevels, unsigned char* data) :
texId(0),
fboId(0),
texSize(0, 0)
{
texSize = size;

glGenTextures(1, &texId);
glBindTexture(GL_TEXTURE_2D, texId);

if (rendertarget)
{
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
else
{
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, texSize.w, texSize.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

if (mipLevels > 1)
{
glGenerateMipmap(GL_TEXTURE_2D);
}

if (rendertarget)
{
glGenFramebuffers(1, &fboId);
}
}

~TextureBuffer()
{
if (texId)
{
glDeleteTextures(1, &texId);
texId = 0;
}
if (fboId)
{
glDeleteFramebuffers(1, &fboId);
fboId = 0;
}
}

Sizei GetSize() const
{
return texSize;
}

void SetAndClearRenderSurface(DepthBuffer* dbuffer)
{
VALIDATE(fboId, "Texture wasn't created as a render target");

GLuint curTexId = texId;

glBindFramebuffer(GL_FRAMEBUFFER, fboId);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dbuffer->texId, 0);

glViewport(0, 0, texSize.w, texSize.h);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
glEnable(GL_FRAMEBUFFER_SRGB);
}

void UnsetRenderSurface()
{
VALIDATE(fboId, "Texture wasn't created as a render target");

glBindFramebuffer(GL_FRAMEBUFFER, fboId);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
}
};

//-------------------------------------------------------------------------------------------
struct OGL
{
static const bool       UseDebugContext = false;

HWND                    Window;
HDC                     hDC;
HGLRC                   WglContext;
OVR::GLEContext         GLEContext;
bool                    Running;
bool                    Key[256];
int                     WinSizeW;
int                     WinSizeH;
GLuint                  fboId;
HINSTANCE               hInstance;

static LRESULT CALLBACK WindowProc(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
OGL* p = reinterpret_cast<OGL*>(GetWindowLongPtr(hWnd, 0));
switch (Msg)
{
case WM_KEYDOWN:
p->Key[wParam] = true;
break;
case WM_KEYUP:
p->Key[wParam] = false;
break;
case WM_DESTROY:
p->Running = false;
break;
default:
return DefWindowProcW(hWnd, Msg, wParam, lParam);
}
if ((p->Key['Q'] && p->Key[VK_CONTROL]) || p->Key[VK_ESCAPE])
{
p->Running = false;
}
return 0;
}

OGL() :
Window(nullptr),
hDC(nullptr),
WglContext(nullptr),
GLEContext(),
Running(false),
WinSizeW(0),
WinSizeH(0),
fboId(0),
hInstance(nullptr)
{
// Clear input
for (int i = 0; i < sizeof(Key) / sizeof(Key[0]); ++i)
Key[i] = false;
}

~OGL()
{
ReleaseDevice();
CloseWindow();
}

bool InitWindow(HINSTANCE hInst, LPCWSTR title)
{
hInstance = hInst;
Running = true;

WNDCLASSW wc;
memset(&wc, 0, sizeof(wc));
wc.style = CS_CLASSDC;
wc.lpfnWndProc = WindowProc;
wc.cbWndExtra = sizeof(struct OGL*);
wc.hInstance = GetModuleHandleW(NULL);
wc.lpszClassName = L"ORT";
RegisterClassW(&wc);

// adjust the window size and show at InitDevice time
Window = CreateWindowW(wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, 0, 0, hInstance, 0);
if (!Window) return false;

SetWindowLongPtr(Window, 0, LONG_PTR(this));

hDC = GetDC(Window);

return true;
}

void CloseWindow()
{
if (Window)
{
if (hDC)
{
ReleaseDC(Window, hDC);
hDC = nullptr;
}
DestroyWindow(Window);
Window = nullptr;
UnregisterClassW(L"OGL", hInstance);
}
}

// Note: currently there is no way to get GL to use the passed pLuid
bool InitDevice(int vpW, int vpH, const LUID* /*pLuid*/, bool windowed = true)
{
UNREFERENCED_PARAMETER(windowed);

WinSizeW = vpW;
WinSizeH = vpH;

RECT size = { 0, 0, vpW, vpH };
AdjustWindowRect(&size, WS_OVERLAPPEDWINDOW, false);

const UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW;
if (!SetWindowPos(Window, nullptr, 0, 0, size.right - size.left, size.bottom - size.top, flags))
return false;

PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARBFunc = nullptr;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARBFunc = nullptr;
{
// First create a context for the purpose of getting access to wglChoosePixelFormatARB / wglCreateContextAttribsARB.
PIXELFORMATDESCRIPTOR pfd;
memset(&pfd, 0, sizeof(pfd));
pfd.nSize = sizeof(pfd);
pfd.nVersion = 1;
pfd.iPixelType = PFD_TYPE_RGBA;
pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
pfd.cColorBits = 32;
pfd.cDepthBits = 16;
int pf = ChoosePixelFormat(hDC, &pfd);
VALIDATE(pf, "Failed to choose pixel format.");

VALIDATE(SetPixelFormat(hDC, pf, &pfd), "Failed to set pixel format.");

HGLRC context = wglCreateContext(hDC);
VALIDATE(context, "wglCreateContextfailed.");
VALIDATE(wglMakeCurrent(hDC, context), "wglMakeCurrent failed.");

wglChoosePixelFormatARBFunc = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
wglCreateContextAttribsARBFunc = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
assert(wglChoosePixelFormatARBFunc && wglCreateContextAttribsARBFunc);

wglDeleteContext(context);
}

// Now create the real context that we will be using.
int iAttributes[] =
{
// WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
WGL_COLOR_BITS_ARB, 32,
WGL_DEPTH_BITS_ARB, 16,
WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
0, 0
};

float fAttributes[] = { 0, 0 };
int   pf = 0;
UINT  numFormats = 0;

VALIDATE(wglChoosePixelFormatARBFunc(hDC, iAttributes, fAttributes, 1, &pf, &numFormats),
"wglChoosePixelFormatARBFunc failed.");

PIXELFORMATDESCRIPTOR pfd;
memset(&pfd, 0, sizeof(pfd));
VALIDATE(SetPixelFormat(hDC, pf, &pfd), "SetPixelFormat failed.");

GLint attribs[16] = {};
int   attribCount = 0;
if (UseDebugContext)
{
attribs[attribCount++] = WGL_CONTEXT_FLAGS_ARB;
attribs[attribCount++] = WGL_CONTEXT_DEBUG_BIT_ARB;
}

attribs[attribCount] = 0;

WglContext = wglCreateContextAttribsARBFunc(hDC, 0, attribs);
VALIDATE(wglMakeCurrent(hDC, WglContext), "wglMakeCurrent failed.");

OVR::GLEContext::SetCurrentContext(&GLEContext);
GLEContext.Init();

glGenFramebuffers(1, &fboId);

glEnable(GL_DEPTH_TEST);
glFrontFace(GL_CW);
glEnable(GL_CULL_FACE);

if (UseDebugContext && GLE_ARB_debug_output)
{
glDebugMessageCallbackARB(DebugGLCallback, NULL);
if (glGetError())
{
OVR_DEBUG_LOG(("glDebugMessageCallbackARB failed."));
}

glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);

// Explicitly disable notification severity output.
glDebugMessageControlARB(GL_DEBUG_SOURCE_API, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
}

return true;
}

bool HandleMessages(void)
{
MSG msg;
while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
{
TranslateMessage(&msg);
DispatchMessage(&msg);
}
return Running;
}

void Run(bool (*MainLoop)(bool retryCreate))
{
while (HandleMessages())
{
// true => we'll attempt to retry for ovrError_DisplayLost
if (!MainLoop(true))
break;
// Sleep a bit before retrying to reduce CPU load while the HMD is disconnected
Sleep(10);
}
}

void ReleaseDevice()
{
if (fboId)
{
glDeleteFramebuffers(1, &fboId);
fboId = 0;
}
if (WglContext)
{
wglMakeCurrent(NULL, NULL);
wglDeleteContext(WglContext);
WglContext = nullptr;
}
GLEContext.Shutdown();
}

static void GLAPIENTRY DebugGLCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
UNREFERENCED_PARAMETER(source);
UNREFERENCED_PARAMETER(type);
UNREFERENCED_PARAMETER(id);
UNREFERENCED_PARAMETER(severity);
UNREFERENCED_PARAMETER(length);
UNREFERENCED_PARAMETER(message);
UNREFERENCED_PARAMETER(userParam);
OVR_DEBUG_LOG(("Message from OpenGL: %s\n", message));
}
};

// Global OpenGL state
static OGL Platform;

//------------------------------------------------------------------------------
struct ShaderFill
{
GLuint            program;
TextureBuffer* texture;

ShaderFill(GLuint vertexShader, GLuint pixelShader, TextureBuffer* _texture)
{
texture = _texture;

program = glCreateProgram();

glAttachShader(program, vertexShader);
glAttachShader(program, pixelShader);

glLinkProgram(program);

glDetachShader(program, vertexShader);
glDetachShader(program, pixelShader);

GLint r;
glGetProgramiv(program, GL_LINK_STATUS, &r);
if (!r)
{
GLchar msg[1024];
glGetProgramInfoLog(program, sizeof(msg), 0, msg);
OVR_DEBUG_LOG(("Linking shaders failed: %s\n", msg));
}
}

~ShaderFill()
{
if (program)
{
glDeleteProgram(program);
program = 0;
}
if (texture)
{
delete texture;
texture = nullptr;
}
}
};

//----------------------------------------------------------------
struct VertexBuffer
{
GLuint    buffer;

VertexBuffer(void* vertices, size_t size)
{
glGenBuffers(1, &buffer);
glBindBuffer(GL_ARRAY_BUFFER, buffer);
glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}
~VertexBuffer()
{
if (buffer)
{
glDeleteBuffers(1, &buffer);
buffer = 0;
}
}
};

//----------------------------------------------------------------
struct IndexBuffer
{
GLuint    buffer;

IndexBuffer(void* indices, size_t size)
{
glGenBuffers(1, &buffer);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
}
~IndexBuffer()
{
if (buffer)
{
glDeleteBuffers(1, &buffer);
buffer = 0;
}
}
};

//----------------------------------------------------------------
struct Bones
{
std::string mName = "";
C_STRUCT aiMatrix4x4 mOffsetMatrix;
unsigned int mNumWeights = 0;
C_STRUCT aiVertexWeight* mWeights = nullptr;
unsigned int iVertexID = 0;
ai_real* fWeight = nullptr;
C_STRUCT aiFace* mFaces = nullptr;
aiNode* mNode = nullptr;

Bones(){
}
};

//---------------------------------------------------------------------------
struct Model
{
struct Vertex
{
Vector3f  Pos; // position
DWORD     C; // color
float     U, V; // texture
};

Vector3f        Pos;
Quatf           Rot;
Matrix4f        Mat;
int             numVertices, numIndices;
Vertex          Vertices[20000]; // Note fixed maximum
GLushort        Indices[20000];
ShaderFill* Fill;
VertexBuffer* vertexBuffer;
IndexBuffer* indexBuffer;
std::vector<Bones> _vecBones;

Model(Vector3f pos, ShaderFill* fill) :
Pos(pos),
Rot(),
Mat(),
numVertices(0),
numIndices(0),
Fill(fill),
vertexBuffer(nullptr),
indexBuffer(nullptr)
{}

~Model()
{
FreeBuffers();
}

Matrix4f& GetMatrix()
{
Mat = Matrix4f(Rot);
Mat = Matrix4f::Translation(Pos) * Mat;
return Mat;
}

void AddVertex(const Vertex& v) { Vertices[numVertices++] = v; }
void AddIndex(GLushort a) { Indices[numIndices++] = a; }

void AllocateBuffers()
{
vertexBuffer = new VertexBuffer(&Vertices[0], numVertices * sizeof(Vertices[0]));
indexBuffer = new IndexBuffer(&Indices[0], numIndices * sizeof(Indices[0]));
}

void FreeBuffers()
{
delete vertexBuffer; vertexBuffer = nullptr;
delete indexBuffer; indexBuffer = nullptr;
}

void addPoint(const glm::vec3& vec3Point, const DWORD c)
{
AddIndex(0 + GLushort(numVertices));

Vertex vertex;
vertex.Pos = Vector3f(vec3Point.x, vec3Point.y, vec3Point.z);
float dist1 = (vertex.Pos - Vector3f(-2, 4, -2)).Length();
float dist2 = (vertex.Pos - Vector3f(3, 4, -3)).Length();
float dist3 = (vertex.Pos - Vector3f(-4, 3, 25)).Length();
int   bri = rand() % 160;
float B = ((c >> 16) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
float G = ((c >> 8) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
float R = ((c >> 0) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
vertex.C = (c & 0xff000000) +
((R > 255 ? 255 : DWORD(R)) << 16) +
((G > 255 ? 255 : DWORD(G)) << 8) +
(B > 255 ? 255 : DWORD(B));
AddVertex(vertex);
}
void RenderPoints(Matrix4f view, Matrix4f proj)
{
Matrix4f combined = proj * view * GetMatrix();

glUseProgram(Fill->program);
glUniformMatrix4fv(glGetUniformLocation(Fill->program, "matWVP"), 1, GL_TRUE, (FLOAT*)&combined);

glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer->buffer);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->buffer);

GLuint posLoc = glGetAttribLocation(Fill->program, "Position");
GLuint colorLoc = glGetAttribLocation(Fill->program, "Color");

glEnableVertexAttribArray(posLoc);
glEnableVertexAttribArray(colorLoc);

glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, Pos));
glVertexAttribPointer(colorLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, C));

//glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, NULL);
glDrawElements(GL_POINTS, numIndices, GL_UNSIGNED_SHORT, NULL);

glDisableVertexAttribArray(posLoc);
glDisableVertexAttribArray(colorLoc);

glBindBuffer(GL_ARRAY_BUFFER, 0);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

glUseProgram(0);
}

void AddBox(float x1, float y1, float z1, float x2, float y2, float z2, DWORD c)
{
Vector3f Vert[][2] =
{
{Vector3f(x1, y2, z1), Vector3f(z1, x1)}, {Vector3f(x2, y2, z1), Vector3f(z1, x2)},
{Vector3f(x2, y2, z2), Vector3f(z2, x2)}, {Vector3f(x1, y2, z2), Vector3f(z2, x1)},
{Vector3f(x1, y1, z1), Vector3f(z1, x1)}, {Vector3f(x2, y1, z1), Vector3f(z1, x2)},
{Vector3f(x2, y1, z2), Vector3f(z2, x2)}, {Vector3f(x1, y1, z2), Vector3f(z2, x1)},
{Vector3f(x1, y1, z2), Vector3f(z2, y1)}, {Vector3f(x1, y1, z1), Vector3f(z1, y1)},
{Vector3f(x1, y2, z1), Vector3f(z1, y2)}, {Vector3f(x1, y2, z2), Vector3f(z2, y2)},
{Vector3f(x2, y1, z2), Vector3f(z2, y1)}, {Vector3f(x2, y1, z1), Vector3f(z1, y1)},
{Vector3f(x2, y2, z1), Vector3f(z1, y2)}, {Vector3f(x2, y2, z2), Vector3f(z2, y2)},
{Vector3f(x1, y1, z1), Vector3f(x1, y1)}, {Vector3f(x2, y1, z1), Vector3f(x2, y1)},
{Vector3f(x2, y2, z1), Vector3f(x2, y2)}, {Vector3f(x1, y2, z1), Vector3f(x1, y2)},
{Vector3f(x1, y1, z2), Vector3f(x1, y1)}, {Vector3f(x2, y1, z2), Vector3f(x2, y1)},
{Vector3f(x2, y2, z2), Vector3f(x2, y2)}, {Vector3f(x1, y2, z2), Vector3f(x1, y2)}
};

GLushort CubeIndices[] =
{
0, 1, 3, 3, 1, 2,
5, 4, 6, 6, 4, 7,
8, 9, 11, 11, 9, 10,
13, 12, 14, 14, 12, 15,
16, 17, 19, 19, 17, 18,
21, 20, 22, 22, 20, 23
};

for (int i = 0; i < sizeof(CubeIndices) / sizeof(CubeIndices[0]); ++i)
AddIndex(CubeIndices[i] + GLushort(numVertices));

// Generate a quad for each box face
for (int v = 0; v < 6 * 4; v++)
{
// Make vertices, with some token lighting
Vertex vvv; vvv.Pos = Vert[v][0]; vvv.U = Vert[v][1].x; vvv.V = Vert[v][1].y;
float dist1 = (vvv.Pos - Vector3f(-2, 4, -2)).Length();
float dist2 = (vvv.Pos - Vector3f(3, 4, -3)).Length();
float dist3 = (vvv.Pos - Vector3f(-4, 3, 25)).Length();
int   bri = rand() % 160;
float B = ((c >> 16) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
float G = ((c >> 8) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
float R = ((c >> 0) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
vvv.C = (c & 0xff000000) +
((R > 255 ? 255 : DWORD(R)) << 16) +
((G > 255 ? 255 : DWORD(G)) << 8) +
(B > 255 ? 255 : DWORD(B));
AddVertex(vvv);
}
}
void AddBox25(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, float x4, float y4, float z4, float x5, float y5, float z5, float x6, float y6, float z6, float x7, float y7, float z7, float x8, float y8, float z8, float x9, float y9, float z9, float x10, float y10, float z10, float x11, float y11, float z11, float x12, float y12, float z12, float x13, float y13, float z13, float x14, float y14, float z14, float x15, float y15, float z15, float x16, float y16, float z16, float x17, float y17, float z17, float x18, float y18, float z18, float x19, float y19, float z19, float x20, float y20, float z20, float x21, float y21, float z21, float x22, float y22, float z22, float x23, float y23, float z23, float x24, float y24, float z24, float x25, float y25, float z25, DWORD c)
{
Vector3f Vert[][2] =
{
{Vector3f(x1, y1 + 0.05f, z1), Vector3f(z1, x1)}, {Vector3f(x1 + 0.05f, y1 + 0.05f, z1), Vector3f(z1, x1 + 0.05f)},
{Vector3f(x1 + 0.05f, y1 + 0.05f, z1 + 0.05f), Vector3f(z1 + 0.05f, x1 + 0.05f)}, {Vector3f(x1, y1 + 0.05f, z1 + 0.05f), Vector3f(z1 + 0.05f, x1)},
{Vector3f(x1, y1, z1), Vector3f(z1, x1)}, {Vector3f(x1 + 0.05f, y1, z1), Vector3f(z1, x1 + 0.05f)},
{Vector3f(x1 + 0.05f, y1, z1 + 0.05f), Vector3f(z1 + 0.05f, x1 + 0.05f)}, {Vector3f(x1, y1, z1 + 0.05f), Vector3f(z1 + 0.05f, x1)},
{Vector3f(x1, y1, z1 + 0.05f), Vector3f(z1 + 0.05f, y1)}, {Vector3f(x1, y1, z1), Vector3f(z1, y1)},
{Vector3f(x1, y1 + 0.05f, z1), Vector3f(z1, y1 + 0.05f)}, {Vector3f(x1, y1 + 0.05f, z1 + 0.05f), Vector3f(z1 + 0.05f, y1 + 0.05f)},
{Vector3f(x1 + 0.05f, y1, z1 + 0.05f), Vector3f(z1 + 0.05f, y1)}, {Vector3f(x1 + 0.05f, y1, z1), Vector3f(z1, y1)},
{Vector3f(x1 + 0.05f, y1 + 0.05f, z1), Vector3f(z1, y1 + 0.05f)}, {Vector3f(x1 + 0.05f, y1 + 0.05f, z1 + 0.05f), Vector3f(z1 + 0.05f, y1 + 0.05f)},
{Vector3f(x1, y1, z1), Vector3f(x1, y1)}, {Vector3f(x1 + 0.05f, y1, z1), Vector3f(x1 + 0.05f, y1)},
{Vector3f(x1 + 0.05f, y1 + 0.05f, z1), Vector3f(x1 + 0.05f, y1 + 0.05f)}, {Vector3f(x1, y1 + 0.05f, z1), Vector3f(x1, y1 + 0.05f)},
{Vector3f(x1, y1, z1 + 0.05f), Vector3f(x1, y1)}, {Vector3f(x1 + 0.05f, y1, z1 + 0.05f), Vector3f(x1 + 0.05f, y1)},
{Vector3f(x1 + 0.05f, y1 + 0.05f, z1 + 0.05f), Vector3f(x1 + 0.05f, y1 + 0.05f)}, {Vector3f(x1, y1 + 0.05f, z1 + 0.05f), Vector3f(x1, y1 + 0.05f)},
{Vector3f(x2, y2 + 0.05f, z2), Vector3f(z2, x2)}, {Vector3f(x2 + 0.05f, y2 + 0.05f, z2), Vector3f(z2, x2 + 0.05f)},
{Vector3f(x2 + 0.05f, y2 + 0.05f, z2 + 0.05f), Vector3f(z2 + 0.05f, x2 + 0.05f)}, {Vector3f(x2, y2 + 0.05f, z2 + 0.05f), Vector3f(z2 + 0.05f, x2)},
{Vector3f(x2, y2, z2), Vector3f(z2, x2)}, {Vector3f(x2 + 0.05f, y2, z2), Vector3f(z2, x2 + 0.05f)},
{Vector3f(x2 + 0.05f, y2, z2 + 0.05f), Vector3f(z2 + 0.05f, x2 + 0.05f)}, {Vector3f(x2, y2, z2 + 0.05f), Vector3f(z2 + 0.05f, x2)},
{Vector3f(x2, y2, z2 + 0.05f), Vector3f(z2 + 0.05f, y2)}, {Vector3f(x2, y2, z2), Vector3f(z2, y2)},
{Vector3f(x2, y2 + 0.05f, z2), Vector3f(z2, y2 + 0.05f)}, {Vector3f(x2, y2 + 0.05f, z2 + 0.05f), Vector3f(z2 + 0.05f, y2 + 0.05f)},
{Vector3f(x2 + 0.05f, y2, z2 + 0.05f), Vector3f(z2 + 0.05f, y2)}, {Vector3f(x2 + 0.05f, y2, z2), Vector3f(z2, y2)},
{Vector3f(x2 + 0.05f, y2 + 0.05f, z2), Vector3f(z2, y2 + 0.05f)}, {Vector3f(x2 + 0.05f, y2 + 0.05f, z2 + 0.05f), Vector3f(z2 + 0.05f, y2 + 0.05f)},
{Vector3f(x2, y2, z2), Vector3f(x2, y2)}, {Vector3f(x2 + 0.05f, y2, z2), Vector3f(x2 + 0.05f, y2)},
{Vector3f(x2 + 0.05f, y2 + 0.05f, z2), Vector3f(x2 + 0.05f, y2 + 0.05f)}, {Vector3f(x2, y2 + 0.05f, z2), Vector3f(x2, y2 + 0.05f)},
{Vector3f(x2, y2, z2 + 0.05f), Vector3f(x2, y2)}, {Vector3f(x2 + 0.05f, y2, z2 + 0.05f), Vector3f(x2 + 0.05f, y2)},
{Vector3f(x2 + 0.05f, y2 + 0.05f, z2 + 0.05f), Vector3f(x2 + 0.05f, y2 + 0.05f)}, {Vector3f(x2, y2 + 0.05f, z2 + 0.05f), Vector3f(x2, y2 + 0.05f)},
{Vector3f(x3, y3 + 0.05f, z3), Vector3f(z3, x3)}, {Vector3f(x3 + 0.05f, y3 + 0.05f, z3), Vector3f(z3, x3 + 0.05f)},
{Vector3f(x3 + 0.05f, y3 + 0.05f, z3 + 0.05f), Vector3f(z3 + 0.05f, x3 + 0.05f)}, {Vector3f(x3, y3 + 0.05f, z3 + 0.05f), Vector3f(z3 + 0.05f, x3)},
{Vector3f(x3, y3, z3), Vector3f(z3, x3)}, {Vector3f(x3 + 0.05f, y3, z3), Vector3f(z3, x3 + 0.05f)},
{Vector3f(x3 + 0.05f, y3, z3 + 0.05f), Vector3f(z3 + 0.05f, x3 + 0.05f)}, {Vector3f(x3, y3, z3 + 0.05f), Vector3f(z3 + 0.05f, x3)},
{Vector3f(x3, y3, z3 + 0.05f), Vector3f(z3 + 0.05f, y3)}, {Vector3f(x3, y3, z3), Vector3f(z3, y3)},
{Vector3f(x3, y3 + 0.05f, z3), Vector3f(z3, y3 + 0.05f)}, {Vector3f(x3, y3 + 0.05f, z3 + 0.05f), Vector3f(z3 + 0.05f, y3 + 0.05f)},
{Vector3f(x3 + 0.05f, y3, z3 + 0.05f), Vector3f(z3 + 0.05f, y3)}, {Vector3f(x3 + 0.05f, y3, z3), Vector3f(z3, y3)},
{Vector3f(x3 + 0.05f, y3 + 0.05f, z3), Vector3f(z3, y3 + 0.05f)}, {Vector3f(x3 + 0.05f, y3 + 0.05f, z3 + 0.05f), Vector3f(z3 + 0.05f, y3 + 0.05f)},
{Vector3f(x3, y3, z3), Vector3f(x3, y3)}, {Vector3f(x3 + 0.05f, y3, z3), Vector3f(x3 + 0.05f, y3)},
{Vector3f(x3 + 0.05f, y3 + 0.05f, z3), Vector3f(x3 + 0.05f, y3 + 0.05f)}, {Vector3f(x3, y3 + 0.05f, z3), Vector3f(x3, y3 + 0.05f)},
{Vector3f(x3, y3, z3 + 0.05f), Vector3f(x3, y3)}, {Vector3f(x3 + 0.05f, y3, z3 + 0.05f), Vector3f(x3 + 0.05f, y3)},
{Vector3f(x3 + 0.05f, y3 + 0.05f, z3 + 0.05f), Vector3f(x3 + 0.05f, y3 + 0.05f)}, {Vector3f(x3, y3 + 0.05f, z3 + 0.05f), Vector3f(x3, y3 + 0.05f)},
{Vector3f(x4, y4 + 0.05f, z4), Vector3f(z4, x4)}, {Vector3f(x4 + 0.05f, y4 + 0.05f, z4), Vector3f(z4, x4 + 0.05f)},
{Vector3f(x4 + 0.05f, y4 + 0.05f, z4 + 0.05f), Vector3f(z4 + 0.05f, x4 + 0.05f)}, {Vector3f(x4, y4 + 0.05f, z4 + 0.05f), Vector3f(z4 + 0.05f, x4)},
{Vector3f(x4, y4, z4), Vector3f(z4, x4)}, {Vector3f(x4 + 0.05f, y4, z4), Vector3f(z4, x4 + 0.05f)},
{Vector3f(x4 + 0.05f, y4, z4 + 0.05f), Vector3f(z4 + 0.05f, x4 + 0.05f)}, {Vector3f(x4, y4, z4 + 0.05f), Vector3f(z4 + 0.05f, x4)},
{Vector3f(x4, y4, z4 + 0.05f), Vector3f(z4 + 0.05f, y4)}, {Vector3f(x4, y4, z4), Vector3f(z4, y4)},
{Vector3f(x4, y4 + 0.05f, z4), Vector3f(z4, y4 + 0.05f)}, {Vector3f(x4, y4 + 0.05f, z4 + 0.05f), Vector3f(z4 + 0.05f, y4 + 0.05f)},
{Vector3f(x4 + 0.05f, y4, z4 + 0.05f), Vector3f(z4 + 0.05f, y4)}, {Vector3f(x4 + 0.05f, y4, z4), Vector3f(z4, y4)},
{Vector3f(x4 + 0.05f, y4 + 0.05f, z4), Vector3f(z4, y4 + 0.05f)}, {Vector3f(x4 + 0.05f, y4 + 0.05f, z4 + 0.05f), Vector3f(z4 + 0.05f, y4 + 0.05f)},
{Vector3f(x4, y4, z4), Vector3f(x4, y4)}, {Vector3f(x4 + 0.05f, y4, z4), Vector3f(x4 + 0.05f, y4)},
{Vector3f(x4 + 0.05f, y4 + 0.05f, z4), Vector3f(x4 + 0.05f, y4 + 0.05f)}, {Vector3f(x4, y4 + 0.05f, z4), Vector3f(x4, y4 + 0.05f)},
{Vector3f(x4, y4, z4 + 0.05f), Vector3f(x4, y4)}, {Vector3f(x4 + 0.05f, y4, z4 + 0.05f), Vector3f(x4 + 0.05f, y4)},
{Vector3f(x4 + 0.05f, y4 + 0.05f, z4 + 0.05f), Vector3f(x4 + 0.05f, y4 + 0.05f)}, {Vector3f(x4, y4 + 0.05f, z4 + 0.05f), Vector3f(x4, y4 + 0.05f)},
{Vector3f(x5, y5 + 0.05f, z5), Vector3f(z5, x5)}, {Vector3f(x5 + 0.05f, y5 + 0.05f, z5), Vector3f(z5, x5 + 0.05f)},
{Vector3f(x5 + 0.05f, y5 + 0.05f, z5 + 0.05f), Vector3f(z5 + 0.05f, x5 + 0.05f)}, {Vector3f(x5, y5 + 0.05f, z5 + 0.05f), Vector3f(z5 + 0.05f, x5)},
{Vector3f(x5, y5, z5), Vector3f(z5, x5)}, {Vector3f(x5 + 0.05f, y5, z5), Vector3f(z5, x5 + 0.05f)},
{Vector3f(x5 + 0.05f, y5, z5 + 0.05f), Vector3f(z5 + 0.05f, x5 + 0.05f)}, {Vector3f(x5, y5, z5 + 0.05f), Vector3f(z5 + 0.05f, x5)},
{Vector3f(x5, y5, z5 + 0.05f), Vector3f(z5 + 0.05f, y5)}, {Vector3f(x5, y5, z5), Vector3f(z5, y5)},
{Vector3f(x5, y5 + 0.05f, z5), Vector3f(z5, y5 + 0.05f)}, {Vector3f(x5, y5 + 0.05f, z5 + 0.05f), Vector3f(z5 + 0.05f, y5 + 0.05f)},
{Vector3f(x5 + 0.05f, y5, z5 + 0.05f), Vector3f(z5 + 0.05f, y5)}, {Vector3f(x5 + 0.05f, y5, z5), Vector3f(z5, y5)},
{Vector3f(x5 + 0.05f, y5 + 0.05f, z5), Vector3f(z5, y5 + 0.05f)}, {Vector3f(x5 + 0.05f, y5 + 0.05f, z5 + 0.05f), Vector3f(z5 + 0.05f, y5 + 0.05f)},
{Vector3f(x5, y5, z5), Vector3f(x5, y5)}, {Vector3f(x5 + 0.05f, y5, z5), Vector3f(x5 + 0.05f, y5)},
{Vector3f(x5 + 0.05f, y5 + 0.05f, z5), Vector3f(x5 + 0.05f, y5 + 0.05f)}, {Vector3f(x5, y5 + 0.05f, z5), Vector3f(x5, y5 + 0.05f)},
{Vector3f(x5, y5, z5 + 0.05f), Vector3f(x5, y5)}, {Vector3f(x5 + 0.05f, y5, z5 + 0.05f), Vector3f(x5 + 0.05f, y5)},
{Vector3f(x5 + 0.05f, y5 + 0.05f, z5 + 0.05f), Vector3f(x5 + 0.05f, y5 + 0.05f)}, {Vector3f(x5, y5 + 0.05f, z5 + 0.05f), Vector3f(x5, y5 + 0.05f)},
{Vector3f(x6, y6 + 0.05f, z6), Vector3f(z6, x6)}, {Vector3f(x6 + 0.05f, y6 + 0.05f, z6), Vector3f(z6, x6 + 0.05f)},
{Vector3f(x6 + 0.05f, y6 + 0.05f, z6 + 0.05f), Vector3f(z6 + 0.05f, x6 + 0.05f)}, {Vector3f(x6, y6 + 0.05f, z6 + 0.05f), Vector3f(z6 + 0.05f, x6)},
{Vector3f(x6, y6, z6), Vector3f(z6, x6)}, {Vector3f(x6 + 0.05f, y6, z6), Vector3f(z6, x6 + 0.05f)},
{Vector3f(x6 + 0.05f, y6, z6 + 0.05f), Vector3f(z6 + 0.05f, x6 + 0.05f)}, {Vector3f(x6, y6, z6 + 0.05f), Vector3f(z6 + 0.05f, x6)},
{Vector3f(x6, y6, z6 + 0.05f), Vector3f(z6 + 0.05f, y6)}, {Vector3f(x6, y6, z6), Vector3f(z6, y6)},
{Vector3f(x6, y6 + 0.05f, z6), Vector3f(z6, y6 + 0.05f)}, {Vector3f(x6, y6 + 0.05f, z6 + 0.05f), Vector3f(z6 + 0.05f, y6 + 0.05f)},
{Vector3f(x6 + 0.05f, y6, z6 + 0.05f), Vector3f(z6 + 0.05f, y6)}, {Vector3f(x6 + 0.05f, y6, z6), Vector3f(z6, y6)},
{Vector3f(x6 + 0.05f, y6 + 0.05f, z6), Vector3f(z6, y6 + 0.05f)}, {Vector3f(x6 + 0.05f, y6 + 0.05f, z6 + 0.05f), Vector3f(z6 + 0.05f, y6 + 0.05f)},
{Vector3f(x6, y6, z6), Vector3f(x6, y6)}, {Vector3f(x6 + 0.05f, y6, z6), Vector3f(x6 + 0.05f, y6)},
{Vector3f(x6 + 0.05f, y6 + 0.05f, z6), Vector3f(x6 + 0.05f, y6 + 0.05f)}, {Vector3f(x6, y6 + 0.05f, z6), Vector3f(x6, y6 + 0.05f)},
{Vector3f(x6, y6, z6 + 0.05f), Vector3f(x6, y6)}, {Vector3f(x6 + 0.05f, y6, z6 + 0.05f), Vector3f(x6 + 0.05f, y6)},
{Vector3f(x6 + 0.05f, y6 + 0.05f, z6 + 0.05f), Vector3f(x6 + 0.05f, y6 + 0.05f)}, {Vector3f(x6, y6 + 0.05f, z6 + 0.05f), Vector3f(x6, y6 + 0.05f)},
{Vector3f(x7, y7 + 0.05f, z7), Vector3f(z7, x7)}, {Vector3f(x7 + 0.05f, y7 + 0.05f, z7), Vector3f(z7, x7 + 0.05f)},
{Vector3f(x7 + 0.05f, y7 + 0.05f, z7 + 0.05f), Vector3f(z7 + 0.05f, x7 + 0.05f)}, {Vector3f(x7, y7 + 0.05f, z7 + 0.05f), Vector3f(z7 + 0.05f, x7)},
{Vector3f(x7, y7, z7), Vector3f(z7, x7)}, {Vector3f(x7 + 0.05f, y7, z7), Vector3f(z7, x7 + 0.05f)},
{Vector3f(x7 + 0.05f, y7, z7 + 0.05f), Vector3f(z7 + 0.05f, x7 + 0.05f)}, {Vector3f(x7, y7, z7 + 0.05f), Vector3f(z7 + 0.05f, x7)},
{Vector3f(x7, y7, z7 + 0.05f), Vector3f(z7 + 0.05f, y7)}, {Vector3f(x7, y7, z7), Vector3f(z7, y7)},
{Vector3f(x7, y7 + 0.05f, z7), Vector3f(z7, y7 + 0.05f)}, {Vector3f(x7, y7 + 0.05f, z7 + 0.05f), Vector3f(z7 + 0.05f, y7 + 0.05f)},
{Vector3f(x7 + 0.05f, y7, z7 + 0.05f), Vector3f(z7 + 0.05f, y7)}, {Vector3f(x7 + 0.05f, y7, z7), Vector3f(z7, y7)},
{Vector3f(x7 + 0.05f, y7 + 0.05f, z7), Vector3f(z7, y7 + 0.05f)}, {Vector3f(x7 + 0.05f, y7 + 0.05f, z7 + 0.05f), Vector3f(z7 + 0.05f, y7 + 0.05f)},
{Vector3f(x7, y7, z7), Vector3f(x7, y7)}, {Vector3f(x7 + 0.05f, y7, z7), Vector3f(x7 + 0.05f, y7)},
{Vector3f(x7 + 0.05f, y7 + 0.05f, z7), Vector3f(x7 + 0.05f, y7 + 0.05f)}, {Vector3f(x7, y7 + 0.05f, z7), Vector3f(x7, y7 + 0.05f)},
{Vector3f(x7, y7, z7 + 0.05f), Vector3f(x7, y7)}, {Vector3f(x7 + 0.05f, y7, z7 + 0.05f), Vector3f(x7 + 0.05f, y7)},
{Vector3f(x7 + 0.05f, y7 + 0.05f, z7 + 0.05f), Vector3f(x7 + 0.05f, y7 + 0.05f)}, {Vector3f(x7, y7 + 0.05f, z7 + 0.05f), Vector3f(x7, y7 + 0.05f)},
{Vector3f(x8, y8 + 0.05f, z8), Vector3f(z8, x8)}, {Vector3f(x8 + 0.05f, y8 + 0.05f, z8), Vector3f(z8, x8 + 0.05f)},
{Vector3f(x8 + 0.05f, y8 + 0.05f, z8 + 0.05f), Vector3f(z8 + 0.05f, x8 + 0.05f)}, {Vector3f(x8, y8 + 0.05f, z8 + 0.05f), Vector3f(z8 + 0.05f, x8)},
{Vector3f(x8, y8, z8), Vector3f(z8, x8)}, {Vector3f(x8 + 0.05f, y8, z8), Vector3f(z8, x8 + 0.05f)},
{Vector3f(x8 + 0.05f, y8, z8 + 0.05f), Vector3f(z8 + 0.05f, x8 + 0.05f)}, {Vector3f(x8, y8, z8 + 0.05f), Vector3f(z8 + 0.05f, x8)},
{Vector3f(x8, y8, z8 + 0.05f), Vector3f(z8 + 0.05f, y8)}, {Vector3f(x8, y8, z8), Vector3f(z8, y8)},
{Vector3f(x8, y8 + 0.05f, z8), Vector3f(z8, y8 + 0.05f)}, {Vector3f(x8, y8 + 0.05f, z8 + 0.05f), Vector3f(z8 + 0.05f, y8 + 0.05f)},
{Vector3f(x8 + 0.05f, y8, z8 + 0.05f), Vector3f(z8 + 0.05f, y8)}, {Vector3f(x8 + 0.05f, y8, z8), Vector3f(z8, y8)},
{Vector3f(x8 + 0.05f, y8 + 0.05f, z8), Vector3f(z8, y8 + 0.05f)}, {Vector3f(x8 + 0.05f, y8 + 0.05f, z8 + 0.05f), Vector3f(z8 + 0.05f, y8 + 0.05f)},
{Vector3f(x8, y8, z8), Vector3f(x8, y8)}, {Vector3f(x8 + 0.05f, y8, z8), Vector3f(x8 + 0.05f, y8)},
{Vector3f(x8 + 0.05f, y8 + 0.05f, z8), Vector3f(x8 + 0.05f, y8 + 0.05f)}, {Vector3f(x8, y8 + 0.05f, z8), Vector3f(x8, y8 + 0.05f)},
{Vector3f(x8, y8, z8 + 0.05f), Vector3f(x8, y8)}, {Vector3f(x8 + 0.05f, y8, z8 + 0.05f), Vector3f(x8 + 0.05f, y8)},
{Vector3f(x8 + 0.05f, y8 + 0.05f, z8 + 0.05f), Vector3f(x8 + 0.05f, y8 + 0.05f)}, {Vector3f(x8, y8 + 0.05f, z8 + 0.05f), Vector3f(x8, y8 + 0.05f)},
{Vector3f(x9, y9 + 0.05f, z9), Vector3f(z9, x9)}, {Vector3f(x9 + 0.05f, y9 + 0.05f, z9), Vector3f(z9, x9 + 0.05f)},
{Vector3f(x9 + 0.05f, y9 + 0.05f, z9 + 0.05f), Vector3f(z9 + 0.05f, x9 + 0.05f)}, {Vector3f(x9, y9 + 0.05f, z9 + 0.05f), Vector3f(z9 + 0.05f, x9)},
{Vector3f(x9, y9, z9), Vector3f(z9, x9)}, {Vector3f(x9 + 0.05f, y9, z9), Vector3f(z9, x9 + 0.05f)},
{Vector3f(x9 + 0.05f, y9, z9 + 0.05f), Vector3f(z9 + 0.05f, x9 + 0.05f)}, {Vector3f(x9, y9, z9 + 0.05f), Vector3f(z9 + 0.05f, x9)},
{Vector3f(x9, y9, z9 + 0.05f), Vector3f(z9 + 0.05f, y9)}, {Vector3f(x9, y9, z9), Vector3f(z9, y9)},
{Vector3f(x9, y9 + 0.05f, z9), Vector3f(z9, y9 + 0.05f)}, {Vector3f(x9, y9 + 0.05f, z9 + 0.05f), Vector3f(z9 + 0.05f, y9 + 0.05f)},
{Vector3f(x9 + 0.05f, y9, z9 + 0.05f), Vector3f(z9 + 0.05f, y9)}, {Vector3f(x9 + 0.05f, y9, z9), Vector3f(z9, y9)},
{Vector3f(x9 + 0.05f, y9 + 0.05f, z9), Vector3f(z9, y9 + 0.05f)}, {Vector3f(x9 + 0.05f, y9 + 0.05f, z9 + 0.05f), Vector3f(z9 + 0.05f, y9 + 0.05f)},
{Vector3f(x9, y9, z9), Vector3f(x9, y9)}, {Vector3f(x9 + 0.05f, y9, z9), Vector3f(x9 + 0.05f, y9)},
{Vector3f(x9 + 0.05f, y9 + 0.05f, z9), Vector3f(x9 + 0.05f, y9 + 0.05f)}, {Vector3f(x9, y9 + 0.05f, z9), Vector3f(x9, y9 + 0.05f)},
{Vector3f(x9, y9, z9 + 0.05f), Vector3f(x9, y9)}, {Vector3f(x9 + 0.05f, y9, z9 + 0.05f), Vector3f(x9 + 0.05f, y9)},
{Vector3f(x9 + 0.05f, y9 + 0.05f, z9 + 0.05f), Vector3f(x9 + 0.05f, y9 + 0.05f)}, {Vector3f(x9, y9 + 0.05f, z9 + 0.05f), Vector3f(x9, y9 + 0.05f)},
{Vector3f(x10, y10 + 0.05f, z10), Vector3f(z10, x10)}, {Vector3f(x10 + 0.05f, y10 + 0.05f, z10), Vector3f(z10, x10 + 0.05f)},
{Vector3f(x10 + 0.05f, y10 + 0.05f, z10 + 0.05f), Vector3f(z10 + 0.05f, x10 + 0.05f)}, {Vector3f(x10, y10 + 0.05f, z10 + 0.05f), Vector3f(z10 + 0.05f, x10)},
{Vector3f(x10, y10, z10), Vector3f(z10, x10)}, {Vector3f(x10 + 0.05f, y10, z10), Vector3f(z10, x10 + 0.05f)},
{Vector3f(x10 + 0.05f, y10, z10 + 0.05f), Vector3f(z10 + 0.05f, x10 + 0.05f)}, {Vector3f(x10, y10, z10 + 0.05f), Vector3f(z10 + 0.05f, x10)},
{Vector3f(x10, y10, z10 + 0.05f), Vector3f(z10 + 0.05f, y10)}, {Vector3f(x10, y10, z10), Vector3f(z10, y10)},
{Vector3f(x10, y10 + 0.05f, z10), Vector3f(z10, y10 + 0.05f)}, {Vector3f(x10, y10 + 0.05f, z10 + 0.05f), Vector3f(z10 + 0.05f, y10 + 0.05f)},
{Vector3f(x10 + 0.05f, y10, z10 + 0.05f), Vector3f(z10 + 0.05f, y10)}, {Vector3f(x10 + 0.05f, y10, z10), Vector3f(z10, y10)},
{Vector3f(x10 + 0.05f, y10 + 0.05f, z10), Vector3f(z10, y10 + 0.05f)}, {Vector3f(x10 + 0.05f, y10 + 0.05f, z10 + 0.05f), Vector3f(z10 + 0.05f, y10 + 0.05f)},
{Vector3f(x10, y10, z10), Vector3f(x10, y10)}, {Vector3f(x10 + 0.05f, y10, z10), Vector3f(x10 + 0.05f, y10)},
{Vector3f(x10 + 0.05f, y10 + 0.05f, z10), Vector3f(x10 + 0.05f, y10 + 0.05f)}, {Vector3f(x10, y10 + 0.05f, z10), Vector3f(x10, y10 + 0.05f)},
{Vector3f(x10, y10, z10 + 0.05f), Vector3f(x10, y10)}, {Vector3f(x10 + 0.05f, y10, z10 + 0.05f), Vector3f(x10 + 0.05f, y10)},
{Vector3f(x10 + 0.05f, y10 + 0.05f, z10 + 0.05f), Vector3f(x10 + 0.05f, y10 + 0.05f)}, {Vector3f(x10, y10 + 0.05f, z10 + 0.05f), Vector3f(x10, y10 + 0.05f)},
{Vector3f(x11, y11 + 0.05f, z11), Vector3f(z11, x11)}, {Vector3f(x11 + 0.05f, y11 + 0.05f, z11), Vector3f(z11, x11 + 0.05f)},
{Vector3f(x11 + 0.05f, y11 + 0.05f, z11 + 0.05f), Vector3f(z11 + 0.05f, x11 + 0.05f)}, {Vector3f(x11, y11 + 0.05f, z11 + 0.05f), Vector3f(z11 + 0.05f, x11)},
{Vector3f(x11, y11, z11), Vector3f(z11, x11)}, {Vector3f(x11 + 0.05f, y11, z11), Vector3f(z11, x11 + 0.05f)},
{Vector3f(x11 + 0.05f, y11, z11 + 0.05f), Vector3f(z11 + 0.05f, x11 + 0.05f)}, {Vector3f(x11, y11, z11 + 0.05f), Vector3f(z11 + 0.05f, x11)},
{Vector3f(x11, y11, z11 + 0.05f), Vector3f(z11 + 0.05f, y11)}, {Vector3f(x11, y11, z11), Vector3f(z11, y11)},
{Vector3f(x11, y11 + 0.05f, z11), Vector3f(z11, y11 + 0.05f)}, {Vector3f(x11, y11 + 0.05f, z11 + 0.05f), Vector3f(z11 + 0.05f, y11 + 0.05f)},
{Vector3f(x11 + 0.05f, y11, z11 + 0.05f), Vector3f(z11 + 0.05f, y11)}, {Vector3f(x11 + 0.05f, y11, z11), Vector3f(z11, y11)},
{Vector3f(x11 + 0.05f, y11 + 0.05f, z11), Vector3f(z11, y11 + 0.05f)}, {Vector3f(x11 + 0.05f, y11 + 0.05f, z11 + 0.05f), Vector3f(z11 + 0.05f, y11 + 0.05f)},
{Vector3f(x11, y11, z11), Vector3f(x11, y11)}, {Vector3f(x11 + 0.05f, y11, z11), Vector3f(x11 + 0.05f, y11)},
{Vector3f(x11 + 0.05f, y11 + 0.05f, z11), Vector3f(x11 + 0.05f, y11 + 0.05f)}, {Vector3f(x11, y11 + 0.05f, z11), Vector3f(x11, y11 + 0.05f)},
{Vector3f(x11, y11, z11 + 0.05f), Vector3f(x11, y11)}, {Vector3f(x11 + 0.05f, y11, z11 + 0.05f), Vector3f(x11 + 0.05f, y11)},
{Vector3f(x11 + 0.05f, y11 + 0.05f, z11 + 0.05f), Vector3f(x11 + 0.05f, y11 + 0.05f)}, {Vector3f(x11, y11 + 0.05f, z11 + 0.05f), Vector3f(x11, y11 + 0.05f)},
{Vector3f(x12, y12 + 0.05f, z12), Vector3f(z12, x12)}, {Vector3f(x12 + 0.05f, y12 + 0.05f, z12), Vector3f(z12, x12 + 0.05f)},
{Vector3f(x12 + 0.05f, y12 + 0.05f, z12 + 0.05f), Vector3f(z12 + 0.05f, x12 + 0.05f)}, {Vector3f(x12, y12 + 0.05f, z12 + 0.05f), Vector3f(z12 + 0.05f, x12)},
{Vector3f(x12, y12, z12), Vector3f(z12, x12)}, {Vector3f(x12 + 0.05f, y12, z12), Vector3f(z12, x12 + 0.05f)},
{Vector3f(x12 + 0.05f, y12, z12 + 0.05f), Vector3f(z12 + 0.05f, x12 + 0.05f)}, {Vector3f(x12, y12, z12 + 0.05f), Vector3f(z12 + 0.05f, x12)},
{Vector3f(x12, y12, z12 + 0.05f), Vector3f(z12 + 0.05f, y12)}, {Vector3f(x12, y12, z12), Vector3f(z12, y12)},
{Vector3f(x12, y12 + 0.05f, z12), Vector3f(z12, y12 + 0.05f)}, {Vector3f(x12, y12 + 0.05f, z12 + 0.05f), Vector3f(z12 + 0.05f, y12 + 0.05f)},
{Vector3f(x12 + 0.05f, y12, z12 + 0.05f), Vector3f(z12 + 0.05f, y12)}, {Vector3f(x12 + 0.05f, y12, z12), Vector3f(z12, y12)},
{Vector3f(x12 + 0.05f, y12 + 0.05f, z12), Vector3f(z12, y12 + 0.05f)}, {Vector3f(x12 + 0.05f, y12 + 0.05f, z12 + 0.05f), Vector3f(z12 + 0.05f, y12 + 0.05f)},
{Vector3f(x12, y12, z12), Vector3f(x12, y12)}, {Vector3f(x12 + 0.05f, y12, z12), Vector3f(x12 + 0.05f, y12)},
{Vector3f(x12 + 0.05f, y12 + 0.05f, z12), Vector3f(x12 + 0.05f, y12 + 0.05f)}, {Vector3f(x12, y12 + 0.05f, z12), Vector3f(x12, y12 + 0.05f)},
{Vector3f(x12, y12, z12 + 0.05f), Vector3f(x12, y12)}, {Vector3f(x12 + 0.05f, y12, z12 + 0.05f), Vector3f(x12 + 0.05f, y12)},
{Vector3f(x12 + 0.05f, y12 + 0.05f, z12 + 0.05f), Vector3f(x12 + 0.05f, y12 + 0.05f)}, {Vector3f(x12, y12 + 0.05f, z12 + 0.05f), Vector3f(x12, y12 + 0.05f)},
{Vector3f(x13, y13 + 0.05f, z13), Vector3f(z13, x13)}, {Vector3f(x13 + 0.05f, y13 + 0.05f, z13), Vector3f(z13, x13 + 0.05f)},
{Vector3f(x13 + 0.05f, y13 + 0.05f, z13 + 0.05f), Vector3f(z13 + 0.05f, x13 + 0.05f)}, {Vector3f(x13, y13 + 0.05f, z13 + 0.05f), Vector3f(z13 + 0.05f, x13)},
{Vector3f(x13, y13, z13), Vector3f(z13, x13)}, {Vector3f(x13 + 0.05f, y13, z13), Vector3f(z13, x13 + 0.05f)},
{Vector3f(x13 + 0.05f, y13, z13 + 0.05f), Vector3f(z13 + 0.05f, x13 + 0.05f)}, {Vector3f(x13, y13, z13 + 0.05f), Vector3f(z13 + 0.05f, x13)},
{Vector3f(x13, y13, z13 + 0.05f), Vector3f(z13 + 0.05f, y13)}, {Vector3f(x13, y13, z13), Vector3f(z13, y13)},
{Vector3f(x13, y13 + 0.05f, z13), Vector3f(z13, y13 + 0.05f)}, {Vector3f(x13, y13 + 0.05f, z13 + 0.05f), Vector3f(z13 + 0.05f, y13 + 0.05f)},
{Vector3f(x13 + 0.05f, y13, z13 + 0.05f), Vector3f(z13 + 0.05f, y13)}, {Vector3f(x13 + 0.05f, y13, z13), Vector3f(z13, y13)},
{Vector3f(x13 + 0.05f, y13 + 0.05f, z13), Vector3f(z13, y13 + 0.05f)}, {Vector3f(x13 + 0.05f, y13 + 0.05f, z13 + 0.05f), Vector3f(z13 + 0.05f, y13 + 0.05f)},
{Vector3f(x13, y13, z13), Vector3f(x13, y13)}, {Vector3f(x13 + 0.05f, y13, z13), Vector3f(x13 + 0.05f, y13)},
{Vector3f(x13 + 0.05f, y13 + 0.05f, z13), Vector3f(x13 + 0.05f, y13 + 0.05f)}, {Vector3f(x13, y13 + 0.05f, z13), Vector3f(x13, y13 + 0.05f)},
{Vector3f(x13, y13, z13 + 0.05f), Vector3f(x13, y13)}, {Vector3f(x13 + 0.05f, y13, z13 + 0.05f), Vector3f(x13 + 0.05f, y13)},
{Vector3f(x13 + 0.05f, y13 + 0.05f, z13 + 0.05f), Vector3f(x13 + 0.05f, y13 + 0.05f)}, {Vector3f(x13, y13 + 0.05f, z13 + 0.05f), Vector3f(x13, y13 + 0.05f)},
{Vector3f(x14, y14 + 0.05f, z14), Vector3f(z14, x14)}, {Vector3f(x14 + 0.05f, y14 + 0.05f, z14), Vector3f(z14, x14 + 0.05f)},
{Vector3f(x14 + 0.05f, y14 + 0.05f, z14 + 0.05f), Vector3f(z14 + 0.05f, x14 + 0.05f)}, {Vector3f(x14, y14 + 0.05f, z14 + 0.05f), Vector3f(z14 + 0.05f, x14)},
{Vector3f(x14, y14, z14), Vector3f(z14, x14)}, {Vector3f(x14 + 0.05f, y14, z14), Vector3f(z14, x14 + 0.05f)},
{Vector3f(x14 + 0.05f, y14, z14 + 0.05f), Vector3f(z14 + 0.05f, x14 + 0.05f)}, {Vector3f(x14, y14, z14 + 0.05f), Vector3f(z14 + 0.05f, x14)},
{Vector3f(x14, y14, z14 + 0.05f), Vector3f(z14 + 0.05f, y14)}, {Vector3f(x14, y14, z14), Vector3f(z14, y14)},
{Vector3f(x14, y14 + 0.05f, z14), Vector3f(z14, y14 + 0.05f)}, {Vector3f(x14, y14 + 0.05f, z14 + 0.05f), Vector3f(z14 + 0.05f, y14 + 0.05f)},
{Vector3f(x14 + 0.05f, y14, z14 + 0.05f), Vector3f(z14 + 0.05f, y14)}, {Vector3f(x14 + 0.05f, y14, z14), Vector3f(z14, y14)},
{Vector3f(x14 + 0.05f, y14 + 0.05f, z14), Vector3f(z14, y14 + 0.05f)}, {Vector3f(x14 + 0.05f, y14 + 0.05f, z14 + 0.05f), Vector3f(z14 + 0.05f, y14 + 0.05f)},
{Vector3f(x14, y14, z14), Vector3f(x14, y14)}, {Vector3f(x14 + 0.05f, y14, z14), Vector3f(x14 + 0.05f, y14)},
{Vector3f(x14 + 0.05f, y14 + 0.05f, z14), Vector3f(x14 + 0.05f, y14 + 0.05f)}, {Vector3f(x14, y14 + 0.05f, z14), Vector3f(x14, y14 + 0.05f)},
{Vector3f(x14, y14, z14 + 0.05f), Vector3f(x14, y14)}, {Vector3f(x14 + 0.05f, y14, z14 + 0.05f), Vector3f(x14 + 0.05f, y14)},
{Vector3f(x14 + 0.05f, y14 + 0.05f, z14 + 0.05f), Vector3f(x14 + 0.05f, y14 + 0.05f)}, {Vector3f(x14, y14 + 0.05f, z14 + 0.05f), Vector3f(x14, y14 + 0.05f)},
{Vector3f(x15, y15 + 0.05f, z15), Vector3f(z15, x15)}, {Vector3f(x15 + 0.05f, y15 + 0.05f, z15), Vector3f(z15, x15 + 0.05f)},
{Vector3f(x15 + 0.05f, y15 + 0.05f, z15 + 0.05f), Vector3f(z15 + 0.05f, x15 + 0.05f)}, {Vector3f(x15, y15 + 0.05f, z15 + 0.05f), Vector3f(z15 + 0.05f, x15)},
{Vector3f(x15, y15, z15), Vector3f(z15, x15)}, {Vector3f(x15 + 0.05f, y15, z15), Vector3f(z15, x15 + 0.05f)},
{Vector3f(x15 + 0.05f, y15, z15 + 0.05f), Vector3f(z15 + 0.05f, x15 + 0.05f)}, {Vector3f(x15, y15, z15 + 0.05f), Vector3f(z15 + 0.05f, x15)},
{Vector3f(x15, y15, z15 + 0.05f), Vector3f(z15 + 0.05f, y15)}, {Vector3f(x15, y15, z15), Vector3f(z15, y15)},
{Vector3f(x15, y15 + 0.05f, z15), Vector3f(z15, y15 + 0.05f)}, {Vector3f(x15, y15 + 0.05f, z15 + 0.05f), Vector3f(z15 + 0.05f, y15 + 0.05f)},
{Vector3f(x15 + 0.05f, y15, z15 + 0.05f), Vector3f(z15 + 0.05f, y15)}, {Vector3f(x15 + 0.05f, y15, z15), Vector3f(z15, y15)},
{Vector3f(x15 + 0.05f, y15 + 0.05f, z15), Vector3f(z15, y15 + 0.05f)}, {Vector3f(x15 + 0.05f, y15 + 0.05f, z15 + 0.05f), Vector3f(z15 + 0.05f, y15 + 0.05f)},
{Vector3f(x15, y15, z15), Vector3f(x15, y15)}, {Vector3f(x15 + 0.05f, y15, z15), Vector3f(x15 + 0.05f, y15)},
{Vector3f(x15 + 0.05f, y15 + 0.05f, z15), Vector3f(x15 + 0.05f, y15 + 0.05f)}, {Vector3f(x15, y15 + 0.05f, z15), Vector3f(x15, y15 + 0.05f)},
{Vector3f(x15, y15, z15 + 0.05f), Vector3f(x15, y15)}, {Vector3f(x15 + 0.05f, y15, z15 + 0.05f), Vector3f(x15 + 0.05f, y15)},
{Vector3f(x15 + 0.05f, y15 + 0.05f, z15 + 0.05f), Vector3f(x15 + 0.05f, y15 + 0.05f)}, {Vector3f(x15, y15 + 0.05f, z15 + 0.05f), Vector3f(x15, y15 + 0.05f)},
{Vector3f(x16, y16 + 0.05f, z16), Vector3f(z16, x16)}, {Vector3f(x16 + 0.05f, y16 + 0.05f, z16), Vector3f(z16, x16 + 0.05f)},
{Vector3f(x16 + 0.05f, y16 + 0.05f, z16 + 0.05f), Vector3f(z16 + 0.05f, x16 + 0.05f)}, {Vector3f(x16, y16 + 0.05f, z16 + 0.05f), Vector3f(z16 + 0.05f, x16)},
{Vector3f(x16, y16, z16), Vector3f(z16, x16)}, {Vector3f(x16 + 0.05f, y16, z16), Vector3f(z16, x16 + 0.05f)},
{Vector3f(x16 + 0.05f, y16, z16 + 0.05f), Vector3f(z16 + 0.05f, x16 + 0.05f)}, {Vector3f(x16, y16, z16 + 0.05f), Vector3f(z16 + 0.05f, x16)},
{Vector3f(x16, y16, z16 + 0.05f), Vector3f(z16 + 0.05f, y16)}, {Vector3f(x16, y16, z16), Vector3f(z16, y16)},
{Vector3f(x16, y16 + 0.05f, z16), Vector3f(z16, y16 + 0.05f)}, {Vector3f(x16, y16 + 0.05f, z16 + 0.05f), Vector3f(z16 + 0.05f, y16 + 0.05f)},
{Vector3f(x16 + 0.05f, y16, z16 + 0.05f), Vector3f(z16 + 0.05f, y16)}, {Vector3f(x16 + 0.05f, y16, z16), Vector3f(z16, y16)},
{Vector3f(x16 + 0.05f, y16 + 0.05f, z16), Vector3f(z16, y16 + 0.05f)}, {Vector3f(x16 + 0.05f, y16 + 0.05f, z16 + 0.05f), Vector3f(z16 + 0.05f, y16 + 0.05f)},
{Vector3f(x16, y16, z16), Vector3f(x16, y16)}, {Vector3f(x16 + 0.05f, y16, z16), Vector3f(x16 + 0.05f, y16)},
{Vector3f(x16 + 0.05f, y16 + 0.05f, z16), Vector3f(x16 + 0.05f, y16 + 0.05f)}, {Vector3f(x16, y16 + 0.05f, z16), Vector3f(x16, y16 + 0.05f)},
{Vector3f(x16, y16, z16 + 0.05f), Vector3f(x16, y16)}, {Vector3f(x16 + 0.05f, y16, z16 + 0.05f), Vector3f(x16 + 0.05f, y16)},
{Vector3f(x16 + 0.05f, y16 + 0.05f, z16 + 0.05f), Vector3f(x16 + 0.05f, y16 + 0.05f)}, {Vector3f(x16, y16 + 0.05f, z16 + 0.05f), Vector3f(x16, y16 + 0.05f)},
{Vector3f(x17, y17 + 0.05f, z17), Vector3f(z17, x17)}, {Vector3f(x17 + 0.05f, y17 + 0.05f, z17), Vector3f(z17, x17 + 0.05f)},
{Vector3f(x17 + 0.05f, y17 + 0.05f, z17 + 0.05f), Vector3f(z17 + 0.05f, x17 + 0.05f)}, {Vector3f(x17, y17 + 0.05f, z17 + 0.05f), Vector3f(z17 + 0.05f, x17)},
{Vector3f(x17, y17, z17), Vector3f(z17, x17)}, {Vector3f(x17 + 0.05f, y17, z17), Vector3f(z17, x17 + 0.05f)},
{Vector3f(x17 + 0.05f, y17, z17 + 0.05f), Vector3f(z17 + 0.05f, x17 + 0.05f)}, {Vector3f(x17, y17, z17 + 0.05f), Vector3f(z17 + 0.05f, x17)},
{Vector3f(x17, y17, z17 + 0.05f), Vector3f(z17 + 0.05f, y17)}, {Vector3f(x17, y17, z17), Vector3f(z17, y17)},
{Vector3f(x17, y17 + 0.05f, z17), Vector3f(z17, y17 + 0.05f)}, {Vector3f(x17, y17 + 0.05f, z17 + 0.05f), Vector3f(z17 + 0.05f, y17 + 0.05f)},
{Vector3f(x17 + 0.05f, y17, z17 + 0.05f), Vector3f(z17 + 0.05f, y17)}, {Vector3f(x17 + 0.05f, y17, z17), Vector3f(z17, y17)},
{Vector3f(x17 + 0.05f, y17 + 0.05f, z17), Vector3f(z17, y17 + 0.05f)}, {Vector3f(x17 + 0.05f, y17 + 0.05f, z17 + 0.05f), Vector3f(z17 + 0.05f, y17 + 0.05f)},
{Vector3f(x17, y17, z17), Vector3f(x17, y17)}, {Vector3f(x17 + 0.05f, y17, z17), Vector3f(x17 + 0.05f, y17)},
{Vector3f(x17 + 0.05f, y17 + 0.05f, z17), Vector3f(x17 + 0.05f, y17 + 0.05f)}, {Vector3f(x17, y17 + 0.05f, z17), Vector3f(x17, y17 + 0.05f)},
{Vector3f(x17, y17, z17 + 0.05f), Vector3f(x17, y17)}, {Vector3f(x17 + 0.05f, y17, z17 + 0.05f), Vector3f(x17 + 0.05f, y17)},
{Vector3f(x17 + 0.05f, y17 + 0.05f, z17 + 0.05f), Vector3f(x17 + 0.05f, y17 + 0.05f)}, {Vector3f(x17, y17 + 0.05f, z17 + 0.05f), Vector3f(x17, y17 + 0.05f)},
{Vector3f(x18, y18 + 0.05f, z18), Vector3f(z18, x18)}, {Vector3f(x18 + 0.05f, y18 + 0.05f, z18), Vector3f(z18, x18 + 0.05f)},
{Vector3f(x18 + 0.05f, y18 + 0.05f, z18 + 0.05f), Vector3f(z18 + 0.05f, x18 + 0.05f)}, {Vector3f(x18, y18 + 0.05f, z18 + 0.05f), Vector3f(z18 + 0.05f, x18)},
{Vector3f(x18, y18, z18), Vector3f(z18, x18)}, {Vector3f(x18 + 0.05f, y18, z18), Vector3f(z18, x18 + 0.05f)},
{Vector3f(x18 + 0.05f, y18, z18 + 0.05f), Vector3f(z18 + 0.05f, x18 + 0.05f)}, {Vector3f(x18, y18, z18 + 0.05f), Vector3f(z18 + 0.05f, x18)},
{Vector3f(x18, y18, z18 + 0.05f), Vector3f(z18 + 0.05f, y18)}, {Vector3f(x18, y18, z18), Vector3f(z18, y18)},
{Vector3f(x18, y18 + 0.05f, z18), Vector3f(z18, y18 + 0.05f)}, {Vector3f(x18, y18 + 0.05f, z18 + 0.05f), Vector3f(z18 + 0.05f, y18 + 0.05f)},
{Vector3f(x18 + 0.05f, y18, z18 + 0.05f), Vector3f(z18 + 0.05f, y18)}, {Vector3f(x18 + 0.05f, y18, z18), Vector3f(z18, y18)},
{Vector3f(x18 + 0.05f, y18 + 0.05f, z18), Vector3f(z18, y18 + 0.05f)}, {Vector3f(x18 + 0.05f, y18 + 0.05f, z18 + 0.05f), Vector3f(z18 + 0.05f, y18 + 0.05f)},
{Vector3f(x18, y18, z18), Vector3f(x18, y18)}, {Vector3f(x18 + 0.05f, y18, z18), Vector3f(x18 + 0.05f, y18)},
{Vector3f(x18 + 0.05f, y18 + 0.05f, z18), Vector3f(x18 + 0.05f, y18 + 0.05f)}, {Vector3f(x18, y18 + 0.05f, z18), Vector3f(x18, y18 + 0.05f)},
{Vector3f(x18, y18, z18 + 0.05f), Vector3f(x18, y18)}, {Vector3f(x18 + 0.05f, y18, z18 + 0.05f), Vector3f(x18 + 0.05f, y18)},
{Vector3f(x18 + 0.05f, y18 + 0.05f, z18 + 0.05f), Vector3f(x18 + 0.05f, y18 + 0.05f)}, {Vector3f(x18, y18 + 0.05f, z18 + 0.05f), Vector3f(x18, y18 + 0.05f)},
{Vector3f(x19, y19 + 0.05f, z19), Vector3f(z19, x19)}, {Vector3f(x19 + 0.05f, y19 + 0.05f, z19), Vector3f(z19, x19 + 0.05f)},
{Vector3f(x19 + 0.05f, y19 + 0.05f, z19 + 0.05f), Vector3f(z19 + 0.05f, x19 + 0.05f)}, {Vector3f(x19, y19 + 0.05f, z19 + 0.05f), Vector3f(z19 + 0.05f, x19)},
{Vector3f(x19, y19, z19), Vector3f(z19, x19)}, {Vector3f(x19 + 0.05f, y19, z19), Vector3f(z19, x19 + 0.05f)},
{Vector3f(x19 + 0.05f, y19, z19 + 0.05f), Vector3f(z19 + 0.05f, x19 + 0.05f)}, {Vector3f(x19, y19, z19 + 0.05f), Vector3f(z19 + 0.05f, x19)},
{Vector3f(x19, y19, z19 + 0.05f), Vector3f(z19 + 0.05f, y19)}, {Vector3f(x19, y19, z19), Vector3f(z19, y19)},
{Vector3f(x19, y19 + 0.05f, z19), Vector3f(z19, y19 + 0.05f)}, {Vector3f(x19, y19 + 0.05f, z19 + 0.05f), Vector3f(z19 + 0.05f, y19 + 0.05f)},
{Vector3f(x19 + 0.05f, y19, z19 + 0.05f), Vector3f(z19 + 0.05f, y19)}, {Vector3f(x19 + 0.05f, y19, z19), Vector3f(z19, y19)},
{Vector3f(x19 + 0.05f, y19 + 0.05f, z19), Vector3f(z19, y19 + 0.05f)}, {Vector3f(x19 + 0.05f, y19 + 0.05f, z19 + 0.05f), Vector3f(z19 + 0.05f, y19 + 0.05f)},
{Vector3f(x19, y19, z19), Vector3f(x19, y19)}, {Vector3f(x19 + 0.05f, y19, z19), Vector3f(x19 + 0.05f, y19)},
{Vector3f(x19 + 0.05f, y19 + 0.05f, z19), Vector3f(x19 + 0.05f, y19 + 0.05f)}, {Vector3f(x19, y19 + 0.05f, z19), Vector3f(x19, y19 + 0.05f)},
{Vector3f(x19, y19, z19 + 0.05f), Vector3f(x19, y19)}, {Vector3f(x19 + 0.05f, y19, z19 + 0.05f), Vector3f(x19 + 0.05f, y19)},
{Vector3f(x19 + 0.05f, y19 + 0.05f, z19 + 0.05f), Vector3f(x19 + 0.05f, y19 + 0.05f)}, {Vector3f(x19, y19 + 0.05f, z19 + 0.05f), Vector3f(x19, y19 + 0.05f)},
{Vector3f(x20, y20 + 0.05f, z20), Vector3f(z20, x20)}, {Vector3f(x20 + 0.05f, y20 + 0.05f, z20), Vector3f(z20, x20 + 0.05f)},
{Vector3f(x20 + 0.05f, y20 + 0.05f, z20 + 0.05f), Vector3f(z20 + 0.05f, x20 + 0.05f)}, {Vector3f(x20, y20 + 0.05f, z20 + 0.05f), Vector3f(z20 + 0.05f, x20)},
{Vector3f(x20, y20, z20), Vector3f(z20, x20)}, {Vector3f(x20 + 0.05f, y20, z20), Vector3f(z20, x20 + 0.05f)},
{Vector3f(x20 + 0.05f, y20, z20 + 0.05f), Vector3f(z20 + 0.05f, x20 + 0.05f)}, {Vector3f(x20, y20, z20 + 0.05f), Vector3f(z20 + 0.05f, x20)},
{Vector3f(x20, y20, z20 + 0.05f), Vector3f(z20 + 0.05f, y20)}, {Vector3f(x20, y20, z20), Vector3f(z20, y20)},
{Vector3f(x20, y20 + 0.05f, z20), Vector3f(z20, y20 + 0.05f)}, {Vector3f(x20, y20 + 0.05f, z20 + 0.05f), Vector3f(z20 + 0.05f, y20 + 0.05f)},
{Vector3f(x20 + 0.05f, y20, z20 + 0.05f), Vector3f(z20 + 0.05f, y20)}, {Vector3f(x20 + 0.05f, y20, z20), Vector3f(z20, y20)},
{Vector3f(x20 + 0.05f, y20 + 0.05f, z20), Vector3f(z20, y20 + 0.05f)}, {Vector3f(x20 + 0.05f, y20 + 0.05f, z20 + 0.05f), Vector3f(z20 + 0.05f, y20 + 0.05f)},
{Vector3f(x20, y20, z20), Vector3f(x20, y20)}, {Vector3f(x20 + 0.05f, y20, z20), Vector3f(x20 + 0.05f, y20)},
{Vector3f(x20 + 0.05f, y20 + 0.05f, z20), Vector3f(x20 + 0.05f, y20 + 0.05f)}, {Vector3f(x20, y20 + 0.05f, z20), Vector3f(x20, y20 + 0.05f)},
{Vector3f(x20, y20, z20 + 0.05f), Vector3f(x20, y20)}, {Vector3f(x20 + 0.05f, y20, z20 + 0.05f), Vector3f(x20 + 0.05f, y20)},
{Vector3f(x20 + 0.05f, y20 + 0.05f, z20 + 0.05f), Vector3f(x20 + 0.05f, y20 + 0.05f)}, {Vector3f(x20, y20 + 0.05f, z20 + 0.05f), Vector3f(x20, y20 + 0.05f)},
{Vector3f(x21, y21 + 0.05f, z21), Vector3f(z21, x21)}, {Vector3f(x21 + 0.05f, y21 + 0.05f, z21), Vector3f(z21, x21 + 0.05f)},
{Vector3f(x21 + 0.05f, y21 + 0.05f, z21 + 0.05f), Vector3f(z21 + 0.05f, x21 + 0.05f)}, {Vector3f(x21, y21 + 0.05f, z21 + 0.05f), Vector3f(z21 + 0.05f, x21)},
{Vector3f(x21, y21, z21), Vector3f(z21, x21)}, {Vector3f(x21 + 0.05f, y21, z21), Vector3f(z21, x21 + 0.05f)},
{Vector3f(x21 + 0.05f, y21, z21 + 0.05f), Vector3f(z21 + 0.05f, x21 + 0.05f)}, {Vector3f(x21, y21, z21 + 0.05f), Vector3f(z21 + 0.05f, x21)},
{Vector3f(x21, y21, z21 + 0.05f), Vector3f(z21 + 0.05f, y21)}, {Vector3f(x21, y21, z21), Vector3f(z21, y21)},
{Vector3f(x21, y21 + 0.05f, z21), Vector3f(z21, y21 + 0.05f)}, {Vector3f(x21, y21 + 0.05f, z21 + 0.05f), Vector3f(z21 + 0.05f, y21 + 0.05f)},
{Vector3f(x21 + 0.05f, y21, z21 + 0.05f), Vector3f(z21 + 0.05f, y21)}, {Vector3f(x21 + 0.05f, y21, z21), Vector3f(z21, y21)},
{Vector3f(x21 + 0.05f, y21 + 0.05f, z21), Vector3f(z21, y21 + 0.05f)}, {Vector3f(x21 + 0.05f, y21 + 0.05f, z21 + 0.05f), Vector3f(z21 + 0.05f, y21 + 0.05f)},
{Vector3f(x21, y21, z21), Vector3f(x21, y21)}, {Vector3f(x21 + 0.05f, y21, z21), Vector3f(x21 + 0.05f, y21)},
{Vector3f(x21 + 0.05f, y21 + 0.05f, z21), Vector3f(x21 + 0.05f, y21 + 0.05f)}, {Vector3f(x21, y21 + 0.05f, z21), Vector3f(x21, y21 + 0.05f)},
{Vector3f(x21, y21, z21 + 0.05f), Vector3f(x21, y21)}, {Vector3f(x21 + 0.05f, y21, z21 + 0.05f), Vector3f(x21 + 0.05f, y21)},
{Vector3f(x21 + 0.05f, y21 + 0.05f, z21 + 0.05f), Vector3f(x21 + 0.05f, y21 + 0.05f)}, {Vector3f(x21, y21 + 0.05f, z21 + 0.05f), Vector3f(x21, y21 + 0.05f)},
{Vector3f(x22, y22 + 0.05f, z22), Vector3f(z22, x22)}, {Vector3f(x22 + 0.05f, y22 + 0.05f, z22), Vector3f(z22, x22 + 0.05f)},
{Vector3f(x22 + 0.05f, y22 + 0.05f, z22 + 0.05f), Vector3f(z22 + 0.05f, x22 + 0.05f)}, {Vector3f(x22, y22 + 0.05f, z22 + 0.05f), Vector3f(z22 + 0.05f, x22)},
{Vector3f(x22, y22, z22), Vector3f(z22, x22)}, {Vector3f(x22 + 0.05f, y22, z22), Vector3f(z22, x22 + 0.05f)},
{Vector3f(x22 + 0.05f, y22, z22 + 0.05f), Vector3f(z22 + 0.05f, x22 + 0.05f)}, {Vector3f(x22, y22, z22 + 0.05f), Vector3f(z22 + 0.05f, x22)},
{Vector3f(x22, y22, z22 + 0.05f), Vector3f(z22 + 0.05f, y22)}, {Vector3f(x22, y22, z22), Vector3f(z22, y22)},
{Vector3f(x22, y22 + 0.05f, z22), Vector3f(z22, y22 + 0.05f)}, {Vector3f(x22, y22 + 0.05f, z22 + 0.05f), Vector3f(z22 + 0.05f, y22 + 0.05f)},
{Vector3f(x22 + 0.05f, y22, z22 + 0.05f), Vector3f(z22 + 0.05f, y22)}, {Vector3f(x22 + 0.05f, y22, z22), Vector3f(z22, y22)},
{Vector3f(x22 + 0.05f, y22 + 0.05f, z22), Vector3f(z22, y22 + 0.05f)}, {Vector3f(x22 + 0.05f, y22 + 0.05f, z22 + 0.05f), Vector3f(z22 + 0.05f, y22 + 0.05f)},
{Vector3f(x22, y22, z22), Vector3f(x22, y22)}, {Vector3f(x22 + 0.05f, y22, z22), Vector3f(x22 + 0.05f, y22)},
{Vector3f(x22 + 0.05f, y22 + 0.05f, z22), Vector3f(x22 + 0.05f, y22 + 0.05f)}, {Vector3f(x22, y22 + 0.05f, z22), Vector3f(x22, y22 + 0.05f)},
{Vector3f(x22, y22, z22 + 0.05f), Vector3f(x22, y22)}, {Vector3f(x22 + 0.05f, y22, z22 + 0.05f), Vector3f(x22 + 0.05f, y22)},
{Vector3f(x22 + 0.05f, y22 + 0.05f, z22 + 0.05f), Vector3f(x22 + 0.05f, y22 + 0.05f)}, {Vector3f(x22, y22 + 0.05f, z22 + 0.05f), Vector3f(x22, y22 + 0.05f)},
{Vector3f(x23, y23 + 0.05f, z23), Vector3f(z23, x23)}, {Vector3f(x23 + 0.05f, y23 + 0.05f, z23), Vector3f(z23, x23 + 0.05f)},
{Vector3f(x23 + 0.05f, y23 + 0.05f, z23 + 0.05f), Vector3f(z23 + 0.05f, x23 + 0.05f)}, {Vector3f(x23, y23 + 0.05f, z23 + 0.05f), Vector3f(z23 + 0.05f, x23)},
{Vector3f(x23, y23, z23), Vector3f(z23, x23)}, {Vector3f(x23 + 0.05f, y23, z23), Vector3f(z23, x23 + 0.05f)},
{Vector3f(x23 + 0.05f, y23, z23 + 0.05f), Vector3f(z23 + 0.05f, x23 + 0.05f)}, {Vector3f(x23, y23, z23 + 0.05f), Vector3f(z23 + 0.05f, x23)},
{Vector3f(x23, y23, z23 + 0.05f), Vector3f(z23 + 0.05f, y23)}, {Vector3f(x23, y23, z23), Vector3f(z23, y23)},
{Vector3f(x23, y23 + 0.05f, z23), Vector3f(z23, y23 + 0.05f)}, {Vector3f(x23, y23 + 0.05f, z23 + 0.05f), Vector3f(z23 + 0.05f, y23 + 0.05f)},
{Vector3f(x23 + 0.05f, y23, z23 + 0.05f), Vector3f(z23 + 0.05f, y23)}, {Vector3f(x23 + 0.05f, y23, z23), Vector3f(z23, y23)},
{Vector3f(x23 + 0.05f, y23 + 0.05f, z23), Vector3f(z23, y23 + 0.05f)}, {Vector3f(x23 + 0.05f, y23 + 0.05f, z23 + 0.05f), Vector3f(z23 + 0.05f, y23 + 0.05f)},
{Vector3f(x23, y23, z23), Vector3f(x23, y23)}, {Vector3f(x23 + 0.05f, y23, z23), Vector3f(x23 + 0.05f, y23)},
{Vector3f(x23 + 0.05f, y23 + 0.05f, z23), Vector3f(x23 + 0.05f, y23 + 0.05f)}, {Vector3f(x23, y23 + 0.05f, z23), Vector3f(x23, y23 + 0.05f)},
{Vector3f(x23, y23, z23 + 0.05f), Vector3f(x23, y23)}, {Vector3f(x23 + 0.05f, y23, z23 + 0.05f), Vector3f(x23 + 0.05f, y23)},
{Vector3f(x23 + 0.05f, y23 + 0.05f, z23 + 0.05f), Vector3f(x23 + 0.05f, y23 + 0.05f)}, {Vector3f(x23, y23 + 0.05f, z23 + 0.05f), Vector3f(x23, y23 + 0.05f)},
{Vector3f(x24, y24 + 0.05f, z24), Vector3f(z24, x24)}, {Vector3f(x24 + 0.05f, y24 + 0.05f, z24), Vector3f(z24, x24 + 0.05f)},
{Vector3f(x24 + 0.05f, y24 + 0.05f, z24 + 0.05f), Vector3f(z24 + 0.05f, x24 + 0.05f)}, {Vector3f(x24, y24 + 0.05f, z24 + 0.05f), Vector3f(z24 + 0.05f, x24)},
{Vector3f(x24, y24, z24), Vector3f(z24, x24)}, {Vector3f(x24 + 0.05f, y24, z24), Vector3f(z24, x24 + 0.05f)},
{Vector3f(x24 + 0.05f, y24, z24 + 0.05f), Vector3f(z24 + 0.05f, x24 + 0.05f)}, {Vector3f(x24, y24, z24 + 0.05f), Vector3f(z24 + 0.05f, x24)},
{Vector3f(x24, y24, z24 + 0.05f), Vector3f(z24 + 0.05f, y24)}, {Vector3f(x24, y24, z24), Vector3f(z24, y24)},
{Vector3f(x24, y24 + 0.05f, z24), Vector3f(z24, y24 + 0.05f)}, {Vector3f(x24, y24 + 0.05f, z24 + 0.05f), Vector3f(z24 + 0.05f, y24 + 0.05f)},
{Vector3f(x24 + 0.05f, y24, z24 + 0.05f), Vector3f(z24 + 0.05f, y24)}, {Vector3f(x24 + 0.05f, y24, z24), Vector3f(z24, y24)},
{Vector3f(x24 + 0.05f, y24 + 0.05f, z24), Vector3f(z24, y24 + 0.05f)}, {Vector3f(x24 + 0.05f, y24 + 0.05f, z24 + 0.05f), Vector3f(z24 + 0.05f, y24 + 0.05f)},
{Vector3f(x24, y24, z24), Vector3f(x24, y24)}, {Vector3f(x24 + 0.05f, y24, z24), Vector3f(x24 + 0.05f, y24)},
{Vector3f(x24 + 0.05f, y24 + 0.05f, z24), Vector3f(x24 + 0.05f, y24 + 0.05f)}, {Vector3f(x24, y24 + 0.05f, z24), Vector3f(x24, y24 + 0.05f)},
{Vector3f(x24, y24, z24 + 0.05f), Vector3f(x24, y24)}, {Vector3f(x24 + 0.05f, y24, z24 + 0.05f), Vector3f(x24 + 0.05f, y24)},
{Vector3f(x24 + 0.05f, y24 + 0.05f, z24 + 0.05f), Vector3f(x24 + 0.05f, y24 + 0.05f)}, {Vector3f(x24, y24 + 0.05f, z24 + 0.05f), Vector3f(x24, y24 + 0.05f)},
{Vector3f(x25, y25 + 0.05f, z25), Vector3f(z25, x25)}, {Vector3f(x25 + 0.05f, y25 + 0.05f, z25), Vector3f(z25, x25 + 0.05f)},
{Vector3f(x25 + 0.05f, y25 + 0.05f, z25 + 0.05f), Vector3f(z25 + 0.05f, x25 + 0.05f)}, {Vector3f(x25, y25 + 0.05f, z25 + 0.05f), Vector3f(z25 + 0.05f, x25)},
{Vector3f(x25, y25, z25), Vector3f(z25, x25)}, {Vector3f(x25 + 0.05f, y25, z25), Vector3f(z25, x25 + 0.05f)},
{Vector3f(x25 + 0.05f, y25, z25 + 0.05f), Vector3f(z25 + 0.05f, x25 + 0.05f)}, {Vector3f(x25, y25, z25 + 0.05f), Vector3f(z25 + 0.05f, x25)},
{Vector3f(x25, y25, z25 + 0.05f), Vector3f(z25 + 0.05f, y25)}, {Vector3f(x25, y25, z25), Vector3f(z25, y25)},
{Vector3f(x25, y25 + 0.05f, z25), Vector3f(z25, y25 + 0.05f)}, {Vector3f(x25, y25 + 0.05f, z25 + 0.05f), Vector3f(z25 + 0.05f, y25 + 0.05f)},
{Vector3f(x25 + 0.05f, y25, z25 + 0.05f), Vector3f(z25 + 0.05f, y25)}, {Vector3f(x25 + 0.05f, y25, z25), Vector3f(z25, y25)},
{Vector3f(x25 + 0.05f, y25 + 0.05f, z25), Vector3f(z25, y25 + 0.05f)}, {Vector3f(x25 + 0.05f, y25 + 0.05f, z25 + 0.05f), Vector3f(z25 + 0.05f, y25 + 0.05f)},
{Vector3f(x25, y25, z25), Vector3f(x25, y25)}, {Vector3f(x25 + 0.05f, y25, z25), Vector3f(x25 + 0.05f, y25)},
{Vector3f(x25 + 0.05f, y25 + 0.05f, z25), Vector3f(x25 + 0.05f, y25 + 0.05f)}, {Vector3f(x25, y25 + 0.05f, z25), Vector3f(x25, y25 + 0.05f)},
{Vector3f(x25, y25, z25 + 0.05f), Vector3f(x25, y25)}, {Vector3f(x25 + 0.05f, y25, z25 + 0.05f), Vector3f(x25 + 0.05f, y25)},
{Vector3f(x25 + 0.05f, y25 + 0.05f, z25 + 0.05f), Vector3f(x25 + 0.05f, y25 + 0.05f)}, {Vector3f(x25, y25 + 0.05f, z25 + 0.05f), Vector3f(x25, y25 + 0.05f)}
};

GLushort CubeIndices25[] =
{
0, 1, 3, 3, 1, 2,
5, 4, 6, 6, 4, 7,
8, 9, 11, 11, 9, 10,
13, 12, 14, 14, 12, 15,
16, 17, 19, 19, 17, 18,
21, 20, 22, 22, 20, 23,
24, 25, 27, 27, 25, 26,
29, 28, 30, 30, 28, 31,
32, 33, 35, 35, 33, 34,
37, 36, 38, 38, 36, 39,
40, 41, 43, 43, 41, 42,
45, 44, 46, 46, 44, 47,
48, 49, 51, 51, 49, 50,
53, 52, 54, 54, 52, 55,
56, 57, 59, 59, 57, 58,
61, 60, 62, 62, 60, 63,
64, 65, 67, 67, 65, 66,
69, 68, 70, 70, 68, 71,
72, 73, 75, 75, 73, 74,
77, 76, 78, 78, 76, 79,
80, 81, 83, 83, 81, 82,
85, 84, 86, 86, 84, 87,
88, 89, 91, 91, 89, 90,
93, 92, 94, 94, 92, 95,
96, 97, 99, 99, 97, 98,
101, 100, 102, 102, 100, 103,
104, 105, 107, 107, 105, 106,
109, 108, 110, 110, 108, 111,
112, 113, 115, 115, 113, 114,
117, 116, 118, 118, 116, 119,
120, 121, 123, 123, 121, 122,
125, 124, 126, 126, 124, 127,
128, 129, 131, 131, 129, 130,
133, 132, 134, 134, 132, 135,
136, 137, 139, 139, 137, 138,
141, 140, 142, 142, 140, 143,
144, 145, 147, 147, 145, 146,
149, 148, 150, 150, 148, 151,
152, 153, 155, 155, 153, 154,
157, 156, 158, 158, 156, 159,
160, 161, 163, 163, 161, 162,
165, 164, 166, 166, 164, 167,
168, 169, 171, 171, 169, 170,
173, 172, 174, 174, 172, 175,
176, 177, 179, 179, 177, 178,
181, 180, 182, 182, 180, 183,
184, 185, 187, 187, 185, 186,
189, 188, 190, 190, 188, 191,
192, 193, 195, 195, 193, 194,
197, 196, 198, 198, 196, 199,
200, 201, 203, 203, 201, 202,
205, 204, 206, 206, 204, 207,
208, 209, 211, 211, 209, 210,
213, 212, 214, 214, 212, 215,
216, 217, 219, 219, 217, 218,
221, 220, 222, 222, 220, 223,
224, 225, 227, 227, 225, 226,
229, 228, 230, 230, 228, 231,
232, 233, 235, 235, 233, 234,
237, 236, 238, 238, 236, 239,
240, 241, 243, 243, 241, 242,
245, 244, 246, 246, 244, 247,
248, 249, 251, 251, 249, 250,
253, 252, 254, 254, 252, 255,
256, 257, 259, 259, 257, 258,
261, 260, 262, 262, 260, 263,
264, 265, 267, 267, 265, 266,
269, 268, 270, 270, 268, 271,
272, 273, 275, 275, 273, 274,
277, 276, 278, 278, 276, 279,
280, 281, 283, 283, 281, 282,
285, 284, 286, 286, 284, 287,
288, 289, 291, 291, 289, 290,
293, 292, 294, 294, 292, 295,
296, 297, 299, 299, 297, 298,
301, 300, 302, 302, 300, 303,
304, 305, 307, 307, 305, 306,
309, 308, 310, 310, 308, 311,
312, 313, 315, 315, 313, 314,
317, 316, 318, 318, 316, 319,
320, 321, 323, 323, 321, 322,
325, 324, 326, 326, 324, 327,
328, 329, 331, 331, 329, 330,
333, 332, 334, 334, 332, 335,
336, 337, 339, 339, 337, 338,
341, 340, 342, 342, 340, 343,
344, 345, 347, 347, 345, 346,
349, 348, 350, 350, 348, 351,
352, 353, 355, 355, 353, 354,
357, 356, 358, 358, 356, 359,
360, 361, 363, 363, 361, 362,
365, 364, 366, 366, 364, 367,
368, 369, 371, 371, 369, 370,
373, 372, 374, 374, 372, 375,
376, 377, 379, 379, 377, 378,
381, 380, 382, 382, 380, 383,
384, 385, 387, 387, 385, 386,
389, 388, 390, 390, 388, 391,
392, 393, 395, 395, 393, 394,
397, 396, 398, 398, 396, 399,
400, 401, 403, 403, 401, 402,
405, 404, 406, 406, 404, 407,
408, 409, 411, 411, 409, 410,
413, 412, 414, 414, 412, 415,
416, 417, 419, 419, 417, 418,
421, 420, 422, 422, 420, 423,
424, 425, 427, 427, 425, 426,
429, 428, 430, 430, 428, 431,
432, 433, 435, 435, 433, 434,
437, 436, 438, 438, 436, 439,
440, 441, 443, 443, 441, 442,
445, 444, 446, 446, 444, 447,
448, 449, 451, 451, 449, 450,
453, 452, 454, 454, 452, 455,
456, 457, 459, 459, 457, 458,
461, 460, 462, 462, 460, 463,
464, 465, 467, 467, 465, 466,
469, 468, 470, 470, 468, 471,
472, 473, 475, 475, 473, 474,
477, 476, 478, 478, 476, 479,
480, 481, 483, 483, 481, 482,
485, 484, 486, 486, 484, 487,
488, 489, 491, 491, 489, 490,
493, 492, 494, 494, 492, 495,
496, 497, 499, 499, 497, 498,
501, 500, 502, 502, 500, 503,
504, 505, 507, 507, 505, 506,
509, 508, 510, 510, 508, 511,
512, 513, 515, 515, 513, 514,
517, 516, 518, 518, 516, 519,
520, 521, 523, 523, 521, 522,
525, 524, 526, 526, 524, 527,
528, 529, 531, 531, 529, 530,
533, 532, 534, 534, 532, 535,
536, 537, 539, 539, 537, 538,
541, 540, 542, 542, 540, 543,
544, 545, 547, 547, 545, 546,
549, 548, 550, 550, 548, 551,
552, 553, 555, 555, 553, 554,
557, 556, 558, 558, 556, 559,
560, 561, 563, 563, 561, 562,
565, 564, 566, 566, 564, 567,
568, 569, 571, 571, 569, 570,
573, 572, 574, 574, 572, 575,
576, 577, 579, 579, 577, 578,
581, 580, 582, 582, 580, 583,
584, 585, 587, 587, 585, 586,
589, 588, 590, 590, 588, 591,
592, 593, 595, 595, 593, 594,
597, 596, 598, 598, 596, 599
};

for (int i = 0; i < sizeof(CubeIndices25) / sizeof(CubeIndices25[0]); ++i)
AddIndex(CubeIndices25[i] + GLushort(numVertices));

// Generate a quad for each box face
//for (int v = 0; v < 6 * 4; v++)
for (int v = 0; v < 600; v++)
{
// Make vertices, with some token lighting
Vertex vvv; vvv.Pos = Vert[v][0]; vvv.U = Vert[v][1].x; vvv.V = Vert[v][1].y;
float dist1 = (vvv.Pos - Vector3f(-2, 4, -2)).Length();
float dist2 = (vvv.Pos - Vector3f(3, 4, -3)).Length();
float dist3 = (vvv.Pos - Vector3f(-4, 3, 25)).Length();
int   bri = rand() % 160;
float B = ((c >> 16) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
float G = ((c >> 8) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
float R = ((c >> 0) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
vvv.C = (c & 0xff000000) +
((R > 255 ? 255 : DWORD(R)) << 16) +
((G > 255 ? 255 : DWORD(G)) << 8) +
(B > 255 ? 255 : DWORD(B));
AddVertex(vvv);
}
}
void Render(Matrix4f view, Matrix4f proj)
{
Matrix4f combined = proj * view * GetMatrix();

glUseProgram(Fill->program);
glUniform1i(glGetUniformLocation(Fill->program, "Texture0"), 0);
glUniformMatrix4fv(glGetUniformLocation(Fill->program, "matWVP"), 1, GL_TRUE, (FLOAT*)&combined);

glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, Fill->texture->texId);

glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer->buffer);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->buffer);

GLuint posLoc = glGetAttribLocation(Fill->program, "Position");
GLuint colorLoc = glGetAttribLocation(Fill->program, "Color");
GLuint uvLoc = glGetAttribLocation(Fill->program, "TexCoord");

glEnableVertexAttribArray(posLoc);
glEnableVertexAttribArray(colorLoc);
glEnableVertexAttribArray(uvLoc);

glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, Pos));
glVertexAttribPointer(colorLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, C));
glVertexAttribPointer(uvLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, U));

glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, NULL);

glDisableVertexAttribArray(posLoc);
glDisableVertexAttribArray(colorLoc);
glDisableVertexAttribArray(uvLoc);

glBindBuffer(GL_ARRAY_BUFFER, 0);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

glUseProgram(0);
}

void AddSkeleton(const float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, float x4, float y4, float z4, float x5, float y5, float z5, float x6, float y6, float z6, float x7, float y7, float z7, float x8, float y8, float z8, float x9, float y9, float z9, float x10, float y10, float z10, float x11, float y11, float z11, float x12, float y12, float z12, float x13, float y13, float z13, float x14, float y14, float z14, float x15, float y15, float z15, float x16, float y16, float z16, float x17, float y17, float z17, float x18, float y18, float z18, float x19, float y19, float z19, float x20, float y20, float z20, float x21, float y21, float z21, float x22, float y22, float z22, float x23, float y23, float z23, float x24, float y24, float z24, float x25, float y25, float z25, DWORD c)
{
Vector3f Vert[][2] =
{
{Vector3f(x1, y1, z1)},
{Vector3f(x2, y2, z2)},
{Vector3f(x3, y3, z3)},
{Vector3f(x4, y4, z4)},
{Vector3f(x5, y5, z5)},
{Vector3f(x6, y6, z6)},
{Vector3f(x7, y7, z7)},
{Vector3f(x8, y8, z8)},
{Vector3f(x9, y9, z9)},
{Vector3f(x10, y10, z10)},
{Vector3f(x11, y11, z11)},
{Vector3f(x12, y12, z12)},
{Vector3f(x13, y13, z13)},
{Vector3f(x14, y14, z14)},
{Vector3f(x15, y15, z15)},
{Vector3f(x16, y16, z16)},
{Vector3f(x17, y17, z17)},
{Vector3f(x18, y18, z18)},
{Vector3f(x19, y19, z19)},
{Vector3f(x20, y20, z20)},
{Vector3f(x21, y21, z21)},
{Vector3f(x22, y22, z22)},
{Vector3f(x23, y23, z23)},
{Vector3f(x24, y24, z24)},
{Vector3f(x25, y25, z25)}
};

GLushort SkeletonIndices[] =
{
3, 2, 2, 20,
20, 4, 4, 5, 5, 6, 6, 22, 6, 7, 7, 21,
20, 8, 8, 9, 9, 10, 10, 24, 10, 11, 11, 23,
20, 1, 1, 0,
0, 12, 12, 13, 13, 14, 14, 15,
0, 16, 16, 17, 17, 18, 18, 19
};

for (int i = 0; i < sizeof(SkeletonIndices) / sizeof(SkeletonIndices[0]); ++i)
AddIndex(SkeletonIndices[i] + GLushort(numVertices));

// Generate a quad for each box face
for (int v = 0; v < 25; v++)
{
// Make vertices, with some token lighting
Vertex vvv; vvv.Pos = Vert[v][0]; vvv.U = Vert[v][1].x; vvv.V = Vert[v][1].y;
float dist1 = (vvv.Pos - Vector3f(-2, 4, -2)).Length();
float dist2 = (vvv.Pos - Vector3f(3, 4, -3)).Length();
float dist3 = (vvv.Pos - Vector3f(-4, 3, 25)).Length();
int   bri = rand() % 160;
float B = ((c >> 16) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
float G = ((c >> 8) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
float R = ((c >> 0) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
vvv.C = (c & 0xff000000) +
((R > 255 ? 255 : DWORD(R)) << 16) +
((G > 255 ? 255 : DWORD(G)) << 8) +
(B > 255 ? 255 : DWORD(B));
AddVertex(vvv);
}
}
void RenderLines(Matrix4f view, Matrix4f proj)
{
Matrix4f combined = proj * view * GetMatrix();

glUseProgram(Fill->program);
glUniformMatrix4fv(glGetUniformLocation(Fill->program, "matWVP"), 1, GL_TRUE, (FLOAT*)&combined);

glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer->buffer);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->buffer);

GLuint posLoc = glGetAttribLocation(Fill->program, "Position");
GLuint colorLoc = glGetAttribLocation(Fill->program, "Color");

glEnableVertexAttribArray(posLoc);
glEnableVertexAttribArray(colorLoc);

glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, Pos));
glVertexAttribPointer(colorLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, C));

glDrawElements(GL_LINES, numIndices, GL_UNSIGNED_SHORT, NULL);

glDisableVertexAttribArray(posLoc);
glDisableVertexAttribArray(colorLoc);

glBindBuffer(GL_ARRAY_BUFFER, 0);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

glUseProgram(0);
}
};


//-------------------------------------------------------------------------
struct Scene
{
int     numModels;
Model* Models[10];
vector<Mesh>    meshes;

void addModel(Model* n)
{
Models[numModels++] = n;
}

void Render(Matrix4f view, Matrix4f proj)
{
for (int i = 0; i < numModels; ++i)
Models[i]->Render(view, proj);
}
void RenderLines(Matrix4f view, Matrix4f proj)
{
for (int i = 0; i < numModels; ++i)
Models[i]->RenderLines(view, proj);
}
void RenderPoints(Matrix4f view, Matrix4f proj)
{
for (int i = 0; i < numModels; ++i)
Models[i]->RenderPoints(view, proj);
}
void Draw(Shader& shader)
{
for (unsigned int i = 0; i < meshes.size(); i++)
meshes[i].Draw(shader);
}

GLuint CreateShader(GLenum type, const GLchar* src)
{
GLuint shader = glCreateShader(type);

glShaderSource(shader, 1, &src, NULL);
glCompileShader(shader);

GLint r;
glGetShaderiv(shader, GL_COMPILE_STATUS, &r);
if (!r)
{
GLchar msg[1024];
glGetShaderInfoLog(shader, sizeof(msg), 0, msg);
if (msg[0]) {
OVR_DEBUG_LOG(("Compiling shader failed: %s\n", msg));
}
return 0;
}

return shader;
}

bool importChildNodes(aiNode* aiNodeParent)
{
if (!aiNodeParent) {
VALIDATE(false, "no valid parent node");
return false;
}

int iChildNumber = aiNodeParent->mNumChildren;
aiNode** aiNodeChilds = aiNodeParent->mChildren;

for (int i = 0; i < iChildNumber; ++i) {
aiNode* aiNodeChild = aiNodeChilds[i];

if (!aiNodeChild) {
VALIDATE(false, "no valid parent node");
return false;
}

aiString aiString = aiNodeChild->mName;

char buffer[100];
sprintf_s(buffer, "bone name: %s childs: %d\n", aiString.data, aiNodeChild->mNumChildren);
OutputDebugStringA(buffer);

if (!importChildNodes(aiNodeChild)) {
VALIDATE(false, "importChildNodes failed");
return false;
}
}

return true;
}

bool importBones(aiMesh* aiMesh, std::vector<Bones>& _vecBones)
{
if (!aiMesh) {
VALIDATE(false, "no valid mesh");
return false;
}

if (!aiMesh->HasBones()) {
return false;
}

int iBoneCount = aiMesh->mNumBones;

char buffer[100];
sprintf_s(buffer, "bone count: %d\n", iBoneCount);
OutputDebugStringA(buffer);

aiBone** aiBoneChilds = aiMesh->mBones;

for (int i = 0; i < iBoneCount; ++i) {
Bones bone;
aiBone* aiBoneChild = aiBoneChilds[i];

if (!aiBoneChild) {
VALIDATE(false, "no valid parent bone");
return false;
}

bone.mName = aiBoneChild->mName.C_Str();
bone.mOffsetMatrix = aiBoneChild->mOffsetMatrix;
bone.mNumWeights = aiBoneChild->mNumWeights;
bone.mWeights = aiBoneChild->mWeights;
bone.mNode = aiBoneChild->mNode;
/*aiNode** nodeChilds = bone.mNode->mChildren;

for (int j=0; j< nodeChilds)

aiNode* node = nodeChilds[0];*/

//char buffer[100];
//sprintf_s(buffer, "bone name: %s\n", aiString.data);
//OutputDebugStringA(buffer);

/*unsigned int iVertexID = 0;
ai_real fWeight = 0.0f;
for (int i = 0; i < bone.mNumWeights; ++i) {
iVertexID = bone.mWeights[i].mVertexId;
fWeight = bone.mWeights[i].mWeight;

char buffer[100];
sprintf_s(buffer, "vertexID:  %d weight: %f\n", iVertexID, fWeight);
OutputDebugStringA(buffer);
}*/
_vecBones.push_back(bone);
}
return true;
}

bool importModel(const std::string& sFile, std::vector<glm::vec3>& vecVec3Positions)
{
string directory;
Assimp::Importer importer;

const aiScene* scene = importer.ReadFile(sFile,
aiProcess_CalcTangentSpace |
aiProcess_Triangulate |
aiProcess_JoinIdenticalVertices |
aiProcess_SortByPType);

if (!scene) {
VALIDATE(false, "No valid scene.");
return false;
}

char buffer[100];
sprintf_s(buffer, "find assimp scene: %d models\n", scene->mNumMeshes);
OutputDebugStringA(buffer);

/*if (!importChildNodes(scene->mRootNode)) {
VALIDATE(false, "called importChildNodes() failed");
return false;
}*/

directory = sFile.substr(0, sFile.find_last_of('/'));

for (GLuint i = 0; i < scene->mNumMeshes; i++)
{
aiMesh* mesh = scene->mMeshes[i];

if (!mesh) {
VALIDATE(false, "No valid mesh.");
}

/*if (!importBones(mesh)) {
VALIDATE(false, "called importBones() failed");
return false;
}*/

if (mesh->HasPositions()) {
char buffer[100];
sprintf_s(buffer, "mesh has %d positions\n", mesh->mNumVertices);
OutputDebugStringA(buffer);

aiVector3D* vecVec3PPositions = mesh->mVertices;
if (!vecVec3PPositions) {
VALIDATE(false, "No valid mesh position data.");
}

for (int i = 0; i < mesh->mNumVertices; i++) {
aiVector3D vec3Position = vecVec3PPositions[i];
vecVec3Positions.push_back(glm::vec3(vec3Position.x, vec3Position.y, vec3Position.z));
}
}
else {
VALIDATE(false, "mesh has no positions.");
}
}

return true;
}

void Init(int includeIntensiveGPUobject)
{
static const GLchar* VertexShaderSrc =
"#version 150\n"
"uniform mat4 matWVP;\n"
"in      vec4 Position;\n"
"in      vec4 Color;\n"
"in      vec2 TexCoord;\n"
"out     vec2 oTexCoord;\n"
"out     vec4 oColor;\n"
"void main()\n"
"{\n"
"   gl_Position = (matWVP * Position);\n"
"   oTexCoord   = TexCoord;\n"
"   oColor.rgb  = pow(Color.rgb, vec3(2.2));\n"   // convert from sRGB to linear
"   oColor.a    = Color.a;\n"
"}\n";

static const char* FragmentShaderSrc =
"#version 150\n"
"uniform sampler2D Texture0;\n"
"in      vec4      oColor;\n"
"in      vec2      oTexCoord;\n"
"out     vec4      FragColor;\n"
"void main()\n"
"{\n"
"   FragColor = oColor * texture2D(Texture0, oTexCoord);\n"
"}\n";

GLuint    vshader = CreateShader(GL_VERTEX_SHADER, VertexShaderSrc);
GLuint    fshader = CreateShader(GL_FRAGMENT_SHADER, FragmentShaderSrc);

// Make textures
ShaderFill* grid_material[4] = {};
for (int k = 0; k < 4; ++k)
{
static DWORD tex_pixels[256 * 256];
for (int j = 0; j < 256; ++j)
{
for (int i = 0; i < 256; ++i)
{
if (k == 0) tex_pixels[j * 256 + i] = (((i >> 7) ^ (j >> 7)) & 1) ? 0xffb4b4b4 : 0xff505050;// floor
if (k == 1) tex_pixels[j * 256 + i] = (((j / 4 & 15) == 0) || (((i / 4 & 15) == 0) && ((((i / 4 & 31) == 0) ^ ((j / 4 >> 4) & 1)) == 0)))
? 0xff3c3c3c : 0xffb4b4b4;// wall
if (k == 2) tex_pixels[j * 256 + i] = (i / 4 == 0 || j / 4 == 0) ? 0xff505050 : 0xffb4b4b4;// ceiling
if (k == 3) tex_pixels[j * 256 + i] = 0xffffffff;// blank
}
}
TextureBuffer* generated_texture = new TextureBuffer(false, Sizei(256, 256), 4, (unsigned char*)tex_pixels);
grid_material[k] = new ShaderFill(vshader, fshader, generated_texture);
}

glDeleteShader(vshader);
glDeleteShader(fshader);

ifstream myfile("motionBothArms_Lars.txt");
if (!myfile.is_open())
{
cout << "Unable to open myfile";
//system("pause");
exit(1);
}

vector<glm::vec3> vecVec3Positions;

//if (!importModel("Male_Zombie/Zombie.glb", vecVec3Positions))
if (!importModel("Male_Zombie/head.obj", vecVec3Positions))
{
return;
}

// the imported model (head)
Model* m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

if (!importModel("Male_Zombie/neck.obj", vecVec3Positions))
{
return;
}

// the imported model (neck)
m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

if (!importModel("Male_Zombie/body.obj", vecVec3Positions))
{
return;
}

// the imported model (body)
m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

if (!importModel("Male_Zombie/hip.obj", vecVec3Positions))
{
return;
}

// the imported model (hip)
m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

if (!importModel("Male_Zombie/upperleg.obj", vecVec3Positions))
{
return;
}

// the imported model (upperleg)
m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

if (!importModel("Male_Zombie/lowerleg.obj", vecVec3Positions))
{
return;
}

// the imported model (lowerleg)
m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

if (!importModel("Male_Zombie/foot.obj", vecVec3Positions))
{
return;
}

// the imported model (foot)
m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

if (!importModel("Male_Zombie/upperarm.obj", vecVec3Positions))
{
return;
}

// the imported model (upperarm)
m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

if (!importModel("Male_Zombie/lowerarm.obj", vecVec3Positions))
{
return;
}

// the imported model (lowerarm)
m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

if (!importModel("Male_Zombie/hand.obj", vecVec3Positions))
{
return;
}

// the imported model (hand)
m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

if (!importModel("Male_Zombie/fingers.obj", vecVec3Positions))
{
return;
}

// the imported model (fingers)
m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

if (!importModel("Male_Zombie/thumb.obj", vecVec3Positions))
{
return;
}

// the imported model (thumb)
m = new Model(Vector3f(0, 0, 0), grid_material[2]);
for (int i = 0; i < vecVec3Positions.size(); ++i) {
m->addPoint(vecVec3Positions[i], 0xff552582);
}
m->AllocateBuffers();
addModel(m);

m = new Model(Vector3f(0, 0, 0), grid_material[1]);  // Walls
m->AddBox(-10.1f, 0.0f, -20.0f, -10.0f, 4.0f, 20.0f, 0xff808080); // Left Wall
m->AddBox(-10.0f, -0.1f, -20.1f, 10.0f, 4.0f, -20.0f, 0xff808080); // Back Wall
m->AddBox(10.0f, -0.1f, -20.0f, 10.1f, 4.0f, 20.0f, 0xff808080); // Right Wall
m->AllocateBuffers();
addModel(m);

if (includeIntensiveGPUobject)
{
m = new Model(Vector3f(0, 0, 0), grid_material[0]);  // Floors
for (float depth = 0.0f; depth > -3.0f; depth -= 0.1f)
m->AddBox(9.0f, 0.5f, -depth, -9.0f, 3.5f, -depth, 0x10ff80ff); // Partition
m->AllocateBuffers();
addModel(m);
}

m = new Model(Vector3f(0, 0, 0), grid_material[0]);  // Floors
//m->AddBox(-10.0f, -0.1f, -20.0f, 10.0f, 0.0f, 20.1f, 0xff808080); // Main floor
//m->AddBox(-15.0f, -6.1f, 18.0f, 15.0f, -6.0f, 30.0f, 0xff808080); // Bottom floor
m->AllocateBuffers();
addModel(m);

m = new Model(Vector3f(0, 0, 0), grid_material[2]);  // Ceiling
m->AddBox(-10.0f, 4.0f, -20.0f, 10.0f, 4.1f, 20.1f, 0xff808080);
m->AllocateBuffers();
addModel(m);

m = new Model(Vector3f(0, 0, 0), grid_material[3]);  // Fixtures & furniture
m->AddBox(9.5f, 0.75f, 3.0f, 10.1f, 2.5f, 3.1f, 0xff383838);   // Right side shelf// Verticals
m->AddBox(9.5f, 0.95f, 3.7f, 10.1f, 2.75f, 3.8f, 0xff383838);   // Right side shelf
m->AddBox(9.55f, 1.20f, 2.5f, 10.1f, 1.30f, 3.75f, 0xff383838); // Right side shelf// Horizontals
m->AddBox(9.55f, 2.00f, 3.05f, 10.1f, 2.10f, 4.2f, 0xff383838); // Right side shelf
m->AddBox(5.0f, 1.1f, 20.0f, 10.0f, 1.2f, 20.1f, 0xff383838);   // Right railing
m->AddBox(-10.0f, 1.1f, 20.0f, -5.0f, 1.2f, 20.1f, 0xff383838);   // Left railing
for (float f = 5.0f; f <= 9.0f; f += 1.0f)
{
m->AddBox(f, 0.0f, 20.0f, f + 0.1f, 1.1f, 20.1f, 0xff505050);// Left Bars
m->AddBox(-f, 1.1f, 20.0f, -f - 0.1f, 0.0f, 20.1f, 0xff505050);// Right Bars
}
/*m->AddBox(-1.8f, 0.8f, 1.0f, 0.0f, 0.7f, 0.0f, 0xff505000); // Table
m->AddBox(-1.8f, 0.0f, 0.0f, -1.7f, 0.7f, 0.1f, 0xff505000); // Table Leg
m->AddBox(-1.8f, 0.7f, 1.0f, -1.7f, 0.0f, 0.9f, 0xff505000); // Table Leg
m->AddBox(0.0f, 0.0f, 1.0f, -0.1f, 0.7f, 0.9f, 0xff505000); // Table Leg
m->AddBox(0.0f, 0.7f, 0.0f, -0.1f, 0.0f, 0.1f, 0xff505000); // Table Leg
m->AddBox(-1.4f, 0.5f, -1.1f, -0.8f, 0.55f, -0.5f, 0xff202050); // Chair Set
m->AddBox(-1.4f, 0.0f, -1.1f, -1.34f, 1.0f, -1.04f, 0xff202050); // Chair Leg 1
m->AddBox(-1.4f, 0.5f, -0.5f, -1.34f, 0.0f, -0.56f, 0xff202050); // Chair Leg 2
m->AddBox(-0.8f, 0.0f, -0.5f, -0.86f, 0.5f, -0.56f, 0xff202050); // Chair Leg 2
m->AddBox(-0.8f, 1.0f, -1.1f, -0.86f, 0.0f, -1.04f, 0xff202050); // Chair Leg 2
m->AddBox(-1.4f, 0.97f, -1.05f, -0.8f, 0.92f, -1.10f, 0xff202050); // Chair Back high bar*/

for (float f = 3.0f; f <= 6.6f; f += 0.4f)
m->AddBox(-3, 0.0f, f, -2.9f, 1.3f, f + 0.1f, 0xff404040); // Posts
m->AllocateBuffers();
addModel(m);

/*static int skeletonClock;
while (skeletonClock <= 10) //1066
{
vector<float> vect;
string temp;
seek_to_line(myfile, ++skeletonClock + 14); //Read data at line skeletonClock + 15
getline(myfile, temp);
stringstream ss(temp);
string buf;
while (ss >> buf)
vect.push_back(::stof(buf.c_str()));

m->AddBox25(vect[0], vect[1], vect[2], vect[3], vect[4], vect[5], vect[6], vect[7], vect[8], vect[9], vect[10], vect[11], vect[12], vect[13], vect[14], vect[15], vect[16], vect[17], vect[18], vect[19], vect[20], vect[21], vect[22], vect[23], vect[24], vect[25], vect[26], vect[27], vect[28], vect[29], vect[30], vect[31], vect[32], vect[33], vect[34], vect[35], vect[36], vect[37], vect[38], vect[39], vect[40], vect[41], vect[42], vect[43], vect[44], vect[45], vect[46], vect[47], vect[48], vect[49], vect[50], vect[51], vect[52], vect[53], vect[54], vect[55], vect[56], vect[57], vect[58], vect[59], vect[60], vect[61], vect[62], vect[63], vect[64], vect[65], vect[66], vect[67], vect[68], vect[69], vect[70], vect[71], vect[72], vect[73], vect[74], 0xff500000);
m->AllocateBuffers();
Add(m);
}*/
m->AddBox25(-0.115309f, 0.153283f, 2.54885f, -0.110507f, 0.467882f, 2.53528f, -0.104767f, 0.771282f, 2.50816f, -0.109963f, 0.920778f, 2.50027f, -0.310304f, 0.67454f, 2.51035f, -0.569052f, 0.731407f, 2.5307f, -0.747203f, 0.893473f, 2.48447f, -0.822246f, 0.947171f, 2.46095f, 0.086728f, 0.678407f, 2.54589f, 0.322395f, 0.710698f, 2.60683f, 0.558251f, 0.873975f, 2.57669f, 0.627528f, 0.921758f, 2.56421f, -0.195207f, 0.150241f, 2.50342f, -0.246323f, -0.257108f, 2.52936f, -0.264987f, -0.608919f, 2.57752f, -0.288185f, -0.69964f, 2.51592f, -0.031919f, 0.151915f, 2.51837f, 0.003236f, -0.245277f, 2.55707f, 0.026294f, -0.598516f, 2.63814f, 0.042509f, -0.692563f, 2.57974f, -0.106299f, 0.697082f, 2.51725f, -0.893856f, 0.98753f, 2.4282f, -0.84078f, 0.914722f, 2.462f, 0.709409f, 0.967994f, 2.56752f, 0.612942f, 0.923567f, 2.502f, 0xff500000);
//m->AddBox25(-0.111627f, 0.132741f, 2.55047f, -0.110303f, 0.45632f, 2.54037f, -0.108415f, 0.767295f, 2.51797f, -0.11094f, 0.918947f, 2.49887f, -0.291579f, 0.651342f, 2.50697f, -0.414845f, 0.401152f, 2.55766f, -0.465615f, 0.176591f, 2.45156f, -0.462083f, 0.113145f, 2.43146f, 0.077786f, 0.645957f, 2.53628f, 0.157761f, 0.376136f, 2.59783f, 0.227156f, 0.138311f, 2.55891f, 0.226286f, 0.079826f, 2.53629f, -0.193704f, 0.129932f, 2.50709f, -0.244439f, -0.262458f, 2.53318f, -0.264592f, -0.609974f, 2.5783f, -0.28752f, -0.700562f, 2.51635f, -0.02623f, 0.131467f, 2.51784f, 0.008341f, -0.252826f, 2.55952f, 0.02879f, -0.598299f, 2.64088f, 0.040088f, -0.69244f, 2.58032f, -0.108917f, 0.691213f, 2.52566f, -0.480792f, 0.012016f, 2.41654f, -0.484713f, 0.152369f, 2.44643f, 0.254225f, -0.020079f, 2.53627f, 0.203793f, 0.069199f, 2.53283f, 0xff202050);
m->AllocateBuffers();
addModel(m);

m->AddSkeleton(-0.115309f, 0.153283f, 2.54885f, -0.110507f, 0.467882f, 2.53528f, -0.104767f, 0.771282f, 2.50816f, -0.109963f, 0.920778f, 2.50027f, -0.310304f, 0.67454f, 2.51035f, -0.569052f, 0.731407f, 2.5307f, -0.747203f, 0.893473f, 2.48447f, -0.822246f, 0.947171f, 2.46095f, 0.086728f, 0.678407f, 2.54589f, 0.322395f, 0.710698f, 2.60683f, 0.558251f, 0.873975f, 2.57669f, 0.627528f, 0.921758f, 2.56421f, -0.195207f, 0.150241f, 2.50342f, -0.246323f, -0.257108f, 2.52936f, -0.264987f, -0.608919f, 2.57752f, -0.288185f, -0.69964f, 2.51592f, -0.031919f, 0.151915f, 2.51837f, 0.003236f, -0.245277f, 2.55707f, 0.026294f, -0.598516f, 2.63814f, 0.042509f, -0.692563f, 2.57974f, -0.106299f, 0.697082f, 2.51725f, -0.893856f, 0.98753f, 2.4282f, -0.84078f, 0.914722f, 2.462f, 0.709409f, 0.967994f, 2.56752f, 0.612942f, 0.923567f, 2.502f, 0xff505000);
//m->AddSkeleton(-0.111627f, 0.132741f, 2.55047f, -0.110303f, 0.45632f, 2.54037f, -0.108415f, 0.767295f, 2.51797f, -0.11094f, 0.918947f, 2.49887f, -0.291579f, 0.651342f, 2.50697f, -0.414845f, 0.401152f, 2.55766f, -0.465615f, 0.176591f, 2.45156f, -0.462083f, 0.113145f, 2.43146f, 0.077786f, 0.645957f, 2.53628f, 0.157761f, 0.376136f, 2.59783f, 0.227156f, 0.138311f, 2.55891f, 0.226286f, 0.079826f, 2.53629f, -0.193704f, 0.129932f, 2.50709f, -0.244439f, -0.262458f, 2.53318f, -0.264592f, -0.609974f, 2.5783f, -0.28752f, -0.700562f, 2.51635f, -0.02623f, 0.131467f, 2.51784f, 0.008341f, -0.252826f, 2.55952f, 0.02879f, -0.598299f, 2.64088f, 0.040088f, -0.69244f, 2.58032f, -0.108917f, 0.691213f, 2.52566f, -0.480792f, 0.012016f, 2.41654f, -0.484713f, 0.152369f, 2.44643f, 0.254225f, -0.020079f, 2.53627f, 0.203793f, 0.069199f, 2.53283f, 0xff202050);
m->AllocateBuffers();
addModel(m);
}

Scene() : numModels(0) {}
Scene(bool includeIntensiveGPUobject) :
numModels(0)
{
Init(includeIntensiveGPUobject);
}
void Release()
{
while (numModels-- > 0)
delete Models[numModels];
}
~Scene()
{
Release();
}
};

#endif // OVR_Win32_GLAppUtil_h
