// vr.cpp
//

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"

idCVar vr_resolutionScale("vr_resolutionScale", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "hmd resolution scaling, restart required");
idCVar vr_playerHeightCM("vr_playerHeightCM", "171", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "player height for vr in centimeters");
idCVar vr_aimLook("vr_aimLook", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "aim where you look");
idCVar vr_seated("vr_seated", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "seated mode");
idCVar vr_forceGamepad("vr_forceGamepad", "0", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "force using the gamepad to control weapons");
idCVar vr_knockbackScale("vr_knockbackScale", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "how much knockback affects you");
idCVar vr_strafing("vr_strafing", "1", CVAR_ARCHIVE | CVAR_BOOL, "enable/disable left control strafing");
idCVar vr_forwardOnly("vr_forwardOnly", "0", CVAR_ARCHIVE | CVAR_BOOL, "left touchpad only moves forward");
idCVar vr_relativeAxis("vr_relativeAxis", "0", CVAR_ARCHIVE | CVAR_BOOL, "movement relative to initial touch");
idCVar vr_maxRadius("vr_maxRadius", "0.9", CVAR_ARCHIVE | CVAR_FLOAT, "smaller values make it easier to hit max movement speed");
idCVar vr_responseCurve("vr_responseCurve", "0", CVAR_ARCHIVE | CVAR_FLOAT, "interpoloate between linear and square curves, -1 for inverse square");
idCVar vr_moveMode("vr_moveMode", "8", CVAR_ARCHIVE | CVAR_INTEGER, "	0 touch walk | 1 touch walk & hold run | 2 touch walk & click run | 3 click walk | 4 click walk & hold run | 5 click walk & double click run | 6 hold walk");
idCVar vr_moveSpeed("vr_moveSpeed", "0.5", CVAR_ARCHIVE | CVAR_FLOAT, "Touchpad player movement speed is multiplied by this value. Set to 1 for normal speed, or between 0 and 1 for slower movement.");

VRConfig_t vrConfig;

vr::IVRSystem* hmd;
float g_vrScaleX = 1.f;
float g_vrScaleY = 1.f;
float g_vrScaleZ = 1.f;
vr::TrackedDeviceIndex_t g_openVRLeftController = vr::k_unTrackedDeviceIndexInvalid;
vr::TrackedDeviceIndex_t g_openVRRightController = vr::k_unTrackedDeviceIndexInvalid;
idVec3 g_SeatedOrigin;
idMat3 g_SeatedAxis;
idMat3 g_SeatedAxisInverse;

void VR_Init(void)
{
	common->Printf("---------- Initializing VR --------\n");

	vr::EVRInitError error = vr::VRInitError_None;
	hmd = vr::VR_Init(&error, vr::VRApplication_Scene);
	if (error != vr::VRInitError_None)
	{
		common->FatalError("VR initialization failed: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(error));
		return;
	}

	if (!vr::VRCompositor())
	{
		common->FatalError("VR compositor not present.\n");
		return;
	}

	//vr::VRCompositor()->ForceInterleavedReprojectionOn( true );

	VR_UpdateResolution();

	hmd->GetProjectionRaw(vr::Eye_Left,
		&vrConfig.openVRfovEye[1][0], &vrConfig.openVRfovEye[1][1],
		&vrConfig.openVRfovEye[1][2], &vrConfig.openVRfovEye[1][3]);

	hmd->GetProjectionRaw(vr::Eye_Right,
		&vrConfig.openVRfovEye[0][0], &vrConfig.openVRfovEye[0][1],
		&vrConfig.openVRfovEye[0][2], &vrConfig.openVRfovEye[0][3]);

	vrConfig.openVRScreenSeparation =
		0.5f * (vrConfig.openVRfovEye[1][1] + vrConfig.openVRfovEye[1][0])
		/ (vrConfig.openVRfovEye[1][1] - vrConfig.openVRfovEye[1][0])
		- 0.5f * (vrConfig.openVRfovEye[0][1] + vrConfig.openVRfovEye[0][0])
		/ (vrConfig.openVRfovEye[0][1] - vrConfig.openVRfovEye[0][0]);

	vr::HmdMatrix34_t mat;

#if 0
	mat = hmd->GetEyeToHeadTransform(vr::Eye_Left);
	Convert4x3Matrix(&mat, hmdEyeLeft);
	MatrixRTInverse(hmdEyeLeft);
#endif

	mat = hmd->GetEyeToHeadTransform(vr::Eye_Right);
#if 0
	Convert4x3Matrix(&mat, hmdEyeRight);
	MatrixRTInverse(hmdEyeRight);
#endif

	vrConfig.openVRUnscaledHalfIPD = mat.m[0][3];
	vrConfig.openVRUnscaledEyeForward = -mat.m[2][3];
	VR_UpdateScaling();

	vrConfig.openVRSeated = true;
	vrConfig.openVREnabled = true;
	g_openVRLeftController = vr::k_unTrackedDeviceIndexInvalid;
	g_openVRRightController = vr::k_unTrackedDeviceIndexInvalid;
	VR_UpdateControllers();

	vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseStanding);
	VR_ConvertMatrix(hmd->GetSeatedZeroPoseToStandingAbsoluteTrackingPose(), g_SeatedOrigin, g_SeatedAxis);
	g_SeatedAxisInverse = g_SeatedAxis.Inverse();
}
