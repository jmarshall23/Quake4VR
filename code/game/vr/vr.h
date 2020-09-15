// vr.h
//

#pragma once

#define INT_MIN     (-2147483647 - 1)
#define INT_MAX       2147483647
#include "openvr/headers/openvr.h"


extern vr::IVRSystem* hmd;
extern float g_vrScaleX;
extern float g_vrScaleY;
extern float g_vrScaleZ;
extern vr::TrackedDeviceIndex_t g_openVRLeftController;
extern vr::TrackedDeviceIndex_t g_openVRRightController;
extern idVec3 g_SeatedOrigin;
extern idMat3 g_SeatedAxis;
extern idMat3 g_SeatedAxisInverse;

void VR_Init(void);

void VR_ConvertMatrix(const vr::HmdMatrix34_t& poseMat, idVec3& origin, idMat3& axis);

const sysEvent_t& VR_UIEventNext();

void VR_ResetPose();
void VR_LogDevices();

void VR_UpdateResolution();
void VR_UpdateScaling();
void VR_UpdateControllers();

int VR_PollGameInputEvents();
int VR_ReturnGameInputEvent(const int n, int& action, int& value);

void VR_PreSwap(GLuint left, GLuint right);
void VR_UpdateVRInfo(void);

bool VR_GetHead(idVec3& origin, idMat3& axis);
bool VR_GetLeftController(idVec3& origin, idMat3& axis);
bool VR_GetRightController(idVec3& origin, idMat3& axis);
void VR_MoveDelta(idVec3& delta, float& height);
void VR_HapticPulse(int leftDuration, int rightDuration);
bool VR_GetLeftControllerAxis(idVec2& axis);
bool VR_GetRightControllerAxis(idVec2& axis);
bool VR_LeftControllerWasPressed();
bool VR_LeftControllerIsPressed();
bool VR_RightControllerWasPressed();
bool VR_RightControllerIsPressed();
bool VR_RightControllerTriggerIsPressed();
bool VR_CalculateView(idVec3& origin, idMat3& axis, const idVec3& eyeOffset, bool overridePitch);

const idVec3& VR_GetSeatedOrigin();
const idMat3& VR_GetSeatedAxis();
const idMat3& VR_GetSeatedAxisInverse();

struct VRConfig_t {
	bool				openVREnabled;
	bool				openVRSeated;
	bool				openVRLeftTouchpad;
	bool				openVRRightTouchpad;
	int					openVRWidth;
	int					openVRHeight;
	float				openVRfovEye[2][4];
	float				openVRScreenSeparation;
	float				openVRScale;
	float				openVRUnscaledEyeForward;
	float				openVRUnscaledHalfIPD;
	float				openVREyeForward;
	float				openVRHalfIPD;
};

extern VRConfig_t vrConfig;


extern idCVar vr_resolutionScale;
extern idCVar vr_playerHeightCM;
extern idCVar vr_aimLook;
extern idCVar vr_seated;
extern idCVar vr_forceGamepad;
extern idCVar vr_knockbackScale;
extern idCVar vr_strafing;
extern idCVar vr_forwardOnly;
extern idCVar vr_relativeAxis;
extern idCVar vr_responseCurve;
extern idCVar vr_moveMode;
extern idCVar vr_moveSpeed;

#undef INT_MIN
#undef INT_MAX