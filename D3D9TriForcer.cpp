#include <D3D9.h>

#pragma comment(lib, "d3d9.lib")

typedef IDirect3D9* (__stdcall *Direct3DCreate9Ptr)(UINT SDKVersion);
typedef HRESULT (__stdcall *Direct3DCreate9ExPtr)(UINT SDKVersion, IDirect3D9Ex **direct_3d_9_ex);

static HMODULE d3d9_module;
static Direct3DCreate9Ptr direct3d_create_9 = 0;
static Direct3DCreate9ExPtr direct3d_create_9_ex = 0;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if(fdwReason != DLL_PROCESS_ATTACH) {
		return true;
	}

	d3d9_module = LoadLibrary(L"C:/Windows/System32/d3d9.dll");
	direct3d_create_9 = (Direct3DCreate9Ptr)GetProcAddress(d3d9_module, "Direct3DCreate9");
	direct3d_create_9_ex = (Direct3DCreate9ExPtr)GetProcAddress(d3d9_module, "Direct3DCreate9Ex");

	return true;
}

IDirect3D9* __stdcall Direct3DCreate9(UINT SDKVersion)
{
	return direct3d_create_9(SDKVersion);
}

HRESULT __stdcall Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **direct_3d_9_ex)
{
	return direct3d_create_9_ex(SDKVersion, direct_3d_9_ex);
}