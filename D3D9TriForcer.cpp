#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <D3D9.h>
#include <detours.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "detours.lib")

typedef IDirect3D9* (WINAPI *Direct3DCreate9Ptr)(UINT SDKVersion);
typedef HRESULT (WINAPI *Direct3DCreate9ExPtr)(UINT SDKVersion, IDirect3D9Ex **direct_3d_9_ex);

static HMODULE d3d9_module;
static Direct3DCreate9Ptr direct3d_create_9 = 0;
static Direct3DCreate9ExPtr direct3d_create_9_ex = 0;

static HRESULT (STDMETHODCALLTYPE *RealCreateDevice)(IDirect3D9 *This, UINT Adapter, D3DDEVTYPE DeviceType, 
	HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, 
	IDirect3DDevice9** ppReturnedDeviceInterface);

static HRESULT (STDMETHODCALLTYPE *RealSetSamplerState)(IDirect3DDevice9 *This, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if(fdwReason != DLL_PROCESS_ATTACH) {
		return TRUE;
	}

	d3d9_module = LoadLibrary(L"C:/Windows/System32/d3d9.dll");
	direct3d_create_9 = (Direct3DCreate9Ptr)GetProcAddress(d3d9_module, "Direct3DCreate9");
	direct3d_create_9_ex = (Direct3DCreate9ExPtr)GetProcAddress(d3d9_module, "Direct3DCreate9Ex");

	return TRUE;
}

HRESULT WINAPI HookedSetSamplerState(IDirect3DDevice9 *This, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	return RealSetSamplerState(This, Sampler, Type, Value);
}

void HookDirect3DDevice9(IDirect3DDevice9 *device)
{
	RealSetSamplerState = device->lpVtbl->SetSamplerState;

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)RealSetSamplerState, HookedSetSamplerState);
	DetourTransactionCommit();
}

HRESULT WINAPI HookedCreateDevice(IDirect3D9 *This, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, 
	D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
{
	HRESULT hresult = RealCreateDevice(This, Adapter, DeviceType, hFocusWindow, 
		BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	HookDirect3DDevice9(*ppReturnedDeviceInterface);

	return hresult;
}

void HookDirect3D9(IDirect3D9 *direct3d9)
{
	RealCreateDevice = direct3d9->lpVtbl->CreateDevice;

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)RealCreateDevice, HookedCreateDevice);
	DetourTransactionCommit();
}

IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion)
{
	IDirect3D9 *direct3d9 = direct3d_create_9(SDKVersion);
	HookDirect3D9(direct3d9);
	return direct3d9;
}

HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **direct_3d_9_ex)
{
	return direct3d_create_9_ex(SDKVersion, direct_3d_9_ex);
}