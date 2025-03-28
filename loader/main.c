/* main.c -- Ducktales: Remastered .so loader
 *
 * Copyright (C) 2025 technicaljicama
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <taihen.h>
// #undef SceKernelPAVector
// #undef SceKernelAddrPair
// #undef SceKernelPaddrList
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/power.h>
#include <psp2/touch.h>
#include <kubridge.h>
#include <psp2/net/net.h>
#include <psp2/sysmodule.h> 

// #define WITH_SOUND //enables glitchy fmod sound support
// #define ANGLE //angle backend, works somewhat
#define PVR2
// #define PIB //just to test, will require cg shaders anyway

#include "etc1_utils.h"

#ifdef PVR2

#include <angle/GLES2/gl2.h>
#include <angle/GLES2/gl2ext.h>
#include <EGL/eglplatform.h>
#include <EGL/egl.h>
#include <gpu_es4/psp2_pvr_hint.h>

#elif defined(PIB)

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <pib.h>
#else
#include <vitaGL.h>
#endif

#include <stdbool.h>
#include <pthread.h>
#include <malloc.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "pthr.h"
#include <math.h>
#include "sfp.h"
#include <errno.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fmod.h>
#include <fmod_event.h>
#include <semaphore.h>
#include "main.h"
#include "config.h"
#include "dialog.h"
#include "so_util.h"

unsigned int sceLibcHeapSize = 64 * 1024 * 1024;
// unsigned int sceUserMainThreadStackSize  = 64 * 1024;
int _newlib_heap_size_user = 160 * 1024 * 1024; //110 and 32 sce perfect for PVR2
void* android_app;

so_module bc2_mod;

SceCtrlData pad;

//Phycont allocator made by juladdr (I-asked on github)
static SceUID g_myPhycontBlk = -1;
static void *g_myPhycont;
static SceClibMspace g_myMspace = NULL;
enum { k_myPhycontSize = 1024 * 1024 * 26, };

#define PHYCONT_BLK_NAME "my_phycont_blk"

#define IS_INSIDE_MY_PHYCONT(P) (((uintptr_t)(P)>=(uintptr_t)g_myPhycont)&&((uintptr_t)(P)<(k_myPhycontSize+(uintptr_t)g_myPhycont)))


void *my_malloc(size_t sz) {
  void *ptr;
  if ((ptr = malloc(sz)))
    return ptr;
  if (g_myMspace && (ptr = sceClibMspaceMalloc(g_myMspace, sz)))
    return ptr;

  return NULL;
}

void *my_calloc(size_t nu, size_t sz) {
  void *ptr;
  if ((ptr = calloc(nu, sz)))
    return ptr;
  if (g_myMspace && (ptr = sceClibMspaceCalloc(g_myMspace, nu, sz)))
    return ptr;

  return NULL;
}

void *my_memalign(size_t al, size_t sz) {
  void *ptr;
  if ((ptr = memalign(al, sz)))
    return ptr;
  if (g_myMspace && (ptr = sceClibMspaceMemalign(g_myMspace, al, sz)))
    return ptr;

  return NULL;
}

void *my_realloc(void *old, size_t sz) {
  if (IS_INSIDE_MY_PHYCONT(old)) {
    return sceClibMspaceRealloc(g_myMspace, old, sz);
  } else {
    return realloc(old, sz);
  }
}

void my_free(void *ptr) {
  if (IS_INSIDE_MY_PHYCONT(ptr)) {
    sceClibMspaceFree(g_myMspace, ptr);
  } else {
    free(ptr);
  }
}


void *__wrap_memcpy(void *dest, const void *src, size_t n) {
  return memcpy(dest, src, n);
}

void *__wrap_memmove(void *dest, const void *src, size_t n) {
  return memmove(dest, src, n);
}

void *__wrap_memset(void *s, int c, size_t n) {
  return memset(s, c, n);
}

int debugPrintf(char *text, ...) {

  va_list list;
  char string[512];

  va_start(list, text);
  vsprintf(string, text, list);
  va_end(list);

  SceUID fd = sceIoOpen("ux0:data/bc2_log.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
  if (fd >= 0) {
    sceIoWrite(fd, string, strlen(string));
    sceIoClose(fd);
  }

  return 0;
}


int ret0(void) {
  return 0;
}

int ret1(void) {
  return 1;
}

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
// #ifdef ENABLE_DEBUG
	va_list list;
	static char string[0x8000];

	va_start(list, fmt);
	vsprintf(string, fmt, list);
	va_end(list);

	sceClibPrintf("[LOG] %s: %s\n", tag, string);
// #endif
	return 0;
}

char *datadir(void) {
  printf("datadir called\n");
  return DATA_PATH;
}
#define ANDROID_TV //only working with 1.0.3
char *obb_path(void) {
  printf("obb called\n");
#ifdef ANDROID_TV
  return "ux0:data/duck/main.124.com.disney.ducktalesremastered_goo.obb";
#else
  return "ux0:data/duck/main.9.com.disney.ducktalesremastered_goo.obb";
#endif
}


int ret_lang() {
  printf("\nReturning fake language\n");
  return 0x0; //english
}


void pthread_exit_fake(void * __retval) {
  printf("pthread_exit\n");
  
  pthread_exit(__retval);
}

void test() {
  printf("called\n");
}

void internal_quit() {
  printf("internal_quit\n");
}

int fake_cpus() {
  printf("returning 1 cpu\n");
  
  return 1;
}

void SetPtrDesc(void *param_1,char *param_2) {
  printf("SetPtrDesc --> %s\n", param_2);
  return;
}


int ALooper_pollAll(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
  // printf("called alooper\n");
  *outData = NULL;
  return 0;
  
}
void nice_f(int pro) {
  printf("nice\n");
  return;
}
void* struct601() {
  return android_app;
}
// void set(void* p) {
//   printf("set");
//   sem_post_f(p);
// }
char *strncpy_f( char *dest, const char *src, size_t count ) {
  // printf("strncpy %s\n", src);
  return strncpy(dest, src, count);
}

int strlen_f(char *__dest)

{   
    return strlen(__dest);
}
int tolower_f( int src ) {
  // printf("towoer %d\n", src);
  return tolower(src);
}

#ifdef PVR2 //ANGLE incompatible shader cache
bool compiled = false;
#ifndef ANGLE
PFNGLGETPROGRAMBINARYOESPROC glGetProgramBinaryOES = NULL;
PFNGLPROGRAMBINARYOESPROC glProgramBinaryOES = NULL;
#endif
GLint loadProgramFromFile(const char* filename, GLuint program) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not open program binary file: %s\n", filename);
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    void* binary = malloc(fileSize);
    if (binary == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for program binary.\n");
        fclose(file);
        return -1;
    }

    fread(binary, 1, fileSize, file);
    fclose(file);

    GLenum binaryFormat;
    memcpy(&binaryFormat, binary, sizeof(binaryFormat));
    GLsizei length = (GLsizei)fileSize - sizeof(binaryFormat);

    glProgramBinaryOES(program, binaryFormat, (char *)binary + sizeof(binaryFormat), length);

    free(binary);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        fprintf(stderr, "Error: Program linking failed after loading from file: %s\n%s\n", filename, infoLog);
        return -1;
    }

    printf("Program loaded from file: %s\n", filename);
    return 0;
}
#ifndef ANGLE
void getOESExtensionFunctions() {
    glGetProgramBinaryOES = (PFNGLGETPROGRAMBINARYOESPROC)eglGetProcAddress("glGetProgramBinaryOES");
    glProgramBinaryOES = (PFNGLPROGRAMBINARYOESPROC)eglGetProcAddress("glProgramBinaryOES");

    if (!glGetProgramBinaryOES || !glProgramBinaryOES) {
        fprintf(stderr, "Error: Could not get OES extensions.\n");
        exit(-1);
    }
}
#endif
void dumpProgramToFile(const char* filename, GLuint program) {
    GLint binaryLength = 0;
    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH_OES, &binaryLength);

    if (binaryLength <= 0) {
        fprintf(stderr, "Error: Could not get program binary length.\n");
        return;
    }

    void* binary = malloc(binaryLength);
    if (binary == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for program binary.\n");
        return;
    }

    GLsizei length;
    GLenum binaryFormat;
    glGetProgramBinaryOES(program, binaryLength, &length, &binaryFormat, binary);

    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not create program binary file: %s\n", filename);
        free(binary);
        return;
    }
    
    fwrite(&binaryFormat, 1, sizeof(binaryFormat), file);
    fwrite(binary, 1, length, file);
    fclose(file);
    free(binary);

    printf("Program dumped to file: %s\n", filename);
}
#endif

GLuint glCreateShader_dbg(GLenum shaderType) {
    fprintf(stderr, "glCreateShader(%x)\n", shaderType);
    
    GLuint progr = glCreateShader(shaderType);

    return progr;
}

int shaders;
int hashme(const char *input) {
  int hashAddress = 5381;
  for (int counter = 0; input[counter]!='\0'&&input[counter]!='}'; counter++) {
      hashAddress = ((hashAddress << 5) + hashAddress) + input[counter];
  }
  return hashAddress;
}
char* shader1 = R"(

uniform float4x4 g_worldView;
uniform float4x4 g_projection;

void main(
	float4 xlat_attrib_POSITION : POSITION,
	float2 xlat_attrib_TEXCOORD : TEXCOORD,
	float2 out xlv_TEXCOORD0 : TEXCOORD0,
	float4 out gl_Position : POSITION
)
  {
  gl_Position = mul(g_projection, mul(g_worldView, xlat_attrib_POSITION));
  xlv_TEXCOORD0 = xlat_attrib_TEXCOORD;
  }
)";
char* shader2 = R"(
uniform float4 g_tint;
uniform float4 g_addColor;
uniform sampler2D g_textureSampler;
uniform sampler2D g_textureSamplerA;

void main(
	float2 xlv_TEXCOORD0 : TEXCOORD0,
	float4 out gl_FragColor : COLOR
)
  {
  float4 OutColor_1;
  float4 tmpvar_2;
  tmpvar_2 = tex2D(g_textureSampler,  xlv_TEXCOORD0);
  OutColor_1.xyz = tmpvar_2.xyz;
  float tmpvar_3;
  tmpvar_3 = tex2D(g_textureSamplerA,  xlv_TEXCOORD0).x;
  OutColor_1.w = tmpvar_3;
  float4 tmpvar_4;
  tmpvar_4 = (OutColor_1 * g_tint);
  OutColor_1 = tmpvar_4;
  float4 tmpvar_5;
  tmpvar_5 = (OutColor_1 + g_addColor);
  OutColor_1 = tmpvar_5;
  gl_FragColor = OutColor_1;
  }
)";

char* shader3 = R"(
uniform float4 g_tint;
uniform float4 g_addColor;
uniform sampler2D g_textureSampler;

void main(
	float2 xlv_TEXCOORD0 : TEXCOORD0,
	float4 out gl_FragColor : COLOR
) 
  {
  float4 OutColor_1;
  OutColor_1 = g_tint;
  float4 tmpvar_2;
  tmpvar_2 = tex2D(g_textureSampler,  xlv_TEXCOORD0);
  OutColor_1.w = (OutColor_1.w * tmpvar_2.x);
  float4 tmpvar_3;
  tmpvar_3 = (OutColor_1 + g_addColor);
  OutColor_1 = tmpvar_3;
  gl_FragColor = OutColor_1;
  }

)";

char* shader4 = R"(
uniform float4x4 g_worldView;
uniform float4x4 g_projection;
uniform float4 g_frameWidthHeight;

void main(
	float4 xlat_attrib_POSITION : POSITION,
	float2 xlat_attrib_TEXCOORD  : TEXCOORD,
	float2 out xlv_TEXCOORD0 : TEXCOORD0,
	float2 out xlv_TEXCOORD1 : TEXCOORD1,
	float2 out xlv_TEXCOORD3 : TEXCOORD3,
	float4 out gl_Position : POSITION
) 
  {
  float2 tmpvar_1;
  float2 tmpvar_2;
  float4 tmpvar_3;
  tmpvar_3 = mul(g_projection, mul(g_worldView, xlat_attrib_POSITION));
  tmpvar_1.x = (((tmpvar_3.x / tmpvar_3.w) + 1.0) / 2.0);
  tmpvar_1.y = (1.0 - ((
	(tmpvar_3.y / tmpvar_3.w)
   + 1.0) / 2.0));
  float2 tmpvar_4;
  tmpvar_4 = ((xlat_attrib_POSITION.xy - g_frameWidthHeight.xy) / g_frameWidthHeight.zw);
  tmpvar_2.x = tmpvar_4.x;
  tmpvar_2.y = (1.0 - tmpvar_4.y);
  gl_Position = tmpvar_3;
  xlv_TEXCOORD0 = xlat_attrib_TEXCOORD;
  xlv_TEXCOORD1 = tmpvar_1;
  xlv_TEXCOORD3 = tmpvar_2;
  }
)";

char* shader5 = R"(
// profile sce_fp_psp2

uniform float4 g_tint;
uniform float4 g_addColor;
uniform float4 g_UVExtents;
uniform float4 g_data0;
uniform sampler2D g_textureSampler;
uniform sampler2D g_textureSamplerA;
uniform sampler2D g_lightSampler;

void main(
	float2 xlv_TEXCOORD0 : TEXCOORD0,
	float2 xlv_TEXCOORD1 : TEXCOORD1,
	float2 xlv_TEXCOORD3 : TEXCOORD3,
	float4 out gl_FragColor : COLOR
) 
  {
  float4 blendedColor_1;
  float4 lightColor_2;
  float4 OutColor_3;
  float4 tmpvar_4;
  tmpvar_4.x = (xlv_TEXCOORD3.x < g_UVExtents.x);
  tmpvar_4.y = (xlv_TEXCOORD3.x > g_UVExtents.y);
  tmpvar_4.z = (xlv_TEXCOORD3.y < g_UVExtents.z);
  tmpvar_4.w = (xlv_TEXCOORD3.y > g_UVExtents.w);
  bool tmpvar_5;
  tmpvar_5 = any(tmpvar_4);
  if (tmpvar_5) {
	discard;
  };
  float4 tmpvar_6;
  tmpvar_6 = tex2D(g_textureSampler,  xlv_TEXCOORD0);
  OutColor_3.xyz = tmpvar_6.xyz;
  float tmpvar_7;
  tmpvar_7 = tex2D(g_textureSamplerA,  xlv_TEXCOORD0).x;
  OutColor_3.w = tmpvar_7;
  float4 tmpvar_8;
  tmpvar_8 = (OutColor_3 * g_tint);
  OutColor_3 = tmpvar_8;
  float4 tmpvar_9;
  tmpvar_9 = tex2D(g_lightSampler,  xlv_TEXCOORD1);
  lightColor_2 = tmpvar_9;
  float4 tmpvar_10;
  tmpvar_10 = lerp(float4(1.0, 1.0, 1.0, 1.0), lightColor_2, g_data0.xxxx);
  blendedColor_1 = tmpvar_10;
  float4 tmpvar_11;
  tmpvar_11 = (blendedColor_1 * OutColor_3);
  blendedColor_1 = tmpvar_11;
  OutColor_3.xyz = tmpvar_11.xyz;
  float4 tmpvar_12;
  tmpvar_12 = (OutColor_3 + g_addColor);
  OutColor_3 = tmpvar_12;
  gl_FragColor = OutColor_3;
  }
)";

char* shader6 = R"(
uniform float4x4 g_worldView;
uniform float4x4 g_projection;
uniform float4 g_ambientLight;
uniform float4 g_light0;
uniform float4 g_light0Data;
uniform float4 g_light0Data3;
uniform float4 g_light1;
uniform float4 g_light1Data;
uniform float4 g_light1Data3;
uniform float4 g_light2;
uniform float4 g_light2Data;
uniform float4 g_light2Data3;
uniform float4 g_light3;
uniform float4 g_light3Data;
uniform float4 g_light3Data3;
uniform bool g_enableLight0;
uniform bool g_enableLight1;
uniform bool g_enableLight2;
uniform bool g_enableLight3;
uniform float4 g_frameWidthHeight;

void main(
	float4 xlat_attrib_POSITION : POSITION,
	float2 xlat_attrib_TEXCOORD : TEXCOORD,
	float2 out xlv_TEXCOORD0 : TEXCOORD0,
	float4 out xlv_TEXCOORD2 : TEXCOORD2,
	float2 out xlv_TEXCOORD3 : TEXCOORD3,
	float4 out gl_Position : POSITION
)
  {
  float4 tmpvar_1;
  float2 tmpvar_2;
  float4 tmpvar_3;
  tmpvar_3 = mul(g_worldView, xlat_attrib_POSITION);
  float4 tmpvar_4;
  tmpvar_4 = mul(g_projection, tmpvar_3);
  float3 viewPos_5;
  viewPos_5 = tmpvar_3.xyz;
  float4 totalLightIntensity_6;
  totalLightIntensity_6 = g_ambientLight;
  if (g_enableLight0) {
	float4 lightData0_7;
	lightData0_7 = g_light0;
	float4 lightData1_8;
	lightData1_8 = g_light0Data;
	float4 lightData3_9;
	lightData3_9 = g_light0Data3;
	float lightIntensity_10;
	lightIntensity_10 = 0.0;
	if ((lightData0_7.w == 0.0)) {
	  float tmpvar_11;
	  float3 tmpvar_12;
	  tmpvar_12 = (lightData0_7.xyz - viewPos_5);
	  tmpvar_11 = sqrt(dot (tmpvar_12, tmpvar_12));
	  float tmpvar_13;
	  tmpvar_13 = (tmpvar_11 / lightData1_8.w);
	  lightIntensity_10 = ((1.0 - (
	    (tmpvar_13 * tmpvar_13)
	   * tmpvar_13)) * float((lightData1_8.w >= tmpvar_11)));
	} else {
	  float tmpvar_14;
	  tmpvar_14 = dot (lightData1_8.xyz, -(normalize(
	    (lightData0_7.xyz - viewPos_5)
	  )));
	  lightIntensity_10 = (((tmpvar_14 - lightData1_8.w) / (1.0 - lightData1_8.w)) * float((tmpvar_14 >= lightData1_8.w)));
	};
	totalLightIntensity_6.xyz = (totalLightIntensity_6.xyz + clamp ((lightData3_9.xyz * lightIntensity_10), 0.0, 1.0));
  };
  if (g_enableLight1) {
	float4 lightData0_15;
	lightData0_15 = g_light1;
	float4 lightData1_16;
	lightData1_16 = g_light1Data;
	float4 lightData3_17;
	lightData3_17 = g_light1Data3;
	float lightIntensity_18;
	lightIntensity_18 = 0.0;
	if ((lightData0_15.w == 0.0)) {
	  float tmpvar_19;
	  float3 tmpvar_20;
	  tmpvar_20 = (lightData0_15.xyz - viewPos_5);
	  tmpvar_19 = sqrt(dot (tmpvar_20, tmpvar_20));
	  float tmpvar_21;
	  tmpvar_21 = (tmpvar_19 / lightData1_16.w);
	  lightIntensity_18 = ((1.0 - (
	    (tmpvar_21 * tmpvar_21)
	   * tmpvar_21)) * float((lightData1_16.w >= tmpvar_19)));
	} else {
	  float tmpvar_22;
	  tmpvar_22 = dot (lightData1_16.xyz, -(normalize(
	    (lightData0_15.xyz - viewPos_5)
	  )));
	  lightIntensity_18 = (((tmpvar_22 - lightData1_16.w) / (1.0 - lightData1_16.w)) * float((tmpvar_22 >= lightData1_16.w)));
	};
	totalLightIntensity_6.xyz = (totalLightIntensity_6.xyz + clamp ((lightData3_17.xyz * lightIntensity_18), 0.0, 1.0));
  };
  if (g_enableLight2) {
	float4 lightData0_23;
	lightData0_23 = g_light2;
	float4 lightData1_24;
	lightData1_24 = g_light2Data;
	float4 lightData3_25;
	lightData3_25 = g_light2Data3;
	float lightIntensity_26;
	lightIntensity_26 = 0.0;
	if ((lightData0_23.w == 0.0)) {
	  float tmpvar_27;
	  float3 tmpvar_28;
	  tmpvar_28 = (lightData0_23.xyz - viewPos_5);
	  tmpvar_27 = sqrt(dot (tmpvar_28, tmpvar_28));
	  float tmpvar_29;
	  tmpvar_29 = (tmpvar_27 / lightData1_24.w);
	  lightIntensity_26 = ((1.0 - (
	    (tmpvar_29 * tmpvar_29)
	   * tmpvar_29)) * float((lightData1_24.w >= tmpvar_27)));
	} else {
	  float tmpvar_30;
	  tmpvar_30 = dot (lightData1_24.xyz, -(normalize(
	    (lightData0_23.xyz - viewPos_5)
	  )));
	  lightIntensity_26 = (((tmpvar_30 - lightData1_24.w) / (1.0 - lightData1_24.w)) * float((tmpvar_30 >= lightData1_24.w)));
	};
	totalLightIntensity_6.xyz = (totalLightIntensity_6.xyz + clamp ((lightData3_25.xyz * lightIntensity_26), 0.0, 1.0));
  };
  if (g_enableLight3) {
	float4 lightData0_31;
	lightData0_31 = g_light3;
	float4 lightData1_32;
	lightData1_32 = g_light3Data;
	float4 lightData3_33;
	lightData3_33 = g_light3Data3;
	float lightIntensity_34;
	lightIntensity_34 = 0.0;
	if ((lightData0_31.w == 0.0)) {
	  float tmpvar_35;
	  float3 tmpvar_36;
	  tmpvar_36 = (lightData0_31.xyz - viewPos_5);
	  tmpvar_35 = sqrt(dot (tmpvar_36, tmpvar_36));
	  float tmpvar_37;
	  tmpvar_37 = (tmpvar_35 / lightData1_32.w);
	  lightIntensity_34 = ((1.0 - (
	    (tmpvar_37 * tmpvar_37)
	   * tmpvar_37)) * float((lightData1_32.w >= tmpvar_35)));
	} else {
	  float tmpvar_38;
	  tmpvar_38 = dot (lightData1_32.xyz, -(normalize(
	    (lightData0_31.xyz - viewPos_5)
	  )));
	  lightIntensity_34 = (((tmpvar_38 - lightData1_32.w) / (1.0 - lightData1_32.w)) * float((tmpvar_38 >= lightData1_32.w)));
	};
	totalLightIntensity_6.xyz = (totalLightIntensity_6.xyz + clamp ((lightData3_33.xyz * lightIntensity_34), 0.0, 1.0));
  };
  float tmpvar_39;
  tmpvar_39 = clamp (((
	clamp (totalLightIntensity_6, 0.0, 1.0)
  .x - 0.4) / 0.6), 0.0, 1.0);
  float3 tmpvar_40;
  tmpvar_40 = float3((tmpvar_39 * (tmpvar_39 * (3.0 -
	(2.0 * tmpvar_39)
  ))));
  tmpvar_1.xyz = tmpvar_40;
  tmpvar_1.w = 0.0;
  float2 tmpvar_41;
  tmpvar_41 = ((xlat_attrib_POSITION.xy - g_frameWidthHeight.xy) / g_frameWidthHeight.zw);
  tmpvar_2.x = tmpvar_41.x;
  tmpvar_2.y = (1.0 - tmpvar_41.y);
  gl_Position = tmpvar_4;
  xlv_TEXCOORD0 = xlat_attrib_TEXCOORD;
  xlv_TEXCOORD2 = tmpvar_1;
  xlv_TEXCOORD3 = tmpvar_2;
  }
)";
/*
 vec4 fcolor;
vec4 fchannels = texture2D(tex, tc);
fcolor  = texture2D(srctex, vec2(fchannels.r, 0.125));
fcolor += texture2D(srctex, vec2(fchannels.g, 0.375));
fcolor += texture2D(srctex, vec2(fchannels.b, 0.625));
fcolor += texture2D(srctex, vec2(fchannels.a, 0.875));
gl_FragColor = fcolor;

 */
