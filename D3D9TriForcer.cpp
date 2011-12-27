#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <D3D9.h>
#include <detours.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "detours.lib")

#include <unordered_map>
#include <memory>

// Direct3D prototypes
typedef IDirect3D9* (WINAPI *Direct3DCreate9Ptr)(UINT SDKVersion);
typedef HRESULT (WINAPI *Direct3DCreate9ExPtr)(UINT SDKVersion, IDirect3D9Ex **direct_3d_9_ex);
typedef HRESULT (STDMETHODCALLTYPE *CreateDevicePtr)(IDirect3D9 *This, UINT Adapter, D3DDEVTYPE DeviceType, 
	HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, 
	IDirect3DDevice9** ppReturnedDeviceInterface);
typedef HRESULT (STDMETHODCALLTYPE *SetSamplerStatePtr)(IDirect3DDevice9 *This, DWORD Sampler, 
	D3DSAMPLERSTATETYPE Type, DWORD Value);

// Globals
static HMODULE d3d9_module;
static Direct3DCreate9Ptr RealDirect3DCreate9 = 0;

// Map from original addresses to detours trampoline addresses.
// Trampoline addresses are stored on the heap, because they need to be non-movable
static std::unordered_map<CreateDevicePtr, std::unique_ptr<CreateDevicePtr>> create_device_ptr_map;
static std::unordered_map<SetSamplerStatePtr, std::unique_ptr<SetSamplerStatePtr>> set_sampler_state_ptr_map;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if(fdwReason != DLL_PROCESS_ATTACH) {
		return TRUE;
	}

	RealDirect3DCreate9 = (Direct3DCreate9Ptr)DetourFindFunction("C:/Windows/System32/d3d9.dll", "Direct3DCreate9");	

	return TRUE;
}

HRESULT WINAPI HookedSetSamplerState(IDirect3DDevice9 *device, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	SetSamplerStatePtr *set_sampler_state_ptr = set_sampler_state_ptr_map[device->lpVtbl->SetSamplerState].get();
	if(set_sampler_state_ptr) {		
		// Set what was asked for
		HRESULT hresult = (*set_sampler_state_ptr)(device, Sampler, Type, Value);

		// Force trilinear, no matter what
		(*set_sampler_state_ptr)(device, Sampler, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

		return hresult;
	}
	else {
		// Hack: Don't know why this happens. Sometimes this function gets called, but 
		// the VTable entry address was not patched by HookDirect3DDevice9, therefore the 
		// detours trampoline can't be determined.		
		// We just return that the call succeeded anyway. For most apps it will work anyway.
		return D3D_OK;
	}
}

void HookDirect3DDevice9(IDirect3DDevice9 *device)
{
	if(set_sampler_state_ptr_map.find(device->lpVtbl->SetSamplerState) == set_sampler_state_ptr_map.end()) {
		SetSamplerStatePtr *set_sampler_state_ptr = new SetSamplerStatePtr(device->lpVtbl->SetSamplerState);
		set_sampler_state_ptr_map[device->lpVtbl->SetSamplerState] = std::unique_ptr<SetSamplerStatePtr>(set_sampler_state_ptr);
	
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach((void**)set_sampler_state_ptr, HookedSetSamplerState);
		DetourTransactionCommit();
	}
}

HRESULT WINAPI HookedCreateDevice(IDirect3D9 *direct3d9, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, 
	DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
{
	CreateDevicePtr *create_device_ptr = create_device_ptr_map[direct3d9->lpVtbl->CreateDevice].get();

	HRESULT hresult = (*create_device_ptr)(direct3d9, Adapter, DeviceType, hFocusWindow, 
		BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
	
	if(hresult == D3D_OK) {
		HookDirect3DDevice9(*ppReturnedDeviceInterface);
	}

	return hresult;
}

void HookDirect3D9(IDirect3D9 *direct3d9)
{	
	if(create_device_ptr_map.find(direct3d9->lpVtbl->CreateDevice) == create_device_ptr_map.end()) {
		CreateDevicePtr *create_device_ptr = new CreateDevicePtr(direct3d9->lpVtbl->CreateDevice);
		create_device_ptr_map[direct3d9->lpVtbl->CreateDevice] = std::unique_ptr<CreateDevicePtr>(create_device_ptr);

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach((void**)create_device_ptr, HookedCreateDevice);
		DetourTransactionCommit();
	}
}

IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion)
{
	IDirect3D9 *direct3d9 = RealDirect3DCreate9(SDKVersion);

	if(direct3d9) {
		HookDirect3D9(direct3d9);
	}

	return direct3d9;
}

HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **direct_3d_9_ex)
{
	// D3D9Ex is not supported
	*direct_3d_9_ex = nullptr;
	return D3DERR_NOTAVAILABLE;
}