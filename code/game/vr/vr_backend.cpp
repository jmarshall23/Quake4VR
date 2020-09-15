// vr_backend.cpp
//

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"

#include "openvr/headers/openvr.h"

bool g_vrLeftControllerWasPressed;
bool g_vrRightControllerWasPressed;
vr::VRControllerState_t g_vrLeftControllerState;
vr::VRControllerState_t g_vrRightControllerState;
int g_openVRLeftControllerPulseDur;
int g_openVRRightControllerPulseDur;

bool g_vrHasHeadPose;
idVec3 g_vrHeadOrigin;
idMat3 g_vrHeadAxis;
bool g_vrHadHead;
idVec3 g_vrHeadLastOrigin;
idVec3 g_vrHeadMoveDelta;

bool g_vrHasLeftControllerPose;
idVec3 g_vrLeftControllerOrigin;
idMat3 g_vrLeftControllerAxis;

bool g_vrHasRightControllerPose;
idVec3 g_vrRightControllerOrigin;
idMat3 g_vrRightControllerAxis;

bool g_poseReset;

void VR_ResetPose()
{
	g_poseReset = true;
	hmd->ResetSeatedZeroPose();
}

void VR_LogDevices()
{
	char modelNumberString[vr::k_unTrackingStringSize];
	int axisType;
	const char* axisTypeString;

	hmd->GetStringTrackedDeviceProperty(
		vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String,
		modelNumberString, vr::k_unTrackingStringSize);
	common->Printf("\nhead  model \"%s\"\n", modelNumberString);

	if (g_openVRLeftController != vr::k_unTrackedDeviceIndexInvalid)
	{
		hmd->GetStringTrackedDeviceProperty(
			g_openVRLeftController, vr::Prop_ModelNumber_String,
			modelNumberString, vr::k_unTrackingStringSize);
		axisType = hmd->GetInt32TrackedDeviceProperty(g_openVRLeftController, vr::Prop_Axis0Type_Int32);
		axisTypeString = hmd->GetControllerAxisTypeNameFromEnum((vr::EVRControllerAxisType)axisType);
		common->Printf("left  model \"%s\" axis %s\n", modelNumberString, axisTypeString);
	}
	else
	{
		common->Printf("left  not detected\n");
	}

	if (g_openVRRightController != vr::k_unTrackedDeviceIndexInvalid)
	{
		hmd->GetStringTrackedDeviceProperty(
			g_openVRRightController, vr::Prop_ModelNumber_String,
			modelNumberString, vr::k_unTrackingStringSize);
		axisType = hmd->GetInt32TrackedDeviceProperty(g_openVRRightController, vr::Prop_Axis0Type_Int32);
		axisTypeString = hmd->GetControllerAxisTypeNameFromEnum((vr::EVRControllerAxisType)axisType);
		common->Printf("right model \"%s\" axis %s\n", modelNumberString, axisTypeString);
	}
	else
	{
		common->Printf("right not detected\n");
	}
}

#define MAX_VREVENTS 256

int g_vrUIEventIndex;
int g_vrUIEventCount;
sysEvent_t g_vrUIEvents[MAX_VREVENTS];

int g_vrGameEventCount;
struct {
	int action;
	int value;
} g_vrGameEvents[MAX_VREVENTS];

void VR_ClearEvents()
{
	g_vrUIEventIndex = 0;
	g_vrUIEventCount = 0;
	g_vrGameEventCount = 0;
	g_vrLeftControllerWasPressed = false;
	g_vrRightControllerWasPressed = false;
}

void VR_UIEventQue(sysEventType_t type, int value, int value2)
{
	assert(g_vrUIEventCount < MAX_VREVENTS);
	sysEvent_t* ev = &g_vrUIEvents[g_vrUIEventCount++];

	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = 0;
	ev->evPtr = NULL;
	//ev->inputDevice = 0;
}

const sysEvent_t& VR_UIEventNext()
{
	assert(g_vrUIEventIndex < MAX_VREVENTS);
	if (g_vrUIEventIndex >= g_vrUIEventCount)
	{
		sysEvent_t& ev = g_vrUIEvents[g_vrUIEventIndex];
		ev.evType = SE_NONE;
		return ev;
	}
	return g_vrUIEvents[g_vrUIEventIndex++];
}

int VR_PollGameInputEvents()
{
	return g_vrGameEventCount;
}