char* lastShader = R"(
precision mediump float;
uniform sampler2D srctex;
uniform sampler2D tex;
varying vec2 tc;
void main() {

}
)";
/*
 * vec4 fcolor;
vec4 fcolor_original = texture2D(tex[int(0.0)], tc0);
vec4 fcolor_source   = texture2D(tex[int(1.0)], tc1);
fcolor = (fcolor_original) * ( cxmul) + (fcolor_source) * ( cxmul1);
gl_FragColor = fcolor;
*/
char* errshader = R"(
precision mediump float;
uniform mat4 cxmul;
uniform mat4 cxmul1;
uniform sampler2D tex[2];
varying vec2 tc0;
varying vec2 tc1;
void main() {

}
)";

/*vec4 fcolor;
vec4 fcolor_org = texture2D(tex[int(0.0)], tc0);
vec4 fcolor_src = texture2D(tex[int(1.0)], tc1);
vec4 fcolor_alp = texture2D(tex[int(2.0)], tc2);
float inAlpha = fcolor_src.a * fcolor_alp.a;
fcolor.a = 1.0;
fcolor.rgb = mix(fcolor_org.rgb, fcolor_src.rgb, inAlpha / fcolor.a);
gl_FragColor = fcolor;*/

char* errshader2 = R"(
precision mediump float;
uniform sampler2D tex[3];
varying vec2 tc0;
varying vec2 tc1;
varying vec2 tc2;
void main() {

}

)";

char* lowshader = R"(
precision lowp float;
uniform highp vec4 g_tint;
uniform highp vec4 g_addColor;
uniform sampler2D g_textureSampler;
uniform sampler2D g_textureSamplerA;
varying highp vec2 xlv_TEXCOORD0;
varying highp vec4 xlv_TEXCOORD2;
void main ()
{
  mediump vec4 OutColor_1;
  lowp vec4 tmpvar_2;
  tmpvar_2 = texture2D (g_textureSampler, xlv_TEXCOORD0);
  OutColor_1.xyz = tmpvar_2.xyz;
  lowp float tmpvar_3;
  tmpvar_3 = texture2D (g_textureSamplerA, xlv_TEXCOORD0).x;
  OutColor_1.w = tmpvar_3;
  highp vec4 tmpvar_4;
  tmpvar_4 = (OutColor_1 * g_tint);
  OutColor_1 = tmpvar_4;
  highp vec3 tmpvar_5;
  tmpvar_5 = (OutColor_1.xyz * xlv_TEXCOORD2.xyz);
  OutColor_1.xyz = tmpvar_5;
  highp vec4 tmpvar_6;
  tmpvar_6 = (OutColor_1 + g_addColor);
  OutColor_1 = tmpvar_6;
  gl_FragData[0] = OutColor_1;
}
)";
bool ds = true;
int shader_name = 0;
int num_shader = 0;
void glDeleteTextures_dbg(GLsizei n, const GLuint* textures) {
   fprintf(stderr, "[PVRDBG] glDeleteTextures(%d, %x)\n", n, textures);
   if(textures != NULL)
     glDeleteTextures(n, textures);
}
void glShaderSource_dbg(GLuint handle, GLsizei count, const GLchar *const *string, const GLint *length) {
    /*
    const GLchar *modifiedStrings = (const GLchar *)malloc(count * sizeof(GLchar *));
    if (modifiedStrings == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }*/
    // shaders++;
char* tmp;
char* shader;
int new_len;
int h = hashme(string[0]);
#ifndef ANGLE
    char buffer[50];
    sprintf(buffer, "ux0:data/duck/cache/%0x.bin", abs(h));
    FILE* file = fopen(buffer, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error2: Could not open program binary file: %s\n", buffer);
        compiled = false;
        shader_name = abs(h);
        // return progr;
    } else {
        fclose(file);
        
          compiled = true;
        shader_name = abs(h);
    }
#endif/*
        if(h == 1046176405) {
        glShaderSource(handle, count, &lastShader, length);
        return;
      }
      if(h == -1314509143) {
        glShaderSource(handle, count, &errshader, length);
        return;
      }
      if(h == -2059046731) {
        glShaderSource(handle, count, &errshader2, length);
        return;
      }*/
if(ds) {
    // Perform replacement on each string
    for (GLsizei i = 0; i < count; ++i) {
#ifndef PVR2
      // modifiedStrings[i] = str_replace(string[i], "gl_FragData[0]", "gl_FragColor");
#endif
      // tmp = str_replace(string[i], "highp", "mediump");
      // tmp = str_replace(tmp, "mediump", "lowp");
      // shaders++;
      // string[i] = tmp;
      int h = hashme(string[i]);
      char* low = "precision mediump float;\n";

      // strcat(shader,low);
      // strcat(shader,string[i]);
      int length2, j;

      length2 = length[i];
      int s2l = strlen(low);
      shader = malloc(s2l+length2+2);
      memset(shader, 0, s2l+length2+2);
      for (j = 0; low[j] != '\0'; ++j) {
        shader[j] = low[j];
      }
      printf("%d\n", length2);
      for (j = 0; j < length2; ++j) {
        shader[s2l+j] = string[i][j];
      }
      // shader[s2l+length2+1] = '}';
      // shader[s2l+length2+1] = '\0';
      new_len = s2l+length2;
#ifdef PVR2

      if(h == -1276739000)
        ds = false;
#endif
#ifndef PVR2
      if(h == 605637737) {
        glShaderSource(handle, count, &shader1, length);
        return;
      } else if(h == 235212001) {
        glShaderSource(handle, count, &shader2, length);
        return;
      } else if(h == -1087641176) {
        glShaderSource(handle, count, &shader3, length);
        return;
      }else if(h == -315174830) {
        glShaderSource(handle, count, &shader4, length);
        return;
      }else if(h == -426919008) {
        glShaderSource(handle, count, &shader5, length);
        return;
      }else if(h == -657363402) {
        glShaderSource(handle, count, &shader6, length);
        return;
      } else {
        glShaderSource(handle, count, string, length);
        return;
      }
#endif
      // printf("%s %d\n", shader, h);
    }

    // char* s = str_replace(string[0], "gl_FragData[0]", "gl_FragColor");
    // const char str[1] = {s};
    // fprintf(stderr, "[PVRDBG] glShaderSource(%x, %x, data, [data])\n", handle, count);
    // printf("%s\n", string[0]);
     GLint lengths[]       = { new_len };
    glShaderSource(handle, count, &shader, lengths);
     free(shader);
} else {
  glShaderSource(handle, count, string, length);
}
    return;
}


#if 0
void glShaderSource_dbg(GLuint handle, GLsizei count, const GLchar *const *string, const GLint *length) {
    /*
    const GLchar *modifiedStrings = (const GLchar *)malloc(count * sizeof(GLchar *));
    if (modifiedStrings == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }*/
    // shaders++;
char* tmp;
char* shader;
int new_len;
    // Perform replacement on each string
    for (GLsizei i = 0; i < count; ++i) {
#ifndef PVR2
      // modifiedStrings[i] = str_replace(string[i], "gl_FragData[0]", "gl_FragColor");
#endif
      // tmp = str_replace(string[i], "highp", "mediump");
      // tmp = str_replace(tmp, "mediump", "lowp");
      // shaders++;
      // string[i] = tmp;
      int h = hashme(string[i]);
      
      // new_len = s2l+j+1;
#ifdef PVR2
        if(h == 1046176405) {
        glShaderSource(handle, count, &lastShader, length);
        return;
      }
      if(h == -1314509143) {
        glShaderSource(handle, count, &errshader, length);
        return;
      }
      if(h == -2059046731) {
        glShaderSource(handle, count, &errshader2, length);
        return;
      }/*
      if(h == 920258061) {
          glShaderSource(handle, count, &lowpshader, &);
        return;
      }*/
#endif
#ifndef PVR2
      if(h == 605637737) {
        glShaderSource(handle, count, &shader1, length);
        return;
      } else if(h == 235212001) {
        glShaderSource(handle, count, &shader2, length);
        return;
      } else if(h == -1087641176) {
        glShaderSource(handle, count, &shader3, length);
        return;
      }else if(h == -315174830) {
        glShaderSource(handle, count, &shader4, length);
        return;
      }else if(h == -426919008) {
        glShaderSource(handle, count, &shader5, length);
        return;
      }else if(h == -657363402) {
        glShaderSource(handle, count, &shader6, length);
        return;
      } else {
        glShaderSource(handle, count, string, length);
        return;
      }
#endif
      printf("%s %d\n", shader, h);
    }

    // char* s = str_replace(string[0], "gl_FragData[0]", "gl_FragColor");
    // const char str[1] = {s};
    // fprintf(stderr, "[PVRDBG] glShaderSource(%x, %x, data, [data])\n", handle, count);
    // printf("%s\n", string[0]);
    glShaderSource(handle, count, string, length);
     free(shader);
    return;
}
#endif
void hook_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    // Save the vertex data to an OBJ file
  char lol[200];
  sprintf(lol, "ux0:buffer%d.obj", rand());
    FILE *objFile = fopen(lol, "w");
    if (objFile) {
        const float *vertices = (const float *)data;
        size_t vertexCount = size / sizeof(float) / 3;
        for (size_t i = 0; i < vertexCount; ++i) {
            fprintf(objFile, "v %f %f %f\n", vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]);
        }
        fclose(objFile);
    }
    glBufferData(target, size, data, usage);
}

