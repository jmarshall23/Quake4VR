/**
* Copyright (C) 2020 Elisha Riedlinger
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*/

#include "dinputto8.h"

BOOL CALLBACK m_IDirectInputEnumEffect::EnumEffectCallback(LPDIRECTINPUTEFFECT pdeff, LPVOID pvRef)
{
	ENUMEFFECT *lpCallbackContext = (ENUMEFFECT*)pvRef;

	if (pdeff)
	{
		pdeff = ProxyAddressLookupTable.FindAddress<m_IDirectInputEffect>(pdeff);
	}

	return lpCallbackContext->lpCallback(pdeff, lpCallbackContext->pvRef);
}

BOOL CALLBACK m_IDirectInputEnumDevice::EnumDeviceCallbackA(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef)
{
	ENUMDEVICE *lpCallbackContext = (ENUMDEVICE*)pvRef;

	DIDEVICEINSTANCEA DI;
	CopyMemory(&DI, lpddi, lpddi->dwSize);

	DI.dwDevType = (lpddi->dwDevType & ~0xFFFF) |													// Remove device type and sub type
		ConvertDevSubTypeTo7(lpddi->dwDevType & 0xFF, (lpddi->dwDevType & 0xFF00) >> 8) << 8 |		// Add converted sub type
		ConvertDevTypeTo7(lpddi->dwDevType & 0xFF);													// Add converted device type

	return ((LPDIENUMDEVICESCALLBACKA)lpCallbackContext->lpCallback)(&DI, lpCallbackContext->pvRef);
}

BOOL CALLBACK m_IDirectInputEnumDevice::EnumDeviceCallbackW(LPCDIDEVICEINSTANCEW lpddi, LPVOID pvRef)
{
	ENUMDEVICE *lpCallbackContext = (ENUMDEVICE*)pvRef;

	DIDEVICEINSTANCEW DI;
	CopyMemory(&DI, lpddi, lpddi->dwSize);

	DI.dwDevType = (lpddi->dwDevType & ~0xFFFF) |													// Remove device type and sub type
		ConvertDevSubTypeTo7(lpddi->dwDevType & 0xFF, (lpddi->dwDevType & 0xFF00) >> 8) << 8 |		// Add converted sub type
		ConvertDevTypeTo7(lpddi->dwDevType & 0xFF);													// Add converted device type

	return ((LPDIENUMDEVICESCALLBACKW)lpCallbackContext->lpCallback)(&DI, lpCallbackContext->pvRef);
}