void VR_GameEventQue(int action, int value)
{
	assert(g_vrGameEventCount < MAX_VREVENTS);
	g_vrGameEvents[g_vrGameEventCount].action = action;
	g_vrGameEvents[g_vrGameEventCount].value = value;
	g_vrGameEventCount++;
}

int VR_ReturnGameInputEvent(const int n, int& action, int& value)
{
	if (n < 0 || n > g_vrGameEventCount)
	{
		return 0;
	}
	action = g_vrGameEvents[n].action;
	value = g_vrGameEvents[n].value;
	return 1;
}

idCVar vr_leftAxis("vr_leftAxis", "0", CVAR_INTEGER | CVAR_ARCHIVE, "left axis mode");
idCVar vr_rightAxis("vr_rightAxis", "4", CVAR_INTEGER | CVAR_ARCHIVE, "right axis mode");

static int VR_AxisToDPad(int mode, float x, float y)
{
	int dir;
	switch (mode)
	{
	case 3:
		if (y >= 0)
		{
			dir = 1; // up
		}
		else
		{
			dir = 3; // down
		}
		break;
	case 4:
		if (x <= 0)
		{
			dir = 0; // left
		}
		else
		{
			dir = 2; // right
		}
		break;
	case 5:
		if (x < y)
		{
			if (x > -y)
			{
				dir = 1; // up
			}
			else
			{
				dir = 0; // left
			}
		}
		else
		{
			if (x > -y)
			{
				dir = 2; // right
			}
			else
			{
				dir = 3; // down
			}
		}
		break;
	default:
		dir = -1;
	}
	return dir;
}

static void VR_GenButtonEvent(uint32_t button, bool left, bool pressed)
{
	switch (button)
	{
	case vr::k_EButton_ApplicationMenu:
		if (left)
		{
			VR_GameEventQue(K_VR_LEFT_MENU, pressed);
			VR_UIEventQue(SE_KEY, K_JOY10, pressed); // pda
		}
		else
		{
			VR_GameEventQue(K_VR_RIGHT_MENU, pressed);
			VR_UIEventQue(SE_KEY, K_JOY9, pressed); // pause menu
		}
		break;
	case vr::k_EButton_Grip:
		if (left)
		{
			VR_GameEventQue(K_VR_LEFT_GRIP, pressed);
			VR_UIEventQue(SE_KEY, K_JOY5, pressed); // prev pda menu
		}
		else
		{
			VR_GameEventQue(K_VR_RIGHT_GRIP, pressed);
			VR_UIEventQue(SE_KEY, K_JOY6, pressed); // next pda menu
		}
		break;
	case vr::k_EButton_SteamVR_Trigger:
		if (left)
		{
			VR_GameEventQue(K_VR_LEFT_TRIGGER, pressed);
			VR_UIEventQue(SE_KEY, K_JOY2, pressed); // menu back
		}
		else
		{
			VR_GameEventQue(K_VR_RIGHT_TRIGGER, pressed);
			VR_UIEventQue(SE_KEY, K_MOUSE1, pressed); // cursor click
		}
		break;
	case vr::k_EButton_SteamVR_Touchpad:
		if (left)
		{
			//VR_UIEventQue( SE_KEY, K_JOY2, pressed ); // menu back
			static keyNum_t uiLastKey;
			if (pressed)
			{
				if (g_vrLeftControllerState.rAxis[0].x < g_vrLeftControllerState.rAxis[0].y)
				{
					if (g_vrLeftControllerState.rAxis[0].x > -g_vrLeftControllerState.rAxis[0].y)
					{
						uiLastKey = K_JOY_STICK1_UP;
					}
					else
					{
						uiLastKey = K_JOY_STICK1_LEFT;
					}
				}
				else
				{
					if (g_vrLeftControllerState.rAxis[0].x > -g_vrLeftControllerState.rAxis[0].y)
					{
						uiLastKey = K_JOY_STICK1_RIGHT;
					}
					else
					{
						uiLastKey = K_JOY_STICK1_DOWN;
					}
				}
				VR_UIEventQue(SE_KEY, uiLastKey, 1);
			}
			else
			{
				VR_UIEventQue(SE_KEY, uiLastKey, 0);
			}

			VR_GameEventQue(K_VR_LEFT_AXIS, pressed);
			if (pressed)
			{
				g_vrLeftControllerWasPressed = true;
			}
			if (!vrConfig.openVRLeftTouchpad)
			{
				break;
			}
			// dpad modes
			static int gameLeftLastKey;
			if (pressed)
			{
				int dir = VR_AxisToDPad(vr_leftAxis.GetInteger(), g_vrLeftControllerState.rAxis[0].x, g_vrLeftControllerState.rAxis[0].y);
				if (dir != -1)
				{
					gameLeftLastKey = K_VR_LEFT_DPAD_LEFT + dir;
					VR_GameEventQue(gameLeftLastKey, 1);
				}
				else
				{
					gameLeftLastKey = K_NONE;
				}
			}
			else if (gameLeftLastKey != K_NONE)
			{
				VR_GameEventQue(gameLeftLastKey, 0);
				gameLeftLastKey = K_NONE;
			}
		}
		else
		{
			VR_UIEventQue(SE_KEY, K_JOY1, pressed); // menu select
			VR_GameEventQue(K_VR_RIGHT_AXIS, pressed);
			if (pressed)
			{
				g_vrRightControllerWasPressed = true;
			}
			if (!vrConfig.openVRRightTouchpad)
			{
				break;
			}
			// dpad modes
			static int gameRightLastKey;
			if (pressed)
			{
				int dir = VR_AxisToDPad(vr_rightAxis.GetInteger(), g_vrRightControllerState.rAxis[0].x, g_vrRightControllerState.rAxis[0].y);
				if (dir != -1)
				{
					gameRightLastKey = K_VR_RIGHT_DPAD_LEFT + dir;
					VR_GameEventQue(gameRightLastKey, 1);
				}
				else
				{
					gameRightLastKey = K_NONE;
				}
			}
			else if (gameRightLastKey != K_NONE)
			{
				VR_GameEventQue(gameRightLastKey, 0);
				gameRightLastKey = K_NONE;
			}
		}
		break;
	case vr::k_EButton_A:
		if (left)
		{
			VR_GameEventQue(K_VR_LEFT_A, pressed);
		}
		else
		{
			VR_GameEventQue(K_VR_RIGHT_A, pressed);
		}
		break;
	default:
		break;
	}
}

static void VR_GenJoyAxisEvents()
{
	if (g_openVRLeftController != vr::k_unTrackedDeviceIndexInvalid)
	{
		vr::VRControllerState_t& state = g_vrLeftControllerState;
		hmd->GetControllerState(g_openVRLeftController, &state);

		// dpad modes
		if (!vrConfig.openVRLeftTouchpad)
		{
			static int gameLeftLastKey;
			if (state.rAxis[0].x * state.rAxis[0].x + state.rAxis[0].y * state.rAxis[0].y > 0.25f)
			{
				int dir = VR_AxisToDPad(vr_leftAxis.GetInteger(), g_vrLeftControllerState.rAxis[0].x, g_vrLeftControllerState.rAxis[0].y);
				if (dir != -1)
				{
					gameLeftLastKey = K_VR_LEFT_DPAD_LEFT + dir;
					VR_GameEventQue(gameLeftLastKey, 1);
				}
				else
				{
					gameLeftLastKey = K_NONE;
				}
			}
			else if (gameLeftLastKey != K_NONE)
			{
				VR_GameEventQue(gameLeftLastKey, 0);
				gameLeftLastKey = K_NONE;
			}
		}
	}
	if (g_openVRRightController != vr::k_unTrackedDeviceIndexInvalid)
	{
		vr::VRControllerState_t& state = g_vrRightControllerState;
		hmd->GetControllerState(g_openVRRightController, &state);

		// dpad modes
		if (!vrConfig.openVRRightTouchpad)
		{
			static int gameRightLastKey;
			if (state.rAxis[0].x * state.rAxis[0].x + state.rAxis[0].y * state.rAxis[0].y > 0.25f)
			{
				int dir = VR_AxisToDPad(vr_rightAxis.GetInteger(), g_vrRightControllerState.rAxis[0].x, g_vrRightControllerState.rAxis[0].y);
				if (dir != -1)
				{
					gameRightLastKey = K_VR_RIGHT_DPAD_LEFT + dir;
					VR_GameEventQue(gameRightLastKey, 1);
				}
				else
				{
					gameRightLastKey = K_NONE;
				}
			}
			else if (gameRightLastKey != K_NONE)
			{
				VR_GameEventQue(gameRightLastKey, 0);
				gameRightLastKey = K_NONE;
			}
		}
	}
}

static void VR_GenMouseEvents()
{
// jmarshall - fix me
	// virtual head tracking mouse for shell UI
	//idVec3 shellOrigin;
	//idMat3 shellAxis;
	//if (g_vrHadHead && tr.guiModel->GetVRShell(shellOrigin, shellAxis))
	//{
	//	const float virtualWidth = renderSystem->GetVirtualWidth();
	//	const float virtualHeight = renderSystem->GetVirtualHeight();
	//	float guiHeight = 12 * 5.3f;
	//	float guiScale = guiHeight / virtualHeight;
	//	float guiWidth = virtualWidth * guiScale;
	//	float guiForward = guiHeight + 12.f;
	//	idVec3 upperLeft = shellOrigin
	//		+ shellAxis[0] * guiForward
	//		+ shellAxis[1] * 0.5f * guiWidth
	//		+ shellAxis[2] * 0.5f * guiHeight;
	//	idMat3 invShellAxis = shellAxis.Inverse();
	//	idVec3 rayStart = (g_vrHeadOrigin - upperLeft) * invShellAxis;
	//	idVec3 rayDir = g_vrHeadAxis[0] * invShellAxis;
	//	if (rayDir.x != 0)
	//	{
	//		static int oldX, oldY;
	//		float wx = rayStart.y - rayStart.x * rayDir.y / rayDir.x;
	//		float wy = rayStart.z - rayStart.x * rayDir.z / rayDir.x;
	//		int x = -wx * vrConfig.nativeScreenWidth / guiWidth;
	//		int y = -wy * vrConfig.nativeScreenHeight / guiHeight;
	//		if (x >= 0 && x < vrConfig.nativeScreenWidth &&
	//			y >= 0 && y < vrConfig.nativeScreenHeight &&
	//			(x != oldX || y != oldY))
	//		{
	//			oldX = x;
	//			oldY = y;
	//			VR_UIEventQue(SE_MOUSE_ABSOLUTE, x, y);
	//		}
	//	}
	//}
// jmarshall end
}

void VR_ConvertMatrix(const vr::HmdMatrix34_t& poseMat, idVec3& origin, idMat3& axis)
{
	origin.Set(
		g_vrScaleX * poseMat.m[2][3],
		g_vrScaleY * poseMat.m[0][3],
		g_vrScaleZ * poseMat.m[1][3]);
	axis[0].Set(poseMat.m[2][2], poseMat.m[0][2], -poseMat.m[1][2]);
	axis[1].Set(poseMat.m[2][0], poseMat.m[0][0], -poseMat.m[1][0]);
	axis[2].Set(-poseMat.m[2][1], -poseMat.m[0][1], poseMat.m[1][1]);
}

bool VR_ConvertPose(const vr::TrackedDevicePose_t& pose, idVec3& origin, idMat3& axis)
{
	if (!pose.bPoseIsValid)
	{
		return false;
	}

	VR_ConvertMatrix(pose.mDeviceToAbsoluteTracking, origin, axis);

	return true;
}

void VR_UpdateResolution(void)
{
	vr_resolutionScale.ClearModified();
	float scale = vr_resolutionScale.GetFloat();
	uint32_t width, height;
	hmd->GetRecommendedRenderTargetSize(&width, &height);
	width = width * scale;
	height = height * scale;
	if (width < 540) width = 640;
	else if (width > 8000) width = 8000;
	if (height < 540) height = 480;
	else if (height > 8000) height = 8000;
	vrConfig.openVRWidth = width;
	vrConfig.openVRHeight = height;
}

void VR_UpdateScaling()
{
	const float m2i = 1 / 0.0254f; // meters to inches
	const float cm2i = 1 / 2.54f; // centimeters to inches
	float ratio = 76.5f / (vr_playerHeightCM.GetFloat() * cm2i); // converts player height to character height
	vrConfig.openVRScale = m2i * ratio;
	vrConfig.openVRHalfIPD = vrConfig.openVRUnscaledHalfIPD * vrConfig.openVRScale;
	vrConfig.openVREyeForward = vrConfig.openVRUnscaledEyeForward * vrConfig.openVRScale;
	g_vrScaleX = -vrConfig.openVRScale;
	g_vrScaleY = -vrConfig.openVRScale;
	g_vrScaleZ = vrConfig.openVRScale;
}

void VR_UpdateControllers()
{
	bool hadLeft = g_openVRLeftController != vr::k_unTrackedDeviceIndexInvalid;
	bool hadRight = g_openVRRightController != vr::k_unTrackedDeviceIndexInvalid;

	g_openVRLeftController = hmd->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
	g_openVRRightController = hmd->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);

	if (hadLeft && g_openVRLeftController == vr::k_unTrackedDeviceIndexInvalid)
	{
		common->Printf("left controller lost\n");
	}
	if (hadRight && g_openVRRightController == vr::k_unTrackedDeviceIndexInvalid)
	{
		common->Printf("right controller lost\n");
	}

	if (g_openVRLeftController != vr::k_unTrackedDeviceIndexInvalid
		|| g_openVRRightController != vr::k_unTrackedDeviceIndexInvalid)
	{
		if (vrConfig.openVRSeated)
		{
			vrConfig.openVRSeated = false;

			char modelNumberString[vr::k_unTrackingStringSize];
			int axisType;

			hmd->GetStringTrackedDeviceProperty(
				g_openVRLeftController, vr::Prop_ModelNumber_String,
				modelNumberString, vr::k_unTrackingStringSize);
			if (strcmp(modelNumberString, "Hydra") == 0)
			{
				vrConfig.openVRLeftTouchpad = 0;
			}
			else
			{
				axisType = hmd->GetInt32TrackedDeviceProperty(g_openVRLeftController, vr::Prop_Axis0Type_Int32);
				vrConfig.openVRLeftTouchpad = (axisType == vr::k_eControllerAxis_TrackPad) ? 1 : 0;
			}

			hmd->GetStringTrackedDeviceProperty(
				g_openVRRightController, vr::Prop_ModelNumber_String,
				modelNumberString, vr::k_unTrackingStringSize);
			if (strcmp(modelNumberString, "Hydra") == 0)
			{
				vrConfig.openVRRightTouchpad = 0;
			}
			else
			{
				axisType = hmd->GetInt32TrackedDeviceProperty(g_openVRRightController, vr::Prop_Axis0Type_Int32);
				vrConfig.openVRRightTouchpad = (axisType == vr::k_eControllerAxis_TrackPad) ? 1 : 0;
			}
		}
	}
	else
	{
		if (!vrConfig.openVRSeated)
		{
			vrConfig.openVRSeated = true;
		}
	}
}

void VR_PreSwap(GLuint left, GLuint right)
{
	//GL_ViewportAndScissor(0, 0, vrConfig.openVRWidth, vrConfig.openVRHeight); // jmarshall
	vr::Texture_t leftEyeTexture = { (void*)left, vr::API_OpenGL, vr::ColorSpace_Gamma };
	vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);
	vr::Texture_t rightEyeTexture = { (void*)right, vr::API_OpenGL, vr::ColorSpace_Gamma };
	vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);
}

void VR_UpdateVRInfo(void)
{
	//vr::VRCompositor()->PostPresentHandoff();

	vr::TrackedDevicePose_t rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
	vr::VRCompositor()->WaitGetPoses(rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

	VR_UpdateControllers();

	if (vr_playerHeightCM.IsModified())
	{
		vr_playerHeightCM.ClearModified();
		VR_UpdateScaling();
	}

	if (vr_seated.IsModified())
	{
		vr_seated.ClearModified();
		//tr.guiModel->UpdateVRShell(); // jmarshall
	}

	vr::TrackedDevicePose_t& hmdPose = rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd];
	g_vrHasHeadPose = hmdPose.bPoseIsValid;
	if (hmdPose.bPoseIsValid)
	{
		VR_ConvertPose(hmdPose, g_vrHeadOrigin, g_vrHeadAxis);
		g_vrHeadOrigin += vrConfig.openVREyeForward * g_vrHeadAxis[0];

		if (g_vrHadHead)
		{
			g_vrHeadMoveDelta = g_vrHeadOrigin - g_vrHeadLastOrigin;
			g_vrHeadLastOrigin = g_vrHeadOrigin;
		}
		else
		{
			g_vrHadHead = true;
			g_vrHeadMoveDelta.Zero();
		}
	}
	else
	{
		g_vrHadHead = false;
	}

	if (vr_forceGamepad.GetBool())
	{
		g_vrHasLeftControllerPose = false;
		g_vrHasRightControllerPose = false;
	}
	else
	{
		if (g_openVRLeftController != vr::k_unTrackedDeviceIndexInvalid)
		{
			if (g_openVRLeftControllerPulseDur > 500)
			{
				hmd->TriggerHapticPulse(g_openVRLeftController, 0, g_openVRLeftControllerPulseDur);
			}
			g_openVRLeftControllerPulseDur = 0;

			static bool hadLeftPose;
			vr::TrackedDevicePose_t& handPose = rTrackedDevicePose[g_openVRLeftController];
			if (handPose.bPoseIsValid)
			{
				g_vrHasLeftControllerPose = true;
				VR_ConvertPose(handPose, g_vrLeftControllerOrigin, g_vrLeftControllerAxis);
				hadLeftPose = true;
			}
			else if (hadLeftPose)
			{
				hadLeftPose = false;
				common->Printf("left controller had no pose\n");
			}
		}

		if (g_openVRRightController != vr::k_unTrackedDeviceIndexInvalid)
		{
			if (g_openVRRightControllerPulseDur > 500)
			{
				hmd->TriggerHapticPulse(g_openVRRightController, 0, g_openVRRightControllerPulseDur);
			}
			g_openVRRightControllerPulseDur = 0;

			static bool hadRightPose;
			vr::TrackedDevicePose_t& handPose = rTrackedDevicePose[g_openVRRightController];
			if (handPose.bPoseIsValid)
			{
				g_vrHasRightControllerPose = true;
				VR_ConvertPose(handPose, g_vrRightControllerOrigin, g_vrRightControllerAxis);
				hadRightPose = true;
			}
			else if (hadRightPose)
			{
				hadRightPose = false;
				common->Printf("right controller had no pose\n");
			}
		}
	}

	VR_ClearEvents();

	VR_GenJoyAxisEvents();

	vr::VREvent_t e;
	while (hmd->PollNextEvent(&e, sizeof(e)))
	{
		//vr::ETrackedControllerRole role;

		switch (e.eventType)
		{
			/*case vr::VREvent_TrackedDeviceActivated:
				role = hmd->GetControllerRoleForTrackedDeviceIndex(e.trackedDeviceIndex);
				switch(role)
				{
				case vr::TrackedControllerRole_LeftHand:
					g_openVRLeftController = e.trackedDeviceIndex;
					break;
				case vr::TrackedControllerRole_RightHand:
					g_openVRRightController = e.trackedDeviceIndex;
					break;
				}
				break;
			case vr::VREvent_TrackedDeviceDeactivated:
				if (e.trackedDeviceIndex == g_openVRLeftController)
				{
					g_openVRLeftController = vr::k_unTrackedDeviceIndexInvalid;
				}
				else if (e.trackedDeviceIndex == g_openVRRightController)
				{
					g_openVRRightController = vr::k_unTrackedDeviceIndexInvalid;
				}
				break;*/
		case vr::VREvent_ButtonPress:
			if (e.trackedDeviceIndex == g_openVRLeftController || e.trackedDeviceIndex == g_openVRRightController)
			{
				VR_GenButtonEvent(e.data.controller.button, e.trackedDeviceIndex == g_openVRLeftController, true);
			}
			break;
		case vr::VREvent_ButtonUnpress:
			if (e.trackedDeviceIndex == g_openVRLeftController || e.trackedDeviceIndex == g_openVRRightController)
			{
				VR_GenButtonEvent(e.data.controller.button, e.trackedDeviceIndex == g_openVRLeftController, false);
			}
			break;
		}
	}

	if (!vrConfig.openVRSeated)
	{
		VR_GenMouseEvents();
	}

	if (g_poseReset)
	{
		g_poseReset = false;
		VR_ConvertMatrix(hmd->GetSeatedZeroPoseToStandingAbsoluteTrackingPose(), g_SeatedOrigin, g_SeatedAxis);
		g_SeatedAxisInverse = g_SeatedAxis.Inverse();
		//tr.guiModel->UpdateVRShell(); // jmarshall
	}
}

