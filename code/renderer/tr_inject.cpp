// tr_inject.cpp
//

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_gamerender.h"
#include "../libs/injector/MinHook.h"

HWND(*game_window) = (HWND *)0x11367190;

void (*RB_NotUsedHook)(void);
void (*RB_ARB2_DrawInteractionEngine)(drawInteraction_t *din);
void (*R_LoadImageEngine)(const char *name, byte **pic, int *width, int *height, unsigned int *timestamp);
void(*RB_STD_DrawViewEngine)(void);
void(*RB_T_FillDepthBufferEngine)(const drawSurf_t *surf);
void(__fastcall *GenerateImageEngine)(idImage *_this, byte *notUsed, const byte *pic, int width, int height, textureFilter_t filter, bool allowDownSize, textureRepeat_t repeat, textureDepth_t depth, int unknown);;
void(*RB_BeginDrawingViewEngine)(void);
void *(*R_StaticAlloc)(int size) = (void *(__cdecl *)(int))0x10129100;
void *(__fastcall * idImage_GetDownsizeEngine)(void *_this, int &scaled_width, int &scaled_height);

void(*RB_DrawElementsWithCounters)(const srfTriangles_t *tri) = (void(__cdecl *)(const srfTriangles_t *))0x1012B320;

idImageManager *globalImages = *((idImageManager **)0x10316B10);

void R_MemPatch(char* dst, char* src, int size)
{
	DWORD oldprotect;
	VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldprotect);
	memcpy(dst, src, size);
	VirtualProtect(dst, size, oldprotect, &oldprotect);
}

HWND R_GetWindowHandle(void) {
	return *game_window;
}

/*
===================
R_NonPowerOfTwoHack
===================
*/
void R_NonPowerOfTwoHack(void) {
	char memPatch[6] = { 0xFF, 0x91, 0x88, 0x00, 0x00, 0x00 }; 
	
	// This function patches out generateImage throwing a error if we use non power of two.
	R_MemPatch((char *)0x100BA581, memPatch, 6);
}

/*
===================
R_DisableOldMipGen
===================
*/
void R_DisableOldMipGen(void) {
	char memPatch[6] = { 0x0F, 0x84, 0xD3, 0x00, 0x00, 0x00 };

	// Disable the flag for mip generation
	R_MemPatch((char *)0x100BA878, memPatch, 6);
}

int __fastcall RenderSystem_GetScreenWidth(idRenderSystem *thisPtr) {
	return vrConfig.openVRWidth;
}

int __fastcall RenderSystem_GetScreenHeight(idRenderSystem* thisPtr) {
	return vrConfig.openVRWidth;
}

void RB_BeginDrawingView(void);

/*
===================
R_InitInjection
===================
*/
void R_InitInjection(void) {
	common->Printf("R_InitInjections: Creating Injection Points...\n");
	
	MH_Initialize();

	{
		void* function = (void*)0x1012B0F0;
		MH_CreateHook(function, RB_BeginDrawingView, (LPVOID*)&RB_BeginDrawingViewEngine);
		MH_EnableHook(function);
	}

	{
		void* function = (void*)0x100EA950;
		MH_CreateHook(function, RenderSystem_GetScreenWidth, (LPVOID*)&RB_NotUsedHook);
		MH_EnableHook(function);
	}

	{
		void* function = (void*)0x100EA960;
		MH_CreateHook(function, RenderSystem_GetScreenHeight, (LPVOID*)&RB_NotUsedHook);
		MH_EnableHook(function);
	}

	{
		void *function = (void *)0x100A5790;
		MH_CreateHook(function, RB_ARB2_DrawInteraction, (LPVOID *)&RB_ARB2_DrawInteractionEngine);
		MH_EnableHook(function); 
	}

	{
		void *function = (void *)0x100A9EC0;
		MH_CreateHook(function, RB_STD_DrawView, (LPVOID *)&RB_STD_DrawViewEngine);
		MH_EnableHook(function);
	}

	{
		void *function = (void *)0x100AFA20;
		MH_CreateHook(function, R_LoadImage, (LPVOID *)&R_LoadImageEngine);
		MH_EnableHook(function);
	}

	{
		void *function = (void *)0x100B7A70;
		MH_CreateHook(function, idImage_GetDownsize, (LPVOID *)&idImage_GetDownsizeEngine);
		MH_EnableHook(function);
	}

	{
		void *function = (void *)0x100B7730;
		void(*notUsed)(void);
		MH_CreateHook(function, idImage::SelectInternalFormat, (LPVOID *)&notUsed);
		MH_EnableHook(function);
	}

	{
		void *function = (void *)0x100A7650;
		MH_CreateHook(function, RB_T_FillDepthBuffer, (LPVOID *)&RB_T_FillDepthBufferEngine);
		MH_EnableHook(function);
	}

	{
		void *function = (void *)0x100BA4B0;
		MH_CreateHook(function, idImage_GenerateImage, (LPVOID *)&GenerateImageEngine);
		MH_EnableHook(function);
	}

	R_NonPowerOfTwoHack();
	R_DisableOldMipGen();
}