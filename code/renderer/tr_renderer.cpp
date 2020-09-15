// tr_renderer.cpp
//

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "tr_gamerender.h"

idCVar r_skipRender("r_skipRender", "0", CVAR_RENDERER | CVAR_BOOL, "skip 3D rendering, but pass 2D");
idCVar r_skipRenderContext("r_skipRenderContext", "0", CVAR_RENDERER | CVAR_BOOL, "NULL the rendering context during backend 3D rendering");

const int	MAX_CLIP_PLANES = 1;				// we may expand this to six for some subview issues

// viewDefs are allocated on the frame temporary stack memory
typedef struct viewDef_s {
	// specified in the call to DrawScene()
	renderView_t		renderView;
	//byte				unknown[136];
	float				projectionMatrix[16];
} viewDef_t;

typedef struct {
	int		commandId, * next;
	viewDef_t* viewDef;
} drawSurfsCommand_t;

viewDef_t** backEnd_viewDef = (viewDef_t**)0x11124D74;
byte* backeEnd_unknown = (byte*)0x11124D78;
int* backEnd_depthFunc = (int*)0x11124E5C;

bool* backEnd_currentRenderCopied = (bool*)0x11124DF8;
int* backEnd_pc_c_surfaces = (int*)0x11124DFC;

void(*GLimp_DeactivateContext)(void) = (void(__cdecl*)(void))0x1017C9B0;
void(*RB_ShowOverdraw)(void) = (void(__cdecl*)(void))0x1012D380;
void(*GLimp_ActivateContext)(void) = (void(__cdecl*)(void))0x1017C990;
void(*RB_SetDefaultGLState)(void) = (void(__cdecl*)(void))0x1011FBD0;

//void(*RB_BeginDrawingView)(void) = (void(__cdecl*)(void))0x1012B0F0;
void(*RB_DetermineLightScale)(void) = (void(__cdecl*)(void))0x1012BFB0;
void(*RB_STD_FillDepthBuffer)(drawSurf_t** drawSurfs, int numDrawSurfs) = (void(__cdecl*)(drawSurf_t**, int))0x100A7B70;
void(*RB_ARB2_DrawInteractions)(void) = (void(__cdecl*)(void))0x100A5D50;
void(*RB_DrawSurfacesWithFlags)(drawSurf_t** drawSurfs, int numDrawSurfs, int unknown, int unknown2) = (void(__cdecl*)(drawSurf_t**, int, int, int))0x100A9D10;
void(*RB_STD_FogAllLights)(void) = (void(__cdecl*)(void))0x100A8DE0;
void(*RB_STD_DrawShaderPasses)(drawSurf_t** drawSurfs, int numDrawSurfs) = (void(__cdecl*)(drawSurf_t**, int))0x10131570;
void(*RB_Unknown)(int unknown1, int unknown2) = (void(__cdecl*)(int, int))0x10112AA0;
int(*R_FindARBProgram)(GLenum target, const char* program) = (int(__cdecl*)(GLenum, const char*))0x100A6CE0;
void(*GL_SelectTexture)(int unit) = (void(__cdecl*)(int))0x1011EDA0;

void VR_PreSwap(GLuint left, GLuint right);
void VR_UpdateVRInfo(void);

int currentEye = -1;

ID_INLINE void	GL_ViewportAndScissor(int x, int y, int w, int h)
{
	glViewport(x, y, w, h);
	glScissor(x, y, w, h);
}

idMat4 GetHMDMatrixPoseEye(vr::Hmd_Eye nEye) {
	vr::HmdMatrix34_t matEyeRight = hmd->GetEyeToHeadTransform(nEye);

	return idMat4(matEyeRight.m[0][0], matEyeRight.m[1][0], matEyeRight.m[2][0], 0.0,
		matEyeRight.m[0][1], matEyeRight.m[1][1], matEyeRight.m[2][1], 0.0,
		matEyeRight.m[0][2], matEyeRight.m[1][2], matEyeRight.m[2][2], 0.0,
		matEyeRight.m[0][3], matEyeRight.m[1][3], matEyeRight.m[2][3], 1.0f);
}

/*
======================
R_SetupProjectionMatrix

This uses the "infinite far z" trick
======================
*/
idCVar r_centerX("r_centerX", "0", CVAR_FLOAT, "projection matrix center adjust");
idCVar r_centerY("r_centerY", "0", CVAR_FLOAT, "projection matrix center adjust");