bool VR_CalculateView(idVec3& origin, idMat3& axis, const idVec3& eyeOffset, bool overridePitch)
{
	if (!g_vrHasHeadPose)
	{
		return false;
	}

	if (overridePitch)
	{
		float pitch = idMath::M_RAD2DEG * asin(axis[0][2]);
		idAngles angles(pitch, 0, 0);
		axis = angles.ToMat3() * axis;
	}

	if (!vr_seated.GetBool())
	{
		origin.z -= eyeOffset.z;
		// ignore x and y
		origin += axis[2] * g_vrHeadOrigin.z;
	}
	else
	{
		origin += axis * g_vrHeadOrigin;
	}

	axis = g_vrHeadAxis * axis;

	return true;
}

bool VR_GetHead(idVec3& origin, idMat3& axis)
{
	if (!g_vrHasHeadPose)
	{
		return false;
	}

	origin = g_vrHeadOrigin;
	axis = g_vrHeadAxis;

	return true;
}

// returns left controller position relative to the head
bool VR_GetLeftController(idVec3& origin, idMat3& axis)
{
	if (!g_vrHasLeftControllerPose || !g_vrHasHeadPose)
	{
		return false;
	}

	origin = g_vrLeftControllerOrigin;
	axis = g_vrLeftControllerAxis;

	return true;
}

// returns right controller position relative to the head
bool VR_GetRightController(idVec3& origin, idMat3& axis)
{
	if (!g_vrHasRightControllerPose || !g_vrHasHeadPose)
	{
		return false;
	}

	origin = g_vrRightControllerOrigin;
	axis = g_vrRightControllerAxis;

	return true;
}

void VR_MoveDelta(idVec3& delta, float& height)
{
	if (!g_vrHasHeadPose)
	{
		height = 0.f;
		delta.Set(0, 0, 0);
		return;
	}

	height = g_vrHeadOrigin.z;

	delta.x = g_vrHeadMoveDelta.x;
	delta.y = g_vrHeadMoveDelta.y;
	delta.z = 0.f;

	g_vrHeadMoveDelta.Zero();
}

void VR_HapticPulse(int leftDuration, int rightDuration)
{
	if (leftDuration > g_openVRLeftControllerPulseDur)
	{
		g_openVRLeftControllerPulseDur = leftDuration;
	}
	if (rightDuration > g_openVRRightControllerPulseDur)
	{
		g_openVRRightControllerPulseDur = rightDuration;
	}
}

bool VR_GetLeftControllerAxis(idVec2& axis)
{
	if (g_openVRLeftController == vr::k_unTrackedDeviceIndexInvalid)
	{
		return false;
	}
	uint64_t mask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad);
	if (!(g_vrLeftControllerState.ulButtonTouched & mask))
	{
		return false;
	}
	axis.x = g_vrLeftControllerState.rAxis[0].x;
	axis.y = g_vrLeftControllerState.rAxis[0].y;
	return true;
}

bool VR_GetRightControllerAxis(idVec2& axis)
{
	if (g_openVRRightController == vr::k_unTrackedDeviceIndexInvalid)
	{
		return false;
	}
	uint64_t mask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad);
	if (!(g_vrRightControllerState.ulButtonTouched & mask))
	{
		return false;
	}
	axis.x = g_vrRightControllerState.rAxis[0].x;
	axis.y = g_vrRightControllerState.rAxis[0].y;
	return true;
}

bool VR_LeftControllerWasPressed()
{
	return g_vrLeftControllerWasPressed;
}

bool VR_LeftControllerIsPressed()
{
	static uint64_t mask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad);
	return (g_vrLeftControllerState.ulButtonPressed & mask) != 0;
}

bool VR_RightControllerWasPressed()
{
	return g_vrRightControllerWasPressed;
}

bool VR_RightControllerIsPressed()
{
	static uint64_t mask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad);
	return (g_vrRightControllerState.ulButtonPressed & mask) != 0;
}

bool VR_RightControllerTriggerIsPressed() 
{
	static uint64_t mask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger);
	return (g_vrRightControllerState.ulButtonPressed & mask) != 0;
}

const idVec3& VR_GetSeatedOrigin()
{
	return g_SeatedOrigin;
}

const idMat3& VR_GetSeatedAxis()
{
	return g_SeatedAxis;
}

const idMat3& VR_GetSeatedAxisInverse()
{
	return g_SeatedAxisInverse;
}