// Hook function for glBufferSubData
void hook_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    // Save the vertex data to an OBJ file
      char lol[200];
  sprintf(lol, "ux0:bufferSub%d.obj", rand());
   FILE *objFile = fopen(lol, "w");
    if (objFile) {
        const float *vertices = (const float *)data;
        size_t vertexCount = size / sizeof(float) / 3;
        for (size_t i = 0; i < vertexCount; ++i) {
            fprintf(objFile, "v %f %f %f\n", vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]);
        }
        fclose(objFile);
    }
    glBufferSubData(target, offset, size, data);
}


void glBindBuffer_dbg(uint32_t target, uint32_t buffer) {
    // fprintf(stderr, "[vgldbg] glBindBuffer(%x, %x)\n", target, buffer);
    return glBindBuffer(target, buffer);
}

void glCompileShader_dbg(uint32_t shader) {
    // fprintf(stderr, "[PVRDBG] glCompileShader(%x) — stubbed\n", shader);


    if (compiled == true) {

    } else {
    // if(shaders < 8) {
      glCompileShader(shader);
      GLint status;
      glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
      if (status == GL_FALSE) {
          GLsizei iloglen;
          glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &iloglen);

          if (iloglen < 0) {

              printf("No OpenGL vertex shader compiler log. What the frick?\n");
          } else {
              if (iloglen == 0) {
                  iloglen = 4096; // buggy driver (Adreno 220+)
              }

              char *ilogmem = (char *)malloc(iloglen + 1);
              ilogmem[iloglen] = '\0';
              glGetShaderInfoLog(shader, iloglen, &iloglen, ilogmem);


              printf("%s\n", ilogmem);
              printf("glGetError %d\n", glGetError());
              GLint params;
              glGetProgramiv(shader, GL_VALIDATE_STATUS, &params);
            
              free(ilogmem);
          }
      }
  }
}

void glGetShaderiv_dbg(GLuint handle, GLenum pname, GLint *params) {
    // fprintf(stderr, "[PVRDBG] glGetShaderiv(%x, %x, [data])\n", handle, pname);

    return glGetShaderiv(handle, pname, params);
}
void init_angle() {
   // glInitAngle(0, 0, 0);
}
void glAttachShader_dbg(uint32_t prog, uint32_t shad) {
    // fprintf(stderr, "[PVRDBG] glAttachShader(%x, %x)\n", prog, shad);
    return glAttachShader(prog, shad);
}

void glUnmapBufferOES_dbg(GLenum prog) {
    fprintf(stderr, "[PVR] glUnmapBufferOES_dbg()\n");
    return;
}
void glMapBufferOES_dbg(GLenum target, GLenum access) {
    fprintf(stderr, "[PVR] glMapBufferOES_dbg()\n");
    return;
}
EGLDisplay eglGetDisplay_f(EGLNativeDisplayType display_id) {
  EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  printf("dpy %p\n", d);
  return d;
}

void glGetProgramBinaryOES_dbg(GLuint program,
  	GLsizei bufsize,
  	GLsizei *length,
  	GLenum *binaryFormat,
  	void *binary) {
    fprintf(stderr, "[PVR] glGetProgramBinaryOES_dbg()\n");
    return;
}
void glProgramBinaryOES_dbg( GLuint program,
  	GLenum binaryFormat,
  	const void *binary,
  	GLsizei length) {
    fprintf(stderr, "[PVR] glProgramBinaryOES_dbg\n");
    return;
}

void glBindAttribLocation_dbg(GLuint program, GLuint index, const GLchar *name) {
    fprintf(stderr, "[PVRDBG] glBindAttribLocation(%x, %x, %s)\n", program, index, name);
    // glLinkProgram(program);
    return glBindAttribLocation(program, index, name);
}

void glLinkProgram_dbg(GLuint progr) {
    // fprintf(stderr, "[PVRDBG] glLinkProgram(%x)\n", progr);
  /*
        char buffer[50];
      sprintf(buffer, "ux0:data/duck/cache/%p.bin", (void*)progr);
   
    if(!compiled) {

      if (loadProgramFromFile(buffer, progr) != 0) {
          dumpProgramToFile(buffer, progr);
      }
    } else {
      loadProgramFromFile(buffer, progr);
    }*/
#ifndef ANGLE
     if(!compiled) {
#endif
        glLinkProgram(progr);
#ifndef ANGLE
        char buffer[50];
        sprintf(buffer, "ux0:data/duck/cache/%0x.bin", shader_name);
      
        dumpProgramToFile(buffer, progr);
     } else {
          char buffer[50];
      sprintf(buffer, "ux0:data/duck/cache/%0x.bin", shader_name);
        loadProgramFromFile(buffer, progr);
     }
#endif
// glEnable(GL_MULTISAMPLE);
    return;
}
void glGetProgramiv_dbg(GLuint program, GLenum pname, GLint *params) {
    // fprintf(stderr, "[PVRDBG] glGetProgramiv(%x, %x, [data])\n", program, pname);
    return glGetProgramiv(program, pname, params);
}

void glGetProgramInfoLog_dbg(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog) {
    // fprintf(stderr, "[PVRDBG] glGetProgramInfoLog(%x, %x, [data], [data])\n", program, maxLength);
    return glGetProgramInfoLog(program, maxLength, length, infoLog);
}

GLint glGetUniformLocation_dbg(GLuint prog, const GLchar *name) {
    // fprintf(stderr, "[PVRDBG] glGetUniformLocation(%x, %s)\n", prog, name);
    return glGetUniformLocation(prog, name);
}
bool d = false;
void glTexParameteri_dbg(GLenum target, GLenum pname, GLint param) {
    // fprintf(stderr, "[PVRDBG] glTexParameteri(%x, %x, %x)\n", target, pname, param);
    // glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );


    return glTexParameteri(target, pname, param);
}
void glGenTextures_dbg(GLsizei n, GLuint *textures) {
    fprintf(stderr, "[PVRDBG] glGenTextures(%x, [data])\n", n);
    return glGenTextures(n, textures);
}
unsigned int nextPowerOfTwo(unsigned int n) {
  // return 16;
  
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}
unsigned char* convertNPOTtoPOT(const unsigned char* npotData, int npotWidth, int npotHeight, int* potWidth, int* potHeight, int channels) {
    *potWidth = nextPowerOfTwo(npotWidth);
    *potHeight = nextPowerOfTwo(npotHeight);

    unsigned char* potData = (unsigned char*)malloc((*potWidth) * (*potHeight) * channels);
    if (!potData) {
        fprintf(stderr, "Failed to allocate memory for POT texture\n");
        return NULL;
    }

    memset(potData, 0, (*potWidth) * (*potHeight) * channels);


    for (int y = 0; y < npotHeight; ++y) {
        memcpy(potData + y * (*potWidth) * channels, npotData + y * npotWidth * channels, npotWidth * channels);
    }

    return potData;
}

unsigned char bilinear_interpolation(unsigned char *src_data, int src_width, int src_height, int channels, float x, float y, int channel) {
    int x1 = (int)floor(x);
    int y1 = (int)floor(y);
    int x2 = x1 + 1;
    int y2 = y1 + 1;

    float dx = x - x1;
    float dy = y - y1;

    // Clamp coordinates to avoid going out of bounds
    x1 = (x1 < 0) ? 0 : (x1 >= src_width) ? src_width - 1 : x1;
    x2 = (x2 < 0) ? 0 : (x2 >= src_width) ? src_width - 1 : x2;
    y1 = (y1 < 0) ? 0 : (y1 >= src_height) ? src_height - 1 : y1;
    y2 = (y2 < 0) ? 0 : (y2 >= src_height) ? src_height - 1 : y2;

    // Calculate weights
    float w11 = (1 - dx) * (1 - dy);
    float w12 = (1 - dx) * dy;
    float w21 = dx * (1 - dy);
    float w22 = dx * dy;

    // Perform interpolation
    unsigned char p11 = src_data[(y1 * src_width + x1) * channels + channel];
    unsigned char p12 = src_data[(y2 * src_width + x1) * channels + channel];
    unsigned char p21 = src_data[(y1 * src_width + x2) * channels + channel];
    unsigned char p22 = src_data[(y2 * src_width + x2) * channels + channel];

    return (unsigned char)(w11 * p11 + w12 * p12 + w21 * p21 + w22 * p22);
}

// Function to resize a raw texture
unsigned char *resize_texture(unsigned char *src_data, int src_width, int src_height, int channels, int new_width, int new_height) {
    if (src_data == NULL || src_width <= 0 || src_height <= 0 || channels <= 0 || new_width <= 0 || new_height <= 0) {
        fprintf(stderr, "Invalid input parameters for resize_texture\n");
        return NULL;
    }

    // Allocate memory for the resized image
    unsigned char *dest_data = (unsigned char *)malloc(new_width * new_height * channels);
    if (dest_data == NULL) {
        fprintf(stderr, "Memory allocation failed for dest_data\n");
        return NULL;
    }

    // Calculate scaling factors
    float x_ratio = (float)(src_width - 1) / (new_width - 1);
    float y_ratio = (float)(src_height - 1) / (new_height - 1);

    // Resize the image using bilinear interpolation
    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            for (int c = 0; c < channels; c++) {
                float src_x = x * x_ratio;
                float src_y = y * y_ratio;
                dest_data[(y * new_width + x) * channels + c] = bilinear_interpolation(src_data, src_width, src_height, channels, src_x, src_y, c);
            }
        }
    }

    return dest_data;
}

void glTexImage2D_dbg(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data) {

    int chan = 3;
    if(format == GL_RGB) { chan = 3; }
    if(format == GL_RGBA) { chan = 4; }
    unsigned int newWidth = nextPowerOfTwo(width), newHeight = nextPowerOfTwo(height);
    if(newWidth > 64 || newHeight > 64) {
        newWidth = newWidth / 2;
        newHeight = newHeight / 2;
    }
    unsigned char *newData = resize_texture(data, width, height, chan, newWidth, newHeight);
    
    
    printf("Resized to: %u x %u\n", newWidth, newHeight);

    glTexImage2D(target, level, format, newWidth, newHeight, border, format, type, newData);
    free(newData);
    
    return;
}
bool init = false;
void glEnable_dbg(uint32_t cap) {

    return glEnable(cap);
}

void glViewport_dbg(GLint x, GLint y, GLsizei width, GLsizei height) {
    // fprintf(stderr, "[PVRDBG] glViewport(%d, %d, %d, %d)\n", x, y, width, height);
    glViewport(0,0,960,544);
    // glViewport(x,y,width,height);
}

void glCompressedTexImage2D_dbg(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data) {
    // fprintf(stderr, "[PVRDBG] glCompressedTexImage2D(%x, %x, %x, %x, %x, %x, %x, [data])\n", target, level, internalformat, width, height, border, imageSize);
    if(internalformat == GL_ETC1_RGB8_OES) {
      void* decompressed_data = malloc(width * height * 3);
      etc1_decode_image((etc1_byte *)data, (etc1_byte *)decompressed_data, width, height, 3, width * 3);
      glTexImage2D_dbg(target, level, GL_RGB, width, height, border, GL_RGB, GL_UNSIGNED_BYTE, decompressed_data);
      free(decompressed_data);
      return;
    } else {
      printf("Non-etc1 tex\n");
      return glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
    }
}
void glUniform1i_dbg(GLint location, GLint v0) {

    return glUniform1i(location, v0);
}
void glUniform1iv_dbg(GLint location, GLsizei count, const GLint *v) {

    return glUniform1iv(location, count, v);
}
void glUseProgram_dbg(GLuint program) {
    
    return glUseProgram(program);
}
void glBindTexture_dbg(int32_t target, uint32_t texture) {
    glBindTexture(target, texture);

    return;
}
void glDrawArrays_dbg(GLenum mode, GLint first, GLsizei count) {
    fprintf(stderr, "[PVRDBG] glDrawArrays(%x, %x, %x)\n", mode, first, count);
   
    glDrawArrays(mode, first, count);
    
    return;
}
void glClearColor_dbg(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    // fprintf(stderr, "[PVRDBG] glClearColor(%f, %f, %f, %f)\n", red, green, blue, alpha);
    return glClearColor(red, green, blue, alpha);
}
void glDrawElements_dbg(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
    fprintf(stderr, "[PVRDBG] glDrawElements(%x, %x, %x, [data])\n", mode, count, type);
    glDrawElements(mode, count, type, indices);
    return;
}

void glBufferData_dbg(GLenum target, GLsizei size, const GLvoid *data, GLenum usage) {
    fprintf(stderr, "[PVRDBG] glBufferData(%x, %x, [data], %x)\n", target, size, usage);
    return glBufferData(target, size, data, usage);
}
void glClear_dbg(GLuint mask) {
    return glClear(mask );
}
void glClearStencil_dbg(GLuint mask) {
    return glClearStencil(mask );
}
void glBufferSubData_dbg(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data) {
    // fprintf(stderr, "[PVRDBG] glBufferSubData(%x, %x, %x, [data], %x)\n", target, offset, size);
    return glBufferSubData(target, offset, size, data);
}
void glEnableVertexAttribArray_dbg(uint32_t index) {
    // fprintf(stderr, "[PVRDBG] glEnableVertexAttribArray(%x)\n", index);
    return glEnableVertexAttribArray(index);
}

void glDisableVertexAttribArray_dbg(uint32_t index) {
    // fprintf(stderr, "[PVRDBG] glDisableVertexAttribArray_dbg(%x)\n", index);
    return glDisableVertexAttribArray(index);
}
void glVertexAttribPointer_dbg(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
    // fprintf(stderr, "[PVRDBG] glVertexAttribPointer(%x, %x, %x, %x, %x, [data])\n", index, size, type, normalized, stride);
    return glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}
#if defined(PVR2) || defined(PIB)
  EGLint gnumconfig;
  EGLConfig gconfig;
  EGLSurface Surface;

EGLSurface eglCreateWindowSurface_f(EGLDisplay display, EGLConfig config, NativeWindowType native_window, EGLint const * attrib_list) {
  #ifndef PIB
  	Psp2NativeWindow window;
    window.type = PSP2_DRAWABLE_TYPE_WINDOW;
	window.numFlipBuffers = 2;
	window.flipChainThrdAffinity = 0x20000;
	window.windowSize = PSP2_WINDOW_960X544;
    Surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)0, 0);
#endif
#ifdef PIB
    Surface = eglCreateWindowSurface(display, config, VITA_WINDOW_960X544, 0);
#endif
	if(!Surface)
	{
		printf("EGL surface create failed.\n");
		return;
	}
	printf("EGL surface create suceess.\n");
    // eglSurfaceAttrib(display, Surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED);
	return Surface;
}
EGLBoolean eglSwapBuffers_f(EGLDisplay dpy, EGLSurface surface) {
  
  eglSwapBuffers(dpy, surface);
  sceCtrlReadBufferPositive(0, &pad, 1);
  // void (* set_is_foreground)(bool is) = (void *)so_symbol(&bc2_mod, "NCT_SetAndroidAppForeground");
  // // set_is_foreground(false);
  // set_is_foreground(true);
  // printf("eglSwapBuffers %p %p 0x%x\n", dpy, surface, eglGetError());
  return;
}
EGLContext cc;
EGLDisplay gdpy;
EGLint ContextAttributeList[] = 
{
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};
EGLBoolean eglInitialize_f(EGLDisplay dpy, EGLint* major, EGLint* minor) {
  // printf("eglInitialize\n");
  EGLBoolean res = eglInitialize(dpy, NULL, NULL);
  printf("eglInitialize\n");
  return res;
}
EGLContext eglCreateContext_f(EGLDisplay display,
  	EGLConfig config,
  	EGLContext share_context,
  	EGLint const * attrib_list) {
    const EGLint contextAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                 EGL_NONE};
  printf("eglCreateContext_f %d\n", eglGetError());
  cc = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);
  printf("eglCreateContext_f %d\n", eglGetError());
  return cc;
}
int eglQuerySurface_f(EGLDisplay dpy,
                                       EGLSurface surface,
                                       EGLint attribute,
                                       EGLint *value)
{
  printf("eglQuerySurface_f %d\n", attribute);
   if(attribute == 0x3057) { *value = 960; };
   if(attribute == 0x3056) { *value = 560; };
    return 1;// eglQuerySurface(dpy, Surface, attribute, value);
}

  EGLBoolean eglMakeCurrent_f(EGLDisplay display,
  	EGLSurface draw,
  	EGLSurface read,
  	EGLContext context) {
    // printf("eglMakeCurrent_f %d\n", eglGetError());
    EGLBoolean c = eglMakeCurrent(display, draw, read, context);
  printf("eglMakeCurrent_f done %d\n", eglGetError());
  // rinit();
#ifdef ANGLE
  glInitAngle(0, 0, 0); //Init angle lib
#endif
  return c;
  }
EGLBoolean eglSwapInterval_f(EGLDisplay dpy, EGLint interval) {
  return eglSwapInterval(dpy, interval);
}
  EGLBoolean eglChooseConfig_f( 	EGLDisplay display,
  	EGLint const * attrib_list,
  	EGLConfig * configs,
  	EGLint config_size,
  	EGLint * num_config) {
        	EGLint cfg_attribs[] = { 		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 8,
		EGL_STENCIL_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE };
      EGLBoolean c = eglChooseConfig(display, attrib_list, configs, config_size, num_config);
      gdpy = display;
      gconfig = configs;
  printf("eglChooseConfig %d\n", eglGetError());
  return c;
  }
#endif

extern void *__cxa_guard_acquire;
extern void *__cxa_guard_release;
extern void *__cxa_atexit;
extern void *__cxa_throw;
extern void *__cxa_allocate_exception;

void* __cxa_allocate_exception_f(int p1) {
  printf("ALLOC ECEPTION %d\n", p1);
  return;
}
typedef struct { // PlaceHolder Structure
    unsigned something[7];
    unsigned int length;
} GroupId2;
// void* _ZN4FMOD11EventSystem4loadEPKcP19FMOD_EVENT_LOADINFOPPNS_12EventProjectE(void* this, const char* name, FMOD_EVENT_LOADINFO* info, FMOD_EVENTPROJECT** project);
void* _ZN4FMOD11EventSystem12setMediaPathEPKc(void* this, const char* name);

void loadeventfile(void* this, const char* name, FMOD_EVENT_LOADINFO* info, FMOD_EVENTPROJECT** project) {
  printf("load %s\n", name);
  _ZN4FMOD11EventSystem12setMediaPathEPKc(this, "app0:module/");
  // _ZN4FMOD11EventSystem4loadEPKcP19FMOD_EVENT_LOADINFOPPNS_12EventProjectE(this, "sound.fev", 0, 0);
}
void glClearDepthf_sfp(int depth)
{
	float fa1;

	fa1 = *(float *)(&depth);
    
    // glClearDepthf(fa1);
}

void glDepthRangef_sfp(int fZNear, int fZFar)
{
	float fa1, fa2;

	fa1 = *(float *)(&fZNear);
	fa2 = *(float *)(&fZFar);
	// glDepthRangef(fa1, fa2);
}

FMOD_EVENTGROUP* preload(void* t, GroupId2* param_1) {
  printf("preload %p %p\n", t,param_1);
  printf("event %p\n", (FMOD_EVENTGROUP*)(t + 12));
  unsigned int uVar1;
  FMOD_EVENTGROUP *local_20;
  FMOD_EVENTGROUP *local_1c;
  
  // (**(code **)(*this->field12_0xc + 0xc))(this->field12_0xc,param_1->field0_0x0,0,&local_1c);
    FMOD_EVENTGROUP** eventgroup_ptr = (FMOD_EVENTGROUP**)((char*)t + 12);
    if (eventgroup_ptr == NULL || *eventgroup_ptr == NULL) {
        printf("Error: eventgroup pointer is NULL\n");
            int* ptr = (int*)malloc(sizeof(int));  // Dynamically allocate memory
    if (ptr != NULL) {
        *ptr = 1;  // Assign the value 1 to the allocated memory
    }
    return ptr;
    }

    FMOD_EVENTGROUP* eventgroup = *eventgroup_ptr;
    FMOD_EventGroup_GetGroupByIndex(
    eventgroup,
    param_1->something[0],
    0,
    (FMOD_EVENTGROUP **)&local_1c);

  printf("preload 1 done\n");
  printf("preloads: %d\n", param_1->length);
  if (1 < param_1->length) {
    uVar1 = 1;
    do {
      local_20 = (FMOD_EVENTGROUP *)0x0;
      // (**(code **)(*local_1c + 0x10))(local_1c,(&param_1->field0_0x0)[uVar1],0,&local_20);
      FMOD_EventGroup_GetGroupByIndex(
      (FMOD_EVENTGROUP *)local_1c,
      (&param_1->something)[uVar1],
      0,
      (FMOD_EVENTGROUP **)&local_20);

      local_1c = local_20;
      uVar1 = uVar1 + 1;
    } while (uVar1 < param_1->length);
  }
  printf("preload done %p\n", local_1c);
  return local_1c;
  // return 0;
}
typedef struct {
    enum { amt = 7 };
    unsigned idx[amt];
    unsigned idxLen;
} GroupId;

void FMOD_Debug_SetLevel_dbg(unsigned int level) {
  printf("Fmod debug %d\n", level);
  FMOD_Debug_SetLevel(0x000000FF);
}

void* GetGroup(void* this, GroupId *p1) {
  // printf("GetGroup\n");
  FMOD_EVENTGROUP* group;
  unsigned int uVar1;
  // printf("GetGroup1\n");
  void* (* GetFmodEventProject)(void *t) = (void *)so_symbol(&bc2_mod, "_ZN12wfSfxManager19GetFmodEventProjectEv");
  FMOD_EVENTPROJECT* project = (FMOD_EVENTPROJECT*)GetFmodEventProject(this);
  printf("GetGroup2 %p\n", project);
  FMOD_EventProject_GetGroupByIndex(project, p1->idx[0], false, &group);
  if (1 < p1->idxLen) {
    uVar1 = 1;
    do {
      FMOD_EVENTGROUP* tgroup;
      // (**(code **)(*local_1c + 0x10))(local_1c,(&param_1->idx)[uVar1],0,&tgroup);
      FMOD_EventProject_GetGroupByIndex(project, p1->idx[uVar1], false, &tgroup);
      group = tgroup;
      uVar1 = uVar1 + 1;
    } while (uVar1 < p1->idxLen);
  }
  // _ZN12wfSfxManager8GetGroupERKNS_7GroupIdE
  printf("GetGroup dfone\n");
  return group;
}

typedef struct { // PlaceHolder Structure
    char field0_0x0;
    char field1_0x1;
    char field2_0x2;
    char field3_0x3;
    char field4_0x4;
    char field5_0x5;
    char field6_0x6;
    char field7_0x7;
    char eventSys; // Created by retype action
    char field9_0x9;
    char field10_0xa;
    char field11_0xb;
    int project;
} SfxManager;

void** items;
int item_ptr = 0;
int calls = 0;
int IsPreloadFinished() {
  // calls++;
  if(item_ptr > 1) {
    // item_ptr = 0;
    return 1; //Trying to get FMOD to not glitch
  }
  // return 0;
  int iVar1;
  int **ppiVar2;
  char local_1c [4];
  void* acStack_18;
  /*
  void* (* const_begin)(void) = (void *)so_symbol(&bc2_mod, "_ZNK11wfFixedListIPN4FMOD10EventGroupEE11const_beginEv");
  void* (* opPoint)(void*) = (void *)so_symbol(&bc2_mod, "_ZNK11wfFixedListIPN4FMOD10EventGroupEE14const_iteratorptEv");
  void* (* opPP)(void*) = (void *)so_symbol(&bc2_mod, "_ZN11wfFixedListIPN4FMOD10EventGroupEE14const_iteratorppEv");
  
  int (* isValid)(void*) = (void *)so_symbol(&bc2_mod, "_ZNK11wfFixedListIPN4FMOD10EventGroupEE14const_iterator7isValidEv");
  
  */
  printf("All symbols2\n");
  // // const_begin();
  printf("Const begin\n");
  // iVar1 = isValid(acStack_18);
  int ctr = 0;
#if 0
  if (items[0] != 0) {
    
    do {
      // ppiVar2 = (int **)opPoint(acStack_18);
      // (**(code **)(**ppiVar2 + 0x38))(*ppiVar2,local_1c);
      FMOD_EVENT_STATE state;
      printf("n0\n");
      FMOD_EventGroup_GetState(items[ctr],&state);
      printf("fmod\n");
      if (state & FMOD_EVENT_STATE_LOADING) {
        return 0;
      }
      // opPP(acStack_18);
      ctr++;
      // iVar1 = isValid(acStack_18);
      
      printf("Looping\n");
    } while (items[ctr] != 0);
  }
#endif
  for(int i = 0; i < item_ptr; i++) {
      FMOD_EVENT_STATE state;
      // printf("n0\n");
      FMOD_EventGroup_GetState((void*)items[i], &state);
      printf("fmod\n");
      if (state & FMOD_EVENT_STATE_LOADING) {
        return 0;
      }
  }
  printf("ret1\n");
  return 1;
}

void PreloadGroup(SfxManager *this,GroupId *param_1)

{
  int iVar1;
  void *pwVar2;
  int *local_14;
  
  local_14 = (int *)GetGroup((void *)this,param_1);
  // iVar1 = IsGroupPreloaded(this,(EventGroup *)local_14);
  int (* IsGroupPreloaded)(void *t, void* p2) = (void *)so_symbol(&bc2_mod, "_ZNK12wfSfxManager16IsGroupPreloadedEPKN4FMOD10EventGroupE");
  int (* TryAcquire)(void *t) = (void *)so_symbol(&bc2_mod, "_ZN11wfSemaphore10TryAcquireEv");
  void* (* GetSingleton)(void) = (void *)so_symbol(&bc2_mod, "_ZN11wfSingletonI15wfFoundationJobE12GetSingletonEv");
  
  void* (* RenderYield)(void*) = (void *)so_symbol(&bc2_mod, "_ZN15wfFoundationJob11RenderYieldEv");
  void (* Release)(void*, int) = (void *)so_symbol(&bc2_mod, "_ZN11wfSemaphore7ReleaseEj");
  
  int (* GetCount)(void*) = (void *)so_symbol(&bc2_mod, "_ZN11wfSemaphore8GetCountEv");
  void (* PushBack)(void*, void**) = (void *)so_symbol(&bc2_mod, "_ZN11wfFixedListIPN4FMOD10EventGroupEE8pushBackERKS2_");
  void* (* GetFmodEventProject)(void *t) = (void *)so_symbol(&bc2_mod, "_ZN12wfSfxManager19GetFmodEventProjectEv");
  
  printf("All symbols\n");
  iVar1 = IsGroupPreloaded((void *)this,(void *)local_14);
  // 0x98EBAB64 is g_sfxLoadFreeEventDataJob
  void* g_sfxLoadFreeEventDataJob = (void*)0x98EBAB64;
  if(item_ptr > 1) return;
  if (iVar1 == 0) {
    if (((uint32_t*)g_sfxLoadFreeEventDataJob)[0] == 0) {
      // (**(code **)(*local_14 + 4))(local_14,0,1); //only decompiled this function because of this
      printf("Dangerous function\n");
      FMOD_EventGroup_LoadEventData(local_14, 0, 1);
      printf("Dangerous function done\n");
    }
    else {
      printf("Raw pointer\n");
      iVar1 = TryAcquire((void*)(g_sfxLoadFreeEventDataJob + 0x30));
      if (iVar1 == 0) {
        do {
          pwVar2 = GetSingleton();
          RenderYield(pwVar2);
          iVar1 = TryAcquire((void *)(g_sfxLoadFreeEventDataJob + 0x30));
        } while (iVar1 != 1);
      }
      printf("Raw pointer 2\n");
      *(int **)(g_sfxLoadFreeEventDataJob + 0x3c) = local_14;
      *(int *)(g_sfxLoadFreeEventDataJob + 0x40) = 0;
      *(char *)(g_sfxLoadFreeEventDataJob + 0x44) = 1;
      printf("Raw pointer 3\n");
      Release((void *)(g_sfxLoadFreeEventDataJob + 0x34),1);
      iVar1 = GetCount((void *)(g_sfxLoadFreeEventDataJob + 0x30));
      while (iVar1 == 0) {
        pwVar2 = (void *)GetSingleton();
        RenderYield(pwVar2);
        iVar1 = GetCount((void *)(g_sfxLoadFreeEventDataJob + 0x30));
      }
      printf("Raw pointer done\n");
    }
    printf("PushBack\n");
    PushBack((void *)&this[4].project,(void **)&local_14);
    items[item_ptr] = (void*)local_14;
    printf("ptr%d\n", item_ptr);
    item_ptr++;
  }
  return;
}
void retv(void) {
  return;
}
#if !defined(PVR2) && !defined(PIB)
void init_vgl() {
  vglSetupRuntimeShaderCompiler(SHARK_OPT_UNSAFE, SHARK_ENABLE, SHARK_ENABLE, SHARK_ENABLE);
  vglUseVram(GL_TRUE);
  vglInitExtended(0, SCREEN_W, SCREEN_H, MEMORY_VITAGL_THRESHOLD_MB * 1024 * 1024, SCE_GXM_MULTISAMPLE_4X);
}
void swap_vgl() {
  vglSwapBuffers(GL_FALSE);
}
#endif



void restore() {
  printf("!!RESTORING CONTEXT!!\n");
  return;
}

void update_ts(void* this, int par1) {
  printf("Starting game\n"); //attempted to get UI to show
  void (* _ZN9TitleScrn13PostAdNewGameEv)(void *ts) = (void *)so_symbol(&bc2_mod, "_ZN9TitleScrn13PostAdNewGameEv");
  _ZN9TitleScrn13PostAdNewGameEv(this);
}

int generate_ctrl() {

    uint16_t uVar4 = 0;

    if ((pad.buttons & SCE_CTRL_CROSS))  uVar4 |= 1; 
    if ((pad.buttons & SCE_CTRL_LEFT))  uVar4 |= 0x20;  
    if ((pad.buttons & SCE_CTRL_CIRCLE))    uVar4 |= 2;   
    if ((pad.buttons & SCE_CTRL_RIGHT))  uVar4 |= 0x10;   
    if ((pad.buttons & SCE_CTRL_UP))  uVar4 |= 0x40;    
    if ((pad.buttons & SCE_CTRL_DOWN))  uVar4 |= 0x80;
    if ((pad.buttons & SCE_CTRL_START))  uVar4 |= 8;


    return uVar4;
}

int ctrl() {
  return generate_ctrl();
}
so_hook popup_hook;
so_hook fun_hook;
int popup(void* p1, int p2, void *param_2,void *param_3, void *param_4,void *param_5) {
  printf("popup  %d\n", p2);
  return SO_CONTINUE(int, popup_hook, p1, p2, param_2, param_3, param_4, param_5);
}
void popup2() {
  printf("_ZN9TitleScrnC2Ev\n");
} //More ui inspection

void fun() {
  SO_CONTINUE(int, fun_hook);
  printf("done fun\n");
}
// _ZN15wfFoundationJob3RunEv
void patch_game(void) {

  hook_arm(so_symbol(&bc2_mod, "_ZN8wfSystem3LogEPKcz"), (uintptr_t)&printf);
  hook_arm(so_symbol(&bc2_mod, "_ZN9wfConsole3LogEPKcz"), (uintptr_t)&printf);
  
  hook_arm(so_symbol(&bc2_mod, "_ZN19wfAndroidDeviceGlue16GetDataDirectoryEv"), (uintptr_t)&datadir);
  hook_arm(so_symbol(&bc2_mod, "_ZN19wfAndroidDeviceGlue10GetOBBPathEv"), (uintptr_t)&obb_path);
  hook_arm(so_symbol(&bc2_mod, "_ZN19wfAndroidDeviceGlue17GetDeviceLanguageEv"), (uintptr_t)&ret_lang);
  // popup_hook = hook_arm(so_symbol(&bc2_mod, "_ZN16MenuFrameFactory11CreateFrameEiP13wfEnvironmentP12wfRenderPassP8wfEntityPv"), (uintptr_t)&popup);
  hook_arm(so_symbol(&bc2_mod, "_ZN28wfAndroidControllerInternals11handleInputEP11AInputEvent"), (uintptr_t)&ret1);
  // hook_arm(so_symbol(&bc2_mod, "_ZN9TitleScrnC2Ev"), (uintptr_t)&popup2);
  // hook_arm(so_symbol(&bc2_mod, "_ZN10wfPlatform16IsOverlayVisibleEv"), (uintptr_t)&ret1);
  // hook_arm(so_symbol(&bc2_mod, "_ZN19wfReferenceHashInstI13wfManagedFileEC2EPKc"), (uintptr_t)&ret0);
  // hook_arm(so_symbol(&bc2_mod, "_Z16NvGetGamepadAxesP7_JNIEnvP8_jobjectRi"), (uintptr_t)&get_gamepad_buttons);
  hook_arm(so_symbol(&bc2_mod, "_ZN10wfPlatform16GetUserDirectoryEv"), (uintptr_t)&datadir);
  
  hook_arm(so_symbol(&bc2_mod, "_ZNK17SfxGroupPreloader11IsPreloadedEPKc"), (uintptr_t)&ret1);
  // hook_arm(so_symbol(&bc2_mod, "_ZN12wfSfxManager16GetGroupForEventEj"), (uintptr_t)&ret0);
  hook_arm(so_symbol(&bc2_mod, "_ZN8wfSystem11GetCpuCountEv"), (uintptr_t)&fake_cpus);
  
  //window _ZN13MenuPopupBase10InitWindowEPKcff
  hook_arm(so_symbol(&bc2_mod, "_ZNK12wfSfxManager17IsPreloadFinishedEv"), (uintptr_t)&IsPreloadFinished); //IsPreloadFinished
  // hook_arm(so_symbol(&bc2_mod, "_ZN12wfSfxManager13FreeEventDataEiPN4FMOD5EventE"), (uintptr_t)&ret1);
  
  hook_arm(so_symbol(&bc2_mod, "_ZN12wfSfxManager8GetGroupERKNS_7GroupIdE"), (uintptr_t)&GetGroup);
  hook_arm(so_symbol(&bc2_mod, "_ZN12wfSfxManager12PreloadGroupERKNS_7GroupIdE"), (uintptr_t)&PreloadGroup);
  
  // fun_hook = hook_addr(0x983b811c, (uintptr_t)&fun);
  // hook_arm(so_symbol(&bc2_mod, "_ZNK16MenuFrameManager17IsFramePopEnabledEv"), (uintptr_t)&ret1);
  hook_arm(so_symbol(&bc2_mod, "_ZN5wfMCP12InternalQuitEv"), (uintptr_t)&internal_quit);
  // hook_arm(so_symbol(&bc2_mod, "_ZN12wfSfxManager6UpdateEf"), (uintptr_t)&retv);
  // hook_arm(so_symbol(&bc2_mod, "_ZN16wfControllerData6UpdateEfb"), (uintptr_t)&ret0);
  
  hook_arm(so_symbol(&bc2_mod, "__cxa_guard_release"), (uintptr_t)&__cxa_guard_release);
  hook_arm(so_symbol(&bc2_mod, "__cxa_guard_acquire"), (uintptr_t)&__cxa_guard_acquire);
  hook_arm(so_symbol(&bc2_mod, "__cxa_throw"), (uintptr_t)&__cxa_throw);
  hook_arm(so_symbol(&bc2_mod, "__cxa_allocate_exception"), (uintptr_t)&__cxa_allocate_exception_f);
  hook_arm(so_symbol(&bc2_mod, "_ZN21wfParticleEmitterBase9IsVisibleEv"), (uintptr_t)&ret0); //Disables SFX which causes a GPU crash
  // hook_arm(so_symbol(&bc2_mod, "_ZN21wfParticleEmitterBase10SetEnabledEb"), (uintptr_t)&ret0);
  // hook_arm(so_symbol(&bc2_mod, "_ZN19wfVirtualController20IsVirtualMouseActiveEv"), (uintptr_t)&ret1);
  
//   
  hook_arm(so_symbol(&bc2_mod, "_ZN19wfAndroidDeviceGlue17HideLoadingDialogEv"), (uintptr_t)&test);
  
  hook_arm(so_symbol(&bc2_mod, "_ZN19wfAndroidDeviceGlue15AcquireWakeLockEv"), (uintptr_t)&test);
  
  hook_arm(so_symbol(&bc2_mod, "_ZN19wfAndroidDeviceGlue15ReleaseWakeLockEv"), (uintptr_t)&test);
  
  // hook_arm(so_symbol(&bc2_mod, "_ZN9TitleScrn6UpdateEf"), (uintptr_t)&update_ts);
  hook_arm(so_symbol(&bc2_mod, "_ZN19wfAndroidDeviceGlue11HasJoystickEv"), (uintptr_t)&ret1);
  hook_arm(so_symbol(&bc2_mod, "_ZN19wfAndroidDeviceGlue7QuitAppEv"), (uintptr_t)&ret0);
  hook_arm(so_symbol(&bc2_mod, "_ZN8wfSystem13GetAndroidAppEv"), (uintptr_t)&struct601);
  
  // 
  // hook_arm(so_symbol(&bc2_mod, "_ZN8wfEngine4ExecEv"), (uintptr_t)&maing);
  
  hook_arm(so_symbol(&bc2_mod, "tolower"), (uintptr_t)&tolower_f);

  hook_arm(so_symbol(&bc2_mod, "_ZN6wfHeap10SetPtrDescEPvPKc"), (uintptr_t)&SetPtrDesc);
  // hook_arm(so_symbol(&bc2_mod, "_ZN15wfFoundationJob12QuitCallbackEPv"), (uintptr_t)&SetThreadName);
  

  // hook_arm(so_symbol(&bc2_mod, "_ZN28wfAndroidControllerInternals16handleTouchInputEP11AInputEvent"), (uintptr_t)&ret1);
  hook_arm(so_symbol(&bc2_mod, "_ZN15NvButtonMapping9getStatusEv"), (uintptr_t)&ctrl);
  hook_arm(so_symbol(&bc2_mod, "_Z19NvGetGamepadButtonsP7_JNIEnvP8_jobjectRi"), (uintptr_t)&ret1);
  hook_arm(so_symbol(&bc2_mod, "_Z16NvGetGamepadAxesP7_JNIEnvP8_jobjectRi"), (uintptr_t)&ret1);
  
/*  hook_thumb(so_symbol(&bc2_mod, "Android_KarismaBridge_GetAppWritePath"), (uintptr_t)&Android_KarismaBridge_GetAppWritePath);

  hook_thumb(so_symbol(&bc2_mod, "Android_KarismaBridge_GetKeyboardOpened"), (uintptr_t)&ret0);

  hook_thumb(so_symbol(&bc2_mod, "Android_KarismaBridge_EnableSound"), (uintptr_t)&Android_KarismaBridge_EnableSound);
  hook_thumb(so_symbol(&bc2_mod, "Android_KarismaBridge_DisableSound"), (uintptr_t)&Android_KarismaBridge_DisableSound);
  hook_thumb(so_symbol(&bc2_mod, "Android_KarismaBridge_LockSound"), (uintptr_t)&ret0);
  
  hook_thumb(so_symbol(&bc2_mod, "Android_KarismaBridge_UnlockSound"), (uintptr_t)&ret0);
 */ // hook_thumb(so_symbol(&bc2_mod, "ALooper_pollAll"), (uintptr_t)&ret0);

}

extern void *_ZdaPv;
extern void *_ZdlPv;
extern void *_Znaj;
extern void *_Znwj;

extern void *__aeabi_atexit;
extern void *__aeabi_d2f;
extern void *__aeabi_d2ulz;
extern void *__aeabi_dcmpgt;
extern void *__aeabi_dmul;
extern void *__aeabi_f2d;
extern void *__aeabi_f2iz;
extern void *__aeabi_f2ulz;
extern void *__aeabi_fadd;
extern void *__aeabi_fcmpge;
extern void *__aeabi_fcmpgt;
extern void *__aeabi_fcmple;
extern void *__aeabi_fcmplt;
extern void *__aeabi_fdiv;
extern void *__aeabi_fsub;
extern void *__aeabi_idiv;
extern void *__aeabi_idivmod;
extern void *__aeabi_l2d;
extern void *__aeabi_l2f;
extern void *__aeabi_ldivmod;
extern void *__aeabi_uidiv;
extern void *__aeabi_uidivmod;
extern void *__aeabi_dcmplt;
extern void *__aeabi_uldivmod;

extern void *__cxa_pure_virtual;
extern void *__aeabi_ddiv;
extern void *__sF;
extern void *__aeabi_idiv0;
extern void *__aeabi_memmove;
extern void *__aeabi_memset;
extern void *__aeabi_ldiv0;
extern void *__aeabi_memcpy;
extern void *__aeabi_i2d;
extern void *__aeabi_ul2f;
extern void *__aeabi_ui2d;
extern void *__aeabi_fcmpeq;
extern void *__aeabi_dcmpge;
extern void *__aeabi_ul2d;
extern void *__aeabi_dcmpeq;
extern void *__aeabi_dsub;
extern void *__aeabi_ui2f;
extern void* _ZN4FMOD5Sound12getSyncPointEiPP14FMOD_SYNCPOINT;
extern void* _ZN4FMOD5Event11getCategoryEPPNS_13EventCategoryE;
extern void* _ZN4FMOD11EventSystem18getCategoryByIndexEiPPNS_13EventCategoryE;
extern void* _ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKciPjPPvS6_EPFS1_S5_S5_EPFS1_S5_S5_jS4_S5_EPFS1_S5_jS5_EPFS1_P18FMOD_ASYNCREADINFOS5_ESA_i;
extern void* _ZN4FMOD5Event7getInfoEPiPPcP15FMOD_EVENT_INFO;
extern void* _ZN4FMOD5Event9setVolumeEf;
extern void* _ZN4FMOD11EventSystem4initEijPvj;
extern void* _ZN4FMOD11EventSystem22getReverbPresetByIndexEiP22FMOD_REVERB_PROPERTIESPPc;
extern void* _ZN4FMOD11EventSystem7releaseEv;
extern void* _ZN4FMOD6System19setAdvancedSettingsEP21FMOD_ADVANCEDSETTINGS;
extern void* _ZN4FMOD11EventSystem23set3DListenerAttributesEiPK11FMOD_VECTORS3_S3_S3_;
extern void* _ZN4FMOD11EventSystem11getCategoryEPKcPPNS_13EventCategoryE;
extern void* _ZN4FMOD11EventSystem20getEventByGUIDStringEPKcjPPNS_5EventE;
extern void* _ZN4FMOD5Event13getMemoryInfoEjjPjP25FMOD_MEMORY_USAGE_DETAILS;
extern void* _ZN4FMOD11EventSystem19setReverbPropertiesEPK22FMOD_REVERB_PROPERTIES;
extern void* _ZN4FMOD5Event15getChannelGroupEPPNS_12ChannelGroupE;
extern void* _ZN4FMOD7Channel11setPositionEjj;
extern void* _ZN4FMOD6System18getChannelsPlayingEPi;
extern void* _ZN4FMOD14EventParameter8setValueEf;
extern void* _ZN4FMOD5Sound16getSyncPointInfoEP14FMOD_SYNCPOINTPciPjj;
extern void* _ZN4FMOD5Event19getParameterByIndexEiPPNS_14EventParameterE;
extern void* _ZN4FMOD5Event15set3DAttributesEPK11FMOD_VECTORS3_S3_;
extern void* _ZN4FMOD12ChannelGroup14getNumChannelsEPi;
extern void* _ZN4FMOD6System19getAdvancedSettingsEP21FMOD_ADVANCEDSETTINGS;
extern void* _ZN4FMOD11EventSystem17set3DNumListenersEi;
extern void* _ZN4FMOD11EventSystem15getSystemObjectEPPNS_6SystemE;
extern void* _ZN4FMOD11EventSystem6updateEv;
extern void* _ZN4FMOD6System13getMemoryInfoEjjPjP25FMOD_MEMORY_USAGE_DETAILS;
extern void* _ZN4FMOD11EventSystem18getEventBySystemIDEjjPPNS_5EventE;
extern void* _ZN4FMOD5Event11setCallbackEPF11FMOD_RESULTP10FMOD_EVENT23FMOD_EVENT_CALLBACKTYPEPvS5_S5_ES5_;
extern void* _ZN4FMOD12ChannelGroup10getChannelEiPPNS_7ChannelE;
extern void* _ZN4FMOD5Event5startEv;
extern void* _ZN4FMOD6System16setDSPBufferSizeEji;
extern void* _ZN4FMOD5Event8getStateEPj;
extern void* _ZN4FMOD5Event9setPausedEb;
extern void* _ZN4FMOD5Event4stopEb;
extern void* _ZN4FMOD11EventSystem26setReverbAmbientPropertiesEP22FMOD_REVERB_PROPERTIES;
extern void* _ZN4FMOD11EventSystem4loadEPKcP19FMOD_EVENT_LOADINFOPPNS_12EventProjectE;
extern void* _ZN4FMOD11EventSystem12getNumEventsEPi;
extern void* _ZN4FMOD11EventSystem17getProjectByIndexEiPPNS_12EventProjectE;
void* sysptr; 
int _ZN4FMOD11EventSystem12getNumEventsEPi_dbg(void* sys) {
  printf("_ZN4FMOD11EventSystem12getNumEventsEPi_dbg %p\n", sys);
  // void* (* _ZN4FMOD11EventSystem12getNumEventsEPi2)(void* system) = _ZN4FMOD11EventSystem12getNumEventsEPi;
  // int ret = _ZN4FMOD11EventSystem12getNumEventsEPi2(&sys);
  int ret = 0;
  FMOD_EventSystem_GetNumEvents(sys, &ret);
  printf("Event num%d\n", ret);
  return ret;
}
#include <fmod_errors.h>
void _ZN4FMOD11EventSystem4loadEPKcP19FMOD_EVENT_LOADINFOPPNS_12EventProjectE_dbg(void* sys, char* str, void* project, void* unk) {
  printf("_ZN4FMOD11EventSystem4loadEPKcP19FMOD_EVENT_LOADINFOPPNS_12EventProjectE_dbg %p %s\n", str);
  printf("%s\n",FMOD_ErrorString(FMOD_EventSystem_Load(sys, "app0:sound.fev", 0, 0)));
  // sysptr = sys;
}
void _ZN4FMOD11EventSystem4initEijPvj_dbg(void* sys, int max, unsigned int flags, void* extra,  unsigned int flags2) {
  printf("_ZN4FMOD11EventSystem4initEijPvj_dbg\n");
  int ret = FMOD_EventSystem_Init(sys, 64, FMOD_INIT_NORMAL, 0, FMOD_EVENT_INIT_NORMAL);
  
  printf("%s\n",FMOD_ErrorString(ret));
}
void FMOD_EventSystem_Create_dbg(void** sys) {
  printf("%s\n",FMOD_ErrorString(FMOD_EventSystem_Create(sys)));
  // proje
}
void __stack_chk_fail_soloader() {
    printf("STACK!\n");
}
extern void *__stack_chk_guard;
extern void *__cxa_finalize;
extern void *__aeabi_unwind_cpp_pr1;
extern void *__aeabi_unwind_cpp_pr0;
static int __stack_chk_guard_fake = 0x42424242;
static FILE __sF_fake[0x100][3];

struct tm *localtime_hook(time_t *timer) {
  struct tm *res = localtime(timer);
  if (res)
    return res;
  // Fix an uninitialized variable bug.
  time(timer);
  return localtime(timer);
}

extern void* aeglCreateContext;

extern void* aeglGetDisplay;

extern void* aeglQuerySurface;

extern void* aeglMakeCurrent;

extern void* aeglGetConfigAttrib;
extern void* aeglInitialize;
extern void* aeglGetError;
extern void* aeglCreateWindowSurface;
extern void* aeglSwapInterval;
extern void* aeglSwapBuffers;
extern void* aeglChooseConfig; //For apitrace

static so_default_dynlib dynlib_functions[] = {
#if defined(PVR2)/* apitrace
  { "eglCreateContext", (uintptr_t)&aeglCreateContext },
  { "eglGetDisplay", (uintptr_t)&aeglGetDisplay },
  { "eglQuerySurface", (uintptr_t)&aeglQuerySurface },
  { "eglMakeCurrent", (uintptr_t)&aeglMakeCurrent },
  { "eglGetConfigAttrib", (uintptr_t)&aeglGetConfigAttrib },
  { "eglGetError", (uintptr_t)&aeglGetError },
  { "eglSwapInterval", (uintptr_t)&aeglSwapInterval },
  { "eglChooseConfig", (uintptr_t)&aeglChooseConfig },
  { "eglSwapBuffers", (uintptr_t)&aeglSwapBuffers },
  { "eglCreateWindowSurface", (uintptr_t)&aeglCreateWindowSurface },
  { "eglInitialize", (uintptr_t)&aeglInitialize },*/
  { "eglCreateContext", (uintptr_t)&eglCreateContext },
  { "eglGetDisplay", (uintptr_t)&eglGetDisplay },
  { "eglQuerySurface", (uintptr_t)&eglQuerySurface },
  { "eglMakeCurrent", (uintptr_t)&eglMakeCurrent_f },
  { "eglGetConfigAttrib", (uintptr_t)&eglGetConfigAttrib },
  { "eglGetError", (uintptr_t)&eglGetError },
  { "eglSwapInterval", (uintptr_t)&eglSwapInterval_f },
  { "eglChooseConfig", (uintptr_t)&eglChooseConfig },
  { "eglSwapBuffers", (uintptr_t)&eglSwapBuffers_f },
  { "eglCreateWindowSurface", (uintptr_t)&eglCreateWindowSurface },
  { "eglInitialize", (uintptr_t)&eglInitialize },
#elif defined(PIB)
  { "eglCreateContext", (uintptr_t)&eglCreateContext },
  { "eglGetDisplay", (uintptr_t)&eglGetDisplay },
  { "eglQuerySurface", (uintptr_t)&eglQuerySurface },
  { "eglMakeCurrent", (uintptr_t)&eglMakeCurrent },
  { "eglGetConfigAttrib", (uintptr_t)&eglGetConfigAttrib },
  { "eglGetError", (uintptr_t)&eglGetError },
  { "eglSwapInterval", (uintptr_t)&ret0 },
  { "eglChooseConfig", (uintptr_t)&eglChooseConfig },
  { "eglSwapBuffers", (uintptr_t)&eglSwapBuffers },
  { "eglCreateWindowSurface", (uintptr_t)&eglCreateWindowSurface },
  { "eglInitialize", (uintptr_t)&eglInitialize },
#else
  { "eglCreateContext", (uintptr_t)&ret0 },
  { "eglGetDisplay", (uintptr_t)&ret0 },
  { "eglQuerySurface", (uintptr_t)&ret0 },
  { "eglMakeCurrent", (uintptr_t)&ret0 },
  { "eglGetConfigAttrib", (uintptr_t)&ret0 },
  { "eglGetError", (uintptr_t)&ret0 },
  { "eglSwapInterval", (uintptr_t)&ret0 },
  { "eglChooseConfig", (uintptr_t)&ret0 },
  { "eglSwapBuffers", (uintptr_t)&swap_vgl },
  { "eglCreateWindowSurface", (uintptr_t)&ret0 },
  { "eglInitialize", (uintptr_t)&init_vgl },
#endif
    { "glIsFramebuffer", (uintptr_t)&glIsFramebuffer },
    { "puts", (uintptr_t)&puts },
    // { "getSyncPoint", (uintptr_t)&ret0 },
    { "getc", (uintptr_t)&getc },
    { "glGetProgramInfoLog", (uintptr_t)&glGetProgramInfoLog_dbg },
    
    { "pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_soloader },
    // { "getCategory", (uintptr_t)&FMOD_Event_GetCategory },
    { "islower", (uintptr_t)&islower },
    // { "getCategoryByIndex", (uintptr_t)&FMOD_EventSystem_GetCategoryByIndex },
    { "cos", (uintptr_t)&cos_sfp },
    { "_Unwind_GetTextRelBase", (uintptr_t)&ret0 },
    { "ispunct", (uintptr_t)&ispunct },
    { "__cxa_finalize", (uintptr_t)&__cxa_finalize },
    { "glCompileShader", (uintptr_t)&glCompileShader_dbg },
    { "send", (uintptr_t)&ret0 },
    { "strcpy", (uintptr_t)&strcpy },
    { "glUniform1fv", (uintptr_t)&glUniform1fv_sfp },
    { "__aeabi_dcmplt", (uintptr_t)&__aeabi_dcmplt },
    { "__aeabi_idiv0", (uintptr_t)&__aeabi_idiv0 },
    { "glUniform2fv", (uintptr_t)&glUniform2fv_sfp },
    // { "setFileSystem", (uintptr_t)&FMOD_System_SetFileSystem },
    { "glGenerateMipmap", (uintptr_t)&glGenerateMipmap },
    { "vsnprintf", (uintptr_t)&vsnprintf },
    { "pthread_getspecific", (uintptr_t)&pthread_getspecific },
    { "pthread_cond_timedwait", (uintptr_t)&pthread_cond_timedwait_soloader },
    { "glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray_dbg },
    { "pthread_create", (uintptr_t)&pthread_create_soloader },
    { "__aeabi_unwind_cpp_pr1", (uintptr_t)&__aeabi_unwind_cpp_pr1 },
    { "isalnum", (uintptr_t)&isalnum },
    { "glClearColor", (uintptr_t)&glClearColor_sfp },
    { "strtod", (uintptr_t)&strtod },
    { "__aeabi_fcmplt", (uintptr_t)&__aeabi_fcmplt },
    { "cosh", (uintptr_t)&cosh },
    { "glUnmapBufferOES", (uintptr_t)&glUnmapBufferOES_dbg },
    { "recv", (uintptr_t)&ret0 },
    
    // { "getInfo", (uintptr_t)&ret0 },
    // { "setVolume", (uintptr_t)&ret0 },
    { "_Unwind_VRS_Get", (uintptr_t)&ret0 },
    { "glGenBuffers", (uintptr_t)&glGenBuffers },
    { "AInputEvent_getSource", (uintptr_t)&ret1 },
    { "pthread_mutex_init", (uintptr_t)&pthread_mutex_init_soloader },
    // { "init", (uintptr_t)&ret0 },
    { "ALooper_prepare", (uintptr_t)&ret1 },
    { "fclose", (uintptr_t)&fclose },
    { "glUniform1f", (uintptr_t)&glUniform1f_sfp },
    { "fseek", (uintptr_t)&fseek },
    { "glBufferData", (uintptr_t)&glBufferData },
    // { "getReverbPresetByIndex", (uintptr_t)&ret0 },
    { "pthread_self", (uintptr_t)&pthread_self_soloader },
    { "AInputQueue_detachLooper", (uintptr_t)&ret1 },
    { "AInputQueue_attachLooper", (uintptr_t)&ret1 },
    { "ceilf", (uintptr_t)&ceilf },
    { "fopen", (uintptr_t)&fopen },
    { "glGetShaderiv", (uintptr_t)&glGetShaderiv_dbg },
    
    { "localtime", (uintptr_t)&localtime },
    { "pthread_cond_init", (uintptr_t)&pthread_cond_init_soloader },
    
    { "AConfiguration_delete", (uintptr_t)&ret1 },
    { "rewind", (uintptr_t)&rewind },
    { "glFlush", (uintptr_t)&glFlush },
    { "write", (uintptr_t)&write },
    // { "release", (uintptr_t)&ret0 },
    { "__aeabi_memmove", (uintptr_t)&__aeabi_memmove },
    { "__aeabi_fcmple", (uintptr_t)&__aeabi_fcmple },
    // { "setAdvancedSettings", (uintptr_t)&FMOD_System_SetAdvancedSettings },
    { "clock", (uintptr_t)&clock },
    { "strtoul", (uintptr_t)&strtoul },
    { "glCreateShader", (uintptr_t)&glCreateShader_dbg },
    
    { "pthread_attr_init", (uintptr_t)&pthread_attr_init_soloader },
    { "glUseProgram", (uintptr_t)&glUseProgram_dbg },
    { "glDepthFunc", (uintptr_t)&glDepthFunc },
    { "glGetShaderInfoLog", (uintptr_t)&glGetShaderInfoLog },
    { "pthread_mutex_trylock", (uintptr_t)&pthread_mutex_trylock_soloader },
    { "glDrawElements", (uintptr_t)&glDrawElements },
    { "freopen", (uintptr_t)&freopen },
    { "AKeyEvent_getKeyCode", (uintptr_t)&ret1 },
    { "memcpy", (uintptr_t)&memcpy },
    // { "set3DListenerAttributes", (uintptr_t)&FMOD_EventSystem_Set3DListenerAttributes },
    { "usleep", (uintptr_t)&usleep },
    { "__aeabi_uidiv", (uintptr_t)&__aeabi_uidiv },
    { "AMotionEvent_getY", (uintptr_t)&ret1 },
    { "glColorMask", (uintptr_t)&glColorMask },
    { "socket", (uintptr_t)&ret0 },
    { "glDeleteFramebuffers", (uintptr_t)&glDeleteFramebuffers },
    { "realloc", (uintptr_t)&my_realloc },
    { "ldexp", (uintptr_t)&ldexp_sfp },
    { "__aeabi_memset", (uintptr_t)&__aeabi_memset },
    { "_Unwind_DeleteException", (uintptr_t)&ret0 },
    { "pthread_key_delete", (uintptr_t)&pthread_key_delete },
    { "strncmp", (uintptr_t)&strncmp },
    { "__aeabi_fadd", (uintptr_t)&__aeabi_fadd },
    // { "getCategory", (uintptr_t)&ret0 },
    // { "getEventByGUIDString", (uintptr_t)&ret0 },
    // { "getMemoryInfo", (uintptr_t)&ret0 },
    { "cosf", (uintptr_t)&cosf },
    { "pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_soloader },
    { "pthread_key_create", (uintptr_t)&pthread_key_create },
    // { "setReverbProperties", (uintptr_t)&ret0 },
    { "pthread_cond_wait", (uintptr_t)&pthread_cond_wait_soloader },
    { "glUniform1iv", (uintptr_t)&glUniform1iv_dbg },
    { "acos", (uintptr_t)&acos_sfp },
    { "__errno", (uintptr_t)&__errno },
    { "wcscpy", (uintptr_t)&strcpy },
    { "_Unwind_Resume_or_Rethrow", (uintptr_t)&ret0 },
    { "glBindFramebuffer", (uintptr_t)&glBindFramebuffer },
    { "strchr", (uintptr_t)&strchr },
    { "glGetUniformLocation", (uintptr_t)&glGetUniformLocation_dbg },
    { "isalpha", (uintptr_t)&isalpha },
    { "strtoull", (uintptr_t)&strtoull },
    { "__aeabi_unwind_cpp_pr0", (uintptr_t)&__aeabi_unwind_cpp_pr0 },
    { "__aeabi_f2ulz", (uintptr_t)&__aeabi_f2ulz },
    // { "getChannelGroup", (uintptr_t)&ret0 },
    { "setjmp", (uintptr_t)&ret0 },
    // { "setPosition", (uintptr_t)&ret0 },
    { "iscntrl", (uintptr_t)&iscntrl },
    { "glGenTextures", (uintptr_t)&glGenTextures },
    { "memalign", (uintptr_t)&my_memalign },
    
    { "pthread_mutexattr_init", (uintptr_t)&pthread_mutexattr_init_soloader },
    { "strncpy", (uintptr_t)&strncpy_f },
    { "glViewport", (uintptr_t)&glViewport_dbg },
    { "glCompressedTexImage2D", (uintptr_t)&glCompressedTexImage2D_dbg },
    { "strcat", (uintptr_t)&strcat },
    { "fputc", (uintptr_t)&fputc },
    { "ALooper_addFd", (uintptr_t)&ret1 },
    { "fputs", (uintptr_t)&fputs },
    { "glGetProgramBinaryOES", (uintptr_t)&glGetProgramBinaryOES_dbg },
    { "glGetString", (uintptr_t)&glGetString },
    { "stat", (uintptr_t)&stat },
    { "floorf", (uintptr_t)&floorf },
    { "__android_log_print", (uintptr_t)&__android_log_print },
    { "FMOD_EventSystem_Create", (uintptr_t)&FMOD_EventSystem_Create_dbg },
    { "atol", (uintptr_t)&atol },
    { "AKeyEvent_getAction", (uintptr_t)&ret1 },
    { "tanh", (uintptr_t)&tanh },
    { "ceil", (uintptr_t)&ceil_sfp },
    { "syscall", (uintptr_t)&ret0 },
    { "glDepthMask", (uintptr_t)&glDepthMask },
    { "__aeabi_fsub", (uintptr_t)&__aeabi_fsub },
    // { "getChannelsPlaying", (uintptr_t)&ret0 },
    // { "setValue", (uintptr_t)&ret0 },
    { "vsprintf", (uintptr_t)&vsprintf },
    { "atan", (uintptr_t)&atan_sfp },
    { "connect", (uintptr_t)&ret0 },
    { "ungetc", (uintptr_t)&ungetc },
    { "FMOD_Memory_Initialize", (uintptr_t)&FMOD_Memory_Initialize },
    { "snprintf", (uintptr_t)&snprintf },
    { "shutdown", (uintptr_t)&ret0 },
    { "strlcpy", (uintptr_t)&strlcpy },
    { "glCheckFramebufferStatus", (uintptr_t)&glCheckFramebufferStatus },
    { "sem_wait", (uintptr_t)&sem_wait_soloader },
    { "glProgramBinaryOES", (uintptr_t)&glProgramBinaryOES_dbg },
    { "glEnable", (uintptr_t)&glEnable_dbg },
    { "glClear", (uintptr_t)&glClear_dbg },
    { "strcoll", (uintptr_t)&strcoll },
    { "strtok", (uintptr_t)&strtok },
    { "isupper", (uintptr_t)&isupper },
    { "AMotionEvent_getAxisValue", (uintptr_t)&ret1 },
    
    { "iswspace", (uintptr_t)&ret0 },
    { "nice", (uintptr_t)&nice_f },
    { "clock_gettime", (uintptr_t)&clock_gettime },
    // { "getSyncPointInfo", (uintptr_t)&ret0 },
    { "glScissor", (uintptr_t)&glScissor },
    // { "getParameterByIndex", (uintptr_t)&ret0 },
    { "fflush", (uintptr_t)&fflush },
    // { "set3DAttributes", (uintptr_t)&ret0 },
    { "free", (uintptr_t)&my_free },
    { "toupper", (uintptr_t)&toupper },
    
    { "AMotionEvent_getAction", (uintptr_t)&ret1 },
    { "pthread_join", (uintptr_t)&pthread_join_soloader },
    { "atan2", (uintptr_t)&atan2_sfp },
    { "ANativeWindow_setBuffersGeometry", (uintptr_t)&ret1 },
    { "sem_getvalue", (uintptr_t)&sem_getvalue_soloader },
    { "tanf", (uintptr_t)&tanf },
    { "strcasecmp", (uintptr_t)&strcasecmp },
    { "glReadPixels", (uintptr_t)&glReadPixels },
    { "abort", (uintptr_t)&abort },
    { "modf", (uintptr_t)&modf },
    { "gmtime", (uintptr_t)&gmtime },
    { "sqrtf", (uintptr_t)&sqrtf },
    { "glGenFramebuffers", (uintptr_t)&glGenFramebuffers },
    { "_Unwind_VRS_Set", (uintptr_t)&ret0 },
    { "__cxa_atexit", (uintptr_t)&__cxa_atexit },
    // { "getNumChannels", (uintptr_t)&ret0 },
    { "AInputEvent_getType", (uintptr_t)&ret1 },
    { "sscanf", (uintptr_t)&sscanf },
    { "AInputQueue_getEvent", (uintptr_t)&ret1 },
    
    { "sin", (uintptr_t)&sin_sfp },
    { "remove", (uintptr_t)&remove },
    { "glBindRenderbuffer", (uintptr_t)&glBindRenderbuffer },
    { "__aeabi_fcmpge", (uintptr_t)&__aeabi_fcmpge },
    { "atoi", (uintptr_t)&atoi },
    { "glBlendEquation", (uintptr_t)&glBlendEquation },
    { "glActiveTexture", (uintptr_t)&glActiveTexture },
    { "exp", (uintptr_t)&exp },
    // { "getAdvancedSettings", (uintptr_t)&ret0 },
    { "isspace", (uintptr_t)&isspace },
    { "glStencilMask", (uintptr_t)&glStencilMask },
    { "sleep", (uintptr_t)&sleep },
    { "sqrt", (uintptr_t)&sqrt_sfp },
    { "fread", (uintptr_t)&fread },
    { "glFramebufferTexture2D", (uintptr_t)&glFramebufferTexture2D },
    { "listen", (uintptr_t)&ret0 },
    { "__aeabi_d2uiz", (uintptr_t)&__aeabi_d2ulz },
    { "glDepthRangef", (uintptr_t)&glDepthRangef_sfp },
    { "AMotionEvent_getPointerCount", (uintptr_t)&ret1 },
    { "sem_post", (uintptr_t)&sem_post_soloader },
    { "__aeabi_dmul", (uintptr_t)&__aeabi_dmul },
    { "qsort", (uintptr_t)&qsort },
    { "glStencilFunc", (uintptr_t)&glStencilFunc },
    { "__aeabi_ldiv0", (uintptr_t)&__aeabi_ldiv0 },
    { "glAttachShader", (uintptr_t)&glAttachShader_dbg },
    { "pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_soloader },
    { "fwrite", (uintptr_t)&fwrite },
    { "wcscmp", (uintptr_t)&strcmp },
    { "__stack_chk_fail", (uintptr_t)&__stack_chk_fail_soloader },
    { "memset", (uintptr_t)&memset },
    { "glFramebufferRenderbuffer", (uintptr_t)&glFramebufferRenderbuffer },
    { "strcspn", (uintptr_t)&strcspn },
    // { "set3DNumListeners", (uintptr_t)&ret0 },
    { "FMOD_Debug_SetLevel", (uintptr_t)&FMOD_Debug_SetLevel_dbg },
    { "glGetFloatv", (uintptr_t)&glGetFloatv_sfp },
    { "setsockopt", (uintptr_t)&ret0 },
    { "pthread_mutexattr_settype", (uintptr_t)&pthread_mutexattr_settype_soloader },
    { "_Unwind_GetRegionStart", (uintptr_t)&ret0 },
    { "glTexSubImage2D", (uintptr_t)&glTexSubImage2D },
    { "glTexParameteri", (uintptr_t)&glTexParameteri_dbg },
    { "exp2", (uintptr_t)&exp2 },
    { "AConfiguration_fromAssetManager", (uintptr_t)&ret1 },
    { "glBindBuffer", (uintptr_t)&glBindBuffer },
    { "strtol", (uintptr_t)&strtol },
    { "__aeabi_f2d", (uintptr_t)&__aeabi_f2d },
    { "acosf", (uintptr_t)&acosf },
    { "sendto", (uintptr_t)&ret0 },
    { "glStencilOp", (uintptr_t)&glStencilOp },
    { "recvfrom", (uintptr_t)&ret0 },
    { "__assert2", (uintptr_t)&ret0 },
    { "ALooper_pollAll", (uintptr_t)&ALooper_pollAll },
    { "sem_trywait", (uintptr_t)&sem_trywait_soloader },
    { "__aeabi_idivmod", (uintptr_t)&__aeabi_idivmod },
    { "glBufferSubData", (uintptr_t)&glBufferSubData },
    { "glDisableVertexAttribArray", (uintptr_t)&glDisableVertexAttribArray_dbg },
    { "__aeabi_i2f", (uintptr_t)&__aeabi_l2f },
    { "malloc", (uintptr_t)&my_malloc },
    { "memchr", (uintptr_t)&memchr },
    { "glGetRenderbufferParameteriv", (uintptr_t)&glGetBufferParameteriv },
    { "glClearStencil", (uintptr_t)&glClearStencil_dbg },
    { "AMotionEvent_getX", (uintptr_t)&ret1 },
    { "glUniform3fv", (uintptr_t)&glUniform3fv_sfp },
    { "__aeabi_uidivmod", (uintptr_t)&__aeabi_uidivmod },
    { "read", (uintptr_t)&read },
    { "glUniform4fv", (uintptr_t)&glUniform4fv_sfp },
    { "pthread_exit", (uintptr_t)&pthread_exit_fake },
    { "memcmp", (uintptr_t)&memcmp },
    // { "getSystemObject", (uintptr_t)&ret0 },
    { "asin", (uintptr_t)&asin_sfp },
    { "glGenRenderbuffers", (uintptr_t)&glGenRenderbuffers },
    { "strerror", (uintptr_t)&strerror },
    // { "update", (uintptr_t)&ret0 },
    { "glDeleteProgram", (uintptr_t)&glDeleteProgram },
    { "glBlendFuncSeparate", (uintptr_t)&glBlendFuncSeparate },
    // { "getMemoryInfo", (uintptr_t)&ret0 },
    { "strncasecmp", (uintptr_t)&strncasecmp },
    // { "getEventBySystemID", (uintptr_t)&ret0 },
    { "time", (uintptr_t)&time },
    { "AMotionEvent_getPointerId", (uintptr_t)&ret1 },
    // { "setCallback", (uintptr_t)&ret0 },
    { "tan", (uintptr_t)&tan_sfp },
    { "log", (uintptr_t)&log_sfp },
    { "glGetIntegerv", (uintptr_t)&glGetIntegerv },
    { "glMapBufferOES", (uintptr_t)&glMapBufferOES_dbg },
    // { "getChannel", (uintptr_t)&ret0 },
    { "__gnu_unwind_frame", (uintptr_t)&ret0 },
    { "glFinish", (uintptr_t)&glFinish },
    { "glShaderSource", (uintptr_t)&glShaderSource_dbg },
    { "glUniform1i", (uintptr_t)&glUniform1i_dbg },
    { "_Unwind_Resume", (uintptr_t)&ret0 },
    { "glGetProgramiv", (uintptr_t)&glGetProgramiv_dbg },
    // { "start", (uintptr_t)&ret0 },
    { "frexp", (uintptr_t)&frexp },
    { "__aeabi_dadd", (uintptr_t)&__aeabi_fadd },
    { "srand48", (uintptr_t)&srand48 },
    { "sprintf", (uintptr_t)&sprintf },
    { "__gnu_ldivmod_helper", (uintptr_t)&ret0 },
    { "fmodf", (uintptr_t)&fmodf },
    { "_Unwind_GetDataRelBase", (uintptr_t)&ret0 },
    { "__aeabi_memcpy", (uintptr_t)&__aeabi_memcpy },
    { "fmod", (uintptr_t)&fmod_sfp },
    { "pthread_attr_setstacksize", (uintptr_t)&pthread_attr_setstacksize_soloader },
    { "glDeleteRenderbuffers", (uintptr_t)&glDeleteRenderbuffers },
    { "glIsRenderbuffer", (uintptr_t)&glIsRenderbuffer },
    // { "setDSPBufferSize", (uintptr_t)&ret0 },
    { "__aeabi_i2d", (uintptr_t)&__aeabi_i2d },
    { "strstr", (uintptr_t)&strstr },
    { "glDisable", (uintptr_t)&glDisable },
    { "strdup", (uintptr_t)&strdup },
    { "wcslen", (uintptr_t)&strlen },
    { "_Unwind_RaiseException", (uintptr_t)&ret0 },
    { "pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_soloader },
    { "wcscoll", (uintptr_t)&strcoll },
    // { "getState", (uintptr_t)&ret0 },
    // { "setPaused", (uintptr_t)&ret0 },
    { "sched_yield", (uintptr_t)&sched_yield },
    { "__aeabi_ul2f", (uintptr_t)&__aeabi_ul2f },
    { "getenv", (uintptr_t)&getenv },
    { "pipe", (uintptr_t)&ret0 },
    { "accept", (uintptr_t)&ret0 },
    { "glRenderbufferStorage", (uintptr_t)&glRenderbufferStorage },
    { "close", (uintptr_t)&close },
    { "pthread_attr_destroy", (uintptr_t)&pthread_attr_destroy_soloader },
    { "log10", (uintptr_t)&log10 },
    { "glUniform2f", (uintptr_t)&glUniform2f_sfp },
    { "bind", (uintptr_t)&ret0 },
    { "glVertexAttribPointer", (uintptr_t)&glVertexAttribPointer },
    { "pow", (uintptr_t)&pow_sfp },
    { "floor", (uintptr_t)&floor_sfp },
    { "AConfiguration_getLanguage", (uintptr_t)&ret1 },
    { "pthread_setspecific", (uintptr_t)&pthread_setspecific },
    { "powf", (uintptr_t)&powf_sfp },
    { "glBindAttribLocation", (uintptr_t)&glBindAttribLocation_dbg },
    { "sinf", (uintptr_t)&sinf },
    { "glGetFramebufferAttachmentParameteriv", (uintptr_t)&glGetFramebufferAttachmentParameteriv },
    { "glCreateProgram", (uintptr_t)&glCreateProgram },
    { "glDeleteTextures", (uintptr_t)&glDeleteTextures },
    { "select", (uintptr_t)&select },
    { "_Unwind_Complete", (uintptr_t)&ret0 },
    { "glReleaseShaderCompiler", (uintptr_t)&glReleaseShaderCompiler },
    { "strpbrk", (uintptr_t)&strpbrk },
    
    { "strlen", (uintptr_t)&strlen_f },
    { "__aeabi_ui2d", (uintptr_t)&__aeabi_ui2d },
    { "lrand48", (uintptr_t)&lrand48 },
    { "AInputQueue_preDispatchEvent", (uintptr_t)&ret1 },
    { "glGetError", (uintptr_t)&glGetError },
    { "memmove", (uintptr_t)&memmove },
    { "_Unwind_GetLanguageSpecificData", (uintptr_t)&ret0 },
    { "__aeabi_fcmpeq", (uintptr_t)&__aeabi_fcmpeq },
    { "fprintf", (uintptr_t)&fprintf },
    { "glTexImage2D", (uintptr_t)&glTexImage2D_dbg },
    { "glLinkProgram", (uintptr_t)&glLinkProgram_dbg },
    // { "stop", (uintptr_t)&ret0 },
    { "glIsProgram", (uintptr_t)&ret0 },
    // { "setReverbAmbientProperties", (uintptr_t)&ret0 },
    { "AConfiguration_getCountry", (uintptr_t)&ret1 },
    { "__aeabi_dcmpge", (uintptr_t)&__aeabi_dcmpge },
    { "pthread_attr_setschedparam", (uintptr_t)&pthread_attr_setschedparam },
    { "sem_init", (uintptr_t)&sem_init_soloader },
    { "glDeleteShader", (uintptr_t)&glDeleteShader },
    { "sem_destroy", (uintptr_t)&sem_destroy_soloader },
    { "__aeabi_ul2d", (uintptr_t)&__aeabi_ul2d },
    { "strncat", (uintptr_t)&strncat },
    { "pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_soloader },
    // { "load", (uintptr_t)&ret0 },
    { "atan2f", (uintptr_t)&atan2f },
    { "isxdigit", (uintptr_t)&isxdigit },
    { "__aeabi_dsub", (uintptr_t)&__aeabi_dsub },
    { "glDeleteBuffers", (uintptr_t)&glDeleteBuffers },
    { "printf", (uintptr_t)&printf },
    { "ftell", (uintptr_t)&ftell },
    { "asinf", (uintptr_t)&asinf },
    { "exit", (uintptr_t)&exit },
    { "glClearDepthf", (uintptr_t)&glClearDepthf_sfp },
    { "AMotionEvent_getSize", (uintptr_t)&ret1 },
    { "glBlendFunc", (uintptr_t)&glBlendFunc },
    { "glUniformMatrix4fv", (uintptr_t)&glUniformMatrix4fv_sfp },
    { "glDrawArrays", (uintptr_t)&glDrawArrays },
    
    { "sinh", (uintptr_t)&sinh },
    { "__aeabi_idiv", (uintptr_t)&__aeabi_idiv },
    { "glBindTexture", (uintptr_t)&glBindTexture_dbg },
    { "__aeabi_fcmpgt", (uintptr_t)&__aeabi_fcmpgt },
    { "pthread_cond_signal", (uintptr_t)&pthread_cond_signal_soloader},
    { "logf", (uintptr_t)&logf },
    { "AMotionEvent_getPressure", (uintptr_t)&ret1 },
    { "strrchr", (uintptr_t)&strrchr },
    { "AConfiguration_new", (uintptr_t)&ret1 },
    { "AInputQueue_finishEvent", (uintptr_t)&ret1 },
    { "glCullFace", (uintptr_t)&glCullFace },
    { "gettimeofday", (uintptr_t)&gettimeofday },
    { "__aeabi_ui2f", (uintptr_t)&__aeabi_ui2f },
    { "__aeabi_uldivmod", (uintptr_t)&__aeabi_uldivmod},
    { "pthread_attr_setdetachstate", (uintptr_t)&pthread_attr_setdetachstate_soloader },
    // { "getNumEvents", (uintptr_t)&ret0 },
    { "strcmp", (uintptr_t)&strcmp },
    { "__aeabi_ddiv", (uintptr_t)&__aeabi_ddiv },
    { "fcntl", (uintptr_t)&ret0 },
    { "__aeabi_dcmpeq", (uintptr_t)&__aeabi_dcmpeq },
    // { "getProjectByIndex", (uintptr_t)&ret0 },
    { "longjmp", (uintptr_t)&longjmp },
    { "inet_addr", (uintptr_t)&ret0 },
    
    { "__stack_chk_guard", (uintptr_t)&__stack_chk_guard_fake },
    
    { "_ZN4FMOD5Sound12getSyncPointEiPP14FMOD_SYNCPOINT", (uintptr_t)&_ZN4FMOD5Sound12getSyncPointEiPP14FMOD_SYNCPOINT },
{ "_ZN4FMOD5Event11getCategoryEPPNS_13EventCategoryE", (uintptr_t)&_ZN4FMOD5Event11getCategoryEPPNS_13EventCategoryE },
{ "_ZN4FMOD11EventSystem18getCategoryByIndexEiPPNS_13EventCategoryE", (uintptr_t)&_ZN4FMOD11EventSystem18getCategoryByIndexEiPPNS_13EventCategoryE },
{ "_ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKciPjPPvS6_EPFS1_S5_S5_EPFS1_S5_S5_jS4_S5_EPFS1_S5_jS5_EPFS1_P18FMOD_ASYNCREADINFOS5_ESA_i", (uintptr_t)&_ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKciPjPPvS6_EPFS1_S5_S5_EPFS1_S5_S5_jS4_S5_EPFS1_S5_jS5_EPFS1_P18FMOD_ASYNCREADINFOS5_ESA_i },
{ "_ZN4FMOD5Event7getInfoEPiPPcP15FMOD_EVENT_INFO", (uintptr_t)&_ZN4FMOD5Event7getInfoEPiPPcP15FMOD_EVENT_INFO },
{ "_ZN4FMOD5Event9setVolumeEf", (uintptr_t)&_ZN4FMOD5Event9setVolumeEf },
{ "_ZN4FMOD11EventSystem4initEijPvj", (uintptr_t)&_ZN4FMOD11EventSystem4initEijPvj },
{ "_ZN4FMOD11EventSystem22getReverbPresetByIndexEiP22FMOD_REVERB_PROPERTIESPPc", (uintptr_t)&_ZN4FMOD11EventSystem22getReverbPresetByIndexEiP22FMOD_REVERB_PROPERTIESPPc },
{ "_ZN4FMOD11EventSystem7releaseEv", (uintptr_t)&_ZN4FMOD11EventSystem7releaseEv },
{ "_ZN4FMOD6System19setAdvancedSettingsEP21FMOD_ADVANCEDSETTINGS", (uintptr_t)&_ZN4FMOD6System19setAdvancedSettingsEP21FMOD_ADVANCEDSETTINGS },
{ "_ZN4FMOD11EventSystem23set3DListenerAttributesEiPK11FMOD_VECTORS3_S3_S3_", (uintptr_t)&_ZN4FMOD11EventSystem23set3DListenerAttributesEiPK11FMOD_VECTORS3_S3_S3_ },
{ "_ZN4FMOD11EventSystem11getCategoryEPKcPPNS_13EventCategoryE", (uintptr_t)&_ZN4FMOD11EventSystem11getCategoryEPKcPPNS_13EventCategoryE },
{ "_ZN4FMOD11EventSystem20getEventByGUIDStringEPKcjPPNS_5EventE", (uintptr_t)&_ZN4FMOD11EventSystem20getEventByGUIDStringEPKcjPPNS_5EventE },
{ "_ZN4FMOD5Event13getMemoryInfoEjjPjP25FMOD_MEMORY_USAGE_DETAILS", (uintptr_t)&_ZN4FMOD5Event13getMemoryInfoEjjPjP25FMOD_MEMORY_USAGE_DETAILS },
{ "_ZN4FMOD11EventSystem19setReverbPropertiesEPK22FMOD_REVERB_PROPERTIES", (uintptr_t)&_ZN4FMOD11EventSystem19setReverbPropertiesEPK22FMOD_REVERB_PROPERTIES },
{ "_ZN4FMOD5Event15getChannelGroupEPPNS_12ChannelGroupE", (uintptr_t)&_ZN4FMOD5Event15getChannelGroupEPPNS_12ChannelGroupE },
{ "_ZN4FMOD7Channel11setPositionEjj", (uintptr_t)&_ZN4FMOD7Channel11setPositionEjj },
{ "_ZN4FMOD6System18getChannelsPlayingEPi", (uintptr_t)&_ZN4FMOD6System18getChannelsPlayingEPi },
{ "_ZN4FMOD14EventParameter8setValueEf", (uintptr_t)&_ZN4FMOD14EventParameter8setValueEf },
{ "_ZN4FMOD5Sound16getSyncPointInfoEP14FMOD_SYNCPOINTPciPjj", (uintptr_t)&_ZN4FMOD5Sound16getSyncPointInfoEP14FMOD_SYNCPOINTPciPjj },
{ "_ZN4FMOD5Event19getParameterByIndexEiPPNS_14EventParameterE", (uintptr_t)&_ZN4FMOD5Event19getParameterByIndexEiPPNS_14EventParameterE },
{ "_ZN4FMOD5Event15set3DAttributesEPK11FMOD_VECTORS3_S3_", (uintptr_t)&_ZN4FMOD5Event15set3DAttributesEPK11FMOD_VECTORS3_S3_ },
{ "_ZN4FMOD12ChannelGroup14getNumChannelsEPi", (uintptr_t)&_ZN4FMOD12ChannelGroup14getNumChannelsEPi },
{ "_ZN4FMOD6System19getAdvancedSettingsEP21FMOD_ADVANCEDSETTINGS", (uintptr_t)&_ZN4FMOD6System19getAdvancedSettingsEP21FMOD_ADVANCEDSETTINGS },
{ "_ZN4FMOD11EventSystem17set3DNumListenersEi", (uintptr_t)&_ZN4FMOD11EventSystem17set3DNumListenersEi },
{ "_ZN4FMOD11EventSystem15getSystemObjectEPPNS_6SystemE", (uintptr_t)&_ZN4FMOD11EventSystem15getSystemObjectEPPNS_6SystemE },
{ "_ZN4FMOD11EventSystem6updateEv", (uintptr_t)&_ZN4FMOD11EventSystem6updateEv },
{ "_ZN4FMOD6System13getMemoryInfoEjjPjP25FMOD_MEMORY_USAGE_DETAILS", (uintptr_t)&_ZN4FMOD6System13getMemoryInfoEjjPjP25FMOD_MEMORY_USAGE_DETAILS },
{ "_ZN4FMOD11EventSystem18getEventBySystemIDEjjPPNS_5EventE", (uintptr_t)&_ZN4FMOD11EventSystem18getEventBySystemIDEjjPPNS_5EventE },
{ "_ZN4FMOD5Event11setCallbackEPF11FMOD_RESULTP10FMOD_EVENT23FMOD_EVENT_CALLBACKTYPEPvS5_S5_ES5_", (uintptr_t)&_ZN4FMOD5Event11setCallbackEPF11FMOD_RESULTP10FMOD_EVENT23FMOD_EVENT_CALLBACKTYPEPvS5_S5_ES5_ },
{ "_ZN4FMOD12ChannelGroup10getChannelEiPPNS_7ChannelE", (uintptr_t)&_ZN4FMOD12ChannelGroup10getChannelEiPPNS_7ChannelE },
{ "_ZN4FMOD5Event5startEv", (uintptr_t)&_ZN4FMOD5Event5startEv },

{ "_ZN4FMOD6System16setDSPBufferSizeEji", (uintptr_t)&_ZN4FMOD6System16setDSPBufferSizeEji },
{ "_ZN4FMOD5Event8getStateEPj", (uintptr_t)&_ZN4FMOD5Event8getStateEPj },
{ "_ZN4FMOD5Event9setPausedEb", (uintptr_t)&_ZN4FMOD5Event9setPausedEb },
{ "_ZN4FMOD5Event4stopEb", (uintptr_t)&_ZN4FMOD5Event4stopEb },
{ "_ZN4FMOD11EventSystem26setReverbAmbientPropertiesEP22FMOD_REVERB_PROPERTIES", (uintptr_t)&_ZN4FMOD11EventSystem26setReverbAmbientPropertiesEP22FMOD_REVERB_PROPERTIES },
{ "_ZN4FMOD11EventSystem4loadEPKcP19FMOD_EVENT_LOADINFOPPNS_12EventProjectE", (uintptr_t)&_ZN4FMOD11EventSystem4loadEPKcP19FMOD_EVENT_LOADINFOPPNS_12EventProjectE },
{ "_ZN4FMOD11EventSystem12getNumEventsEPi", (uintptr_t)&_ZN4FMOD11EventSystem12getNumEventsEPi },
{ "_ZN4FMOD11EventSystem17getProjectByIndexEiPPNS_12EventProjectE", (uintptr_t)&_ZN4FMOD11EventSystem17getProjectByIndexEiPPNS_12EventProjectE },


};

int check_kubridge(void) {
  int search_unk[2];
  return _vshKernelSearchModuleByName("kubridge", search_unk);
}

int file_exists(const char *path) {
  SceIoStat stat;
  return sceIoGetstat(path, &stat) >= 0;
}

#ifdef WITH_SOUND
static SceUID NGSHook = -1;
static tai_hook_ref_t NGSHook_ref;
static tai_hook_ref_t NGSHook2_ref;
static tai_hook_ref_t NGSHook3_ref;
static tai_hook_ref_t AudioOpenHook_ref;

//The following code makes sure FMOD is able to initialize without issues, so when turning the hooks off, fmod won't do anything

unsigned char DAT_81009c64[64] = {
    0x01, 0x00, 0x00, 0x00, 0x14, 0x95, 0x00, 0x81, 0x03, 0x00, 0x00, 0x00, 0x68, 0xA4, 0x00, 0x81, 
    0x02, 0x00, 0x00, 0x00, 0x30, 0x95, 0x00, 0x81, 0x05, 0x00, 0x00, 0x00, 0xD4, 0xA1, 0x00, 0x81, 
    0x03, 0x00, 0x00, 0x00, 0x3C, 0x95, 0x00, 0x81, 0x02, 0x00, 0x00, 0x00, 0x7C, 0xA2, 0x00, 0x81, 
    0x04, 0x00, 0x00, 0x00, 0x4C, 0x95, 0x00, 0x81, 0x02, 0x00, 0x00, 0x00, 0xFC, 0xA6, 0x00, 0x81
};
typedef struct SceNgsSystemInitParams
{
    SceInt32 nMaxRacks;      /* Maximum number of Racks within the Stage */
    SceInt32 nMaxVoices;     /* Maximum number of active voices */
    SceInt32 nGranularity;   /* PCM sample granularity (NGS will process and output PCM sample packets of this size) */
    SceInt32 nSampleRate;    /* Base sample rate */
    SceInt32 nMaxModules;    /* Maximum number of module types which are available for the whole system. */
} SceNgsSystemInitParams;
SceNgsSystemInitParams init_param;
int memSize;
int sceNgsSystemGetRequiredMemorySize_dbg(void* pSynthParams, int* pnSize) {
  printf("sceNgsSystemGetRequiredMemorySize_dbg\n");
  // pSynthParams = &DAT_81009c64;
  // *pnSize = 0x3f;
  uint32_t sys_size;
  
  init_param.nMaxRacks = 64;
  init_param.nMaxVoices = 64;
  init_param.nGranularity = 512;
  init_param.nSampleRate = 48000;
  init_param.nMaxModules = 14;
  int ret = TAI_CONTINUE(SceUID, NGSHook_ref, &init_param, pnSize);
  memSize = pnSize;
  // pnSize = sys_size;
  return ret;
}
int sceNgsSystemInit_dbg(void* pSynthSysMemory, const SceUInt32 uMemSize, void* pSynthParams, void* pSystemHandle) {
   int ret = TAI_CONTINUE(SceUID, NGSHook2_ref, pSynthSysMemory, uMemSize, &init_param, pSystemHandle);
   printf("sceNgsSystemInit %d\n", ret);
   return ret;
}

int sceNgsVoiceGetStateData_dbg(void* hVoiceHandle, const SceUInt32 uModule, void* pMem, const SceUInt32 uMemSize) {
  // printf("s %d mod %d\n", uMemSize, uModule);
  // if(uMemSize == 24) return 0;
  int memsize = uMemSize;
  if(uModule == 1) memsize = sizeof(short) * 512 * 2;
  
   int ret = TAI_CONTINUE(SceUID, NGSHook3_ref, hVoiceHandle, uModule, pMem, memsize);
    return ret;
}


static SceUID LoadModuleHook = -1;
static tai_hook_ref_t LoadModuleHook_ref;
SceUID sceKernelLoadStartModule_p(int id)
{
	sceClibPrintf("Starting Module: %d\n",id);
	
	SceUID ret;
	ret = TAI_CONTINUE(SceUID, LoadModuleHook_ref, id);
	
	if(id == 11) //NGS
	{
		taiHookFunctionImport(&NGSHook_ref, 
									"libfmodex",
									0xB01598D9, //sceNgsSystemGetRequiredMemorySize
                                    0x6CE8B36F,
									sceNgsSystemGetRequiredMemorySize_dbg);
          taiHookFunctionImport(&NGSHook2_ref, 
									"libfmodex",
									0xB01598D9, 
                                    0xED14CF4A, //sceNgsSystemInit
									sceNgsSystemInit_dbg);
          taiHookFunctionImport(&NGSHook3_ref, 
									"libfmodex",
									0xB01598D9, 
                                    0xC9B8C0B4, //sceNgsVoiceGetStateData
									sceNgsVoiceGetStateData_dbg);
          
        
		// sceClibPrintf("sceNgsSystemGetRequiredMemorySize %x\n",NGSHook_ref);
	}
	
	return ret;
}

int sceAudioOutOpenPort_dbg(int	type,int len,int freq,int mode) {
    int ret = TAI_CONTINUE(SceUID, AudioOpenHook_ref, type, 512, freq, mode);
    return ret;
}
#endif
void real_main() {
    if (check_kubridge() < 0)
    fatal_error("Error kubridge.skprx is not installed.");

  if (!file_exists("ur0:/data/libshacccg.suprx") && !file_exists("ur0:/data/external/libshacccg.suprx"))
    fatal_error("Error libshacccg.suprx is not installed.");

  if (so_file_load(&bc2_mod, SO_PATH, LOAD_ADDRESS) < 0)
    fatal_error("Error could not load %s.", SO_PATH);
printf("load\n");
  so_relocate(&bc2_mod);
  printf("so_relocate\n");
  so_resolve(&bc2_mod, dynlib_functions, sizeof(dynlib_functions), 1);
  printf("so_resolve\n");
  patch_game();
  // so_flush_caches(&bc2_mod);
  printf("so_flush_caches\n");

  so_initialize(&bc2_mod);
  printf("so_initialize\n");
#ifdef WITH_SOUND
  sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
  int ret = sceNetShowNetstat();
  SceNetInitParam initparam;
    
  if (ret == SCE_NET_ERROR_ENOTINIT) {
      initparam.memory = malloc(141 * 1024);
      initparam.size = 141 * 1024;
      initparam.flags = 0;
      sceNetInit(&initparam);
  }
#endif
    sceKernelLoadStartModule("vs0:sys/external/libfios2.suprx", 0, NULL, 0, NULL, NULL);
	sceKernelLoadStartModule("vs0:sys/external/libc.suprx", 0, NULL, 0, NULL, NULL);
    // sceKernelLoadStartModule("vs0:sys/external/libngs.suprx", 0, NULL, 0, NULL, NULL);
     sceKernelLoadStartModule("app0:module/libfmodex_dt.suprx", 0, NULL, 0, NULL, NULL);
#ifdef WITH_SOUND
    LoadModuleHook = taiHookFunctionImport(&LoadModuleHook_ref, 
										  "libfmodex",
										  0x03FCF19D,
										  0x79A0160A, //sceKernelLoadStartModule
										  sceKernelLoadStartModule_p);
    
    taiHookFunctionImport(&AudioOpenHook_ref, 
										  "libfmodex",
										  0x438BB957,
										  0x5BC341E4, //sceKernelLoadStartModule
										  sceAudioOutOpenPort_dbg);
#endif
#ifdef PVR2
    //from https://github.com/SonicMastr/godot-vita/blob/3.x/platform/vita/godot_vita.cpp#L61
    char title_id[0xA];
	char app_dir_path[0x100];
	char app_kernel_module_path[0x100];
	SceUID pid = -1;
    pid = sceKernelGetProcessId();
	sceAppMgrAppParamGetString(pid, 12, title_id, sizeof(title_id));
	snprintf(app_dir_path, sizeof(app_dir_path), "ux0:app/%s", title_id);
	snprintf(app_kernel_module_path, sizeof(app_kernel_module_path), "%s/module/libgpu_es4_kernel_ext.skprx", app_dir_path);

	SceUID res = taiLoadStartKernelModule(app_kernel_module_path, 0, NULL, 0);
	if (res < 0) {
		sceClibPrintf("Failed to load kernel module: %08x\n", res);
	}
    
    
    char *default_path = "app0:module";
    sceKernelLoadStartModule("app0:module/libgpu_es4_ext.suprx", 0, NULL, 0, NULL, NULL);
  	sceKernelLoadStartModule("app0:module/libIMGEGL.suprx", 0, NULL, 0, NULL, NULL);
    // sceKernelLoadStartModule("app0:module/libGLESv2.suprx", 0, NULL, 0, NULL, NULL);
    PVRSRV_PSP2_APPHINT hint;
    // memset(&hint, 0, sizeof(hint));
  	PVRSRVInitializeAppHint(&hint);
    // hint.bDisableAsyncTextureOp = EGL_TRUE;
    // hint.bEnableMemorySpeedTest = EGL_FALSE;
    // hint.bDisableHWTQBufferBlit = EGL_TRUE;
    // hint.ui32SwTexOpCleanupDelay = 16000;
    // hint.ui32DriverMemorySize = 8 * 1024 * 1024;
    /*
    hint.ui32UNCTexHeapSize = 60 * 1024 * 1024;
    hint.ui32CDRAMTexHeapSize = 96 * 1024 * 1024;
    hint.bDisableHWTQBufferBlit = EGL_TRUE;
    hint.ui32PDSFragBufferSize = 1024 * 1024;
    hint.bDisableUSEASMOPT = EGL_TRUE;
	// hint.bDumpShaders = EGL_FALSE;
	hint.bEnableAppTextureDependency = EGL_TRUE;
	hint.ui32ParamBufferSize = 32 * 1024 * 1024;
	hint.bEnableStaticMTECopy = EGL_FALSE;
	hint.bEnableStaticPDSVertex = EGL_FALSE;*/
    snprintf(hint.szGLES1, 255, "%s/%s", default_path, "libGLESv1_CM.suprx");
    snprintf(hint.szGLES2, 255, "%s/%s", default_path, "libGLESv2.suprx");
    snprintf(hint.szWindowSystem, 255, "%s/%s", default_path, "libpvrPSP2_WSEGL.suprx");
  	PVRSRVCreateVirtualAppHint(&hint);
    // eglInit(EGL_DEFAULT_DISPLAY, 0);
    // rinit();
    // while(1) { render(); };
    // int width, height;
    // image = stbi_load("app0:texture.png", &width, &height, 0, 0);
#endif
#ifdef PIB
    
  pibInit(PIB_SHACCCG); 
#endif
#ifndef ANGLE
  getOESExtensionFunctions();
#endif
  void (* fmod_is_foreground)(void) = (void *)so_symbol(&bc2_mod, "NCT_AndroidFmodForeground");
  fmod_is_foreground();
  items = (void **)malloc(255 * sizeof(void *));
  memset(items, 0, 255 * sizeof(void *));

  // prepare_jni_dyn_fields();
  sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
  jni_load();

  return 0;
}


int main(int argc, char *argv[]) {
  // sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
  // sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  // scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);
  
  
  g_myPhycontBlk = sceKernelAllocMemBlock(PHYCONT_BLK_NAME, SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, k_myPhycontSize, NULL);

  if (g_myPhycontBlk < 0) {
    printf("\"" PHYCONT_BLK_NAME "\" alloc failed\n");
    abort();
  }

  sceKernelGetMemBlockBase(g_myPhycontBlk, &g_myPhycont);
  sceClibMspaceCreate(g_myPhycont, k_myPhycontSize);
  

//   

/*
taiHookFunctionImport(&NGSHook_ref, 
										  "sceNgs",
										  TAI_ANY_LIBRARY,
										  0xF2B759C1, //sceNgsSystemGetRequiredMemorySize
										  sceNgsSystemGetRequiredMemorySize_dbg);
*/

  android_app = malloc(100);
  
  memset(android_app, 0, sizeof(android_app));
  memset(android_app+64, 1, 1);
  
	pthread_t t;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 0x400000);
	pthread_create(&t, &attr, real_main, NULL);
    // free(android_app);
    return sceKernelExitDeleteThread(0);
}