void R_SetStereoProjectionMatrix(void)
{
	extern int currentEye;
	if (currentEye == -1)
		return;

	int targetEye = currentEye;

	// random jittering is usefull when multiple
	// frames are going to be blended together
	// for motion blurred anti-aliasing
	float jitterx, jittery;
	jitterx = 0.0f;
	jittery = 0.0f;

	float fov_left = vrConfig.openVRfovEye[targetEye][0];
	float fov_right = vrConfig.openVRfovEye[targetEye][1];
	float fov_bottom = vrConfig.openVRfovEye[targetEye][2];
	float fov_top = vrConfig.openVRfovEye[targetEye][3];
	float stereoScreenSeparation = 0.f;

	//
	// set up projection matrix
	//
	const float zNear = 0.5f;

	float ymax = fov_top;
	float ymin = fov_bottom;

	float xmax = fov_right;
	float xmin = fov_left;

	const float width = xmax - xmin;
	const float height = ymax - ymin;

	const int viewWidth = vrConfig.openVRWidth / 2;
	const int viewHeight = vrConfig.openVRHeight;

	jitterx = jitterx * width / viewWidth;
	jitterx += r_centerX.GetFloat();
	jitterx += stereoScreenSeparation;
	xmin += jitterx * width;
	xmax += jitterx * width;

	jittery = jittery * height / viewHeight;
	jittery += r_centerY.GetFloat();
	ymin += jittery * height;
	ymax += jittery * height;

	float* projectionMatrix = &(*backEnd_viewDef)->projectionMatrix[0];

	vr::HmdMatrix44_t mat;
	if(currentEye == 0) {
		mat = hmd->GetProjectionMatrix(vr::Eye_Left, 0.5, 2000, vr::EGraphicsAPIConvention::API_OpenGL);
	}
	else {
		mat = hmd->GetProjectionMatrix(vr::Eye_Right, 0.5, 2000, vr::EGraphicsAPIConvention::API_OpenGL);
	}
	
	memcpy(projectionMatrix, &mat, sizeof(float) * 16);

	//projectionMatrix[0 * 4 + 0] = 2.0f / width;
	//projectionMatrix[1 * 4 + 0] = 0.0f;
	//projectionMatrix[2 * 4 + 0] = (xmax + xmin) / width;	// normally 0
	//projectionMatrix[3 * 4 + 0] = 0.0f;
	//
	//projectionMatrix[0 * 4 + 1] = 0.0f;
	//projectionMatrix[1 * 4 + 1] = 2.0f / height;
	//projectionMatrix[2 * 4 + 1] = (ymax + ymin) / height;	// normally 0
	//projectionMatrix[3 * 4 + 1] = 0.0f;
	//
	//// this is the far-plane-at-infinity formulation, and
	//// crunches the Z range slightly so w=0 vertexes do not
	//// rasterize right at the wraparound point
	projectionMatrix[0 * 4 + 2] = 0.0f;
	projectionMatrix[1 * 4 + 2] = 0.0f;
	//projectionMatrix[2 * 4 + 2] = -0.999f; // adjust value to prevent imprecision issues
	//projectionMatrix[3 * 4 + 2] = -2.0f * zNear;
	//
	//projectionMatrix[0 * 4 + 3] = 0.0f;
	//projectionMatrix[1 * 4 + 3] = 0.0f;
	projectionMatrix[2 * 4 + 3] = -1.0f;
	//projectionMatrix[3 * 4 + 3] = 0.0f;
}

void RB_BeginDrawingView(void) {
	

	// set the modelview matrix for the viewer
	glMatrixMode(GL_PROJECTION);
	

	glLoadMatrixf(&(*backEnd_viewDef)->projectionMatrix[0]);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void R_UpdateVR(void) {
	VR_PreSwap(forwardRender[0]->GetColorImage(0)->texnum, forwardRender[1]->GetColorImage(0)->texnum);
	VR_UpdateVRInfo();
}

/*
=============
RB_STD_DrawView

=============
*/
void RB_STD_DrawView(void) {
	float oldProjectionMatrix[16];
	memcpy(oldProjectionMatrix, &(*backEnd_viewDef)->projectionMatrix[0], sizeof(float) * 16);	

	if((*backEnd_viewDef)->renderView.shaderParms[11] == 1000.0f) {
		currentEye = 1;
	}
	else if ((*backEnd_viewDef)->renderView.shaderParms[11] == 1001.0f) {
		currentEye = 0;
	}
	else {
		currentEye = -1;
	}

	if(currentEye >= 0)
		forwardRender[currentEye]->MakeCurrent();

	RB_BeginDrawingViewEngine(); // Make sure stuff is setup and bound.
	R_SetStereoProjectionMatrix();
	GL_ViewportAndScissor(0, 0, vrConfig.openVRWidth, vrConfig.openVRHeight);
	RB_STD_DrawViewEngine();

	idRenderTexture::BindNull();

	memcpy(&(*backEnd_viewDef)->projectionMatrix[0], oldProjectionMatrix, sizeof(float) * 16);

	GL_ViewportAndScissor(0, 0, vrConfig.openVRWidth, vrConfig.openVRHeight);
}
