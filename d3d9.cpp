#include "includes.h"
#include <d3d9.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment( lib, "d3d9.lib" )

// forward declared in imgui_impl_win32.cpp.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

// vtable indices for IDirect3DDevice9.
#define D3D9_RESET_INDEX     16
#define D3D9_ENDSCENE_INDEX  42

bool g_imgui_initialized = false;
HWND g_game_window = nullptr;

// one-time imgui context + backend init, lazily on first present.
static void InitImGui( IDirect3DDevice9* device ) {
	IMGUI_CHECKVERSION( );
	ImGui::CreateContext( );

	ImGuiIO& io = ImGui::GetIO( );
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	if ( !g_game_window )
		g_game_window = FindWindowA( XOR( "Valve001" ), NULL );

	ImGui_ImplWin32_Init( g_game_window );
	ImGui_ImplDX9_Init( device );

	ImGuiMenu::SetupStyle( );

	g_imgui_initialized = true;
}

long __stdcall Hooks::EndScene( void* device_ptr ) {
	IDirect3DDevice9* device = ( IDirect3DDevice9* )device_ptr;

	if ( !g_imgui_initialized )
		InitImGui( device );

	if ( g_gui.m_open ) {
		ImGui_ImplDX9_NewFrame( );
		ImGui_ImplWin32_NewFrame( );
		ImGui::NewFrame( );

		ImGuiMenu::Render( );

		ImGui::EndFrame( );
		ImGui::Render( );
		ImGui_ImplDX9_RenderDrawData( ImGui::GetDrawData( ) );
	}

	return g_hooks.m_EndScene_original( device_ptr );
}

long __stdcall Hooks::Reset( void* device_ptr, void* params ) {
	if ( g_imgui_initialized )
		ImGui_ImplDX9_InvalidateDeviceObjects( );

	long result = g_hooks.m_Reset_original( device_ptr, params );

	if ( g_imgui_initialized )
		ImGui_ImplDX9_CreateDeviceObjects( );

	return result;
}

void Hooks::InitD3D( ) {
	g_game_window = FindWindowA( XOR( "Valve001" ), NULL );

	// create our own throwaway window so we never disturb the game's device.
	WNDCLASSEXA wc{ };
	wc.cbSize        = sizeof( wc );
	wc.lpfnWndProc   = DefWindowProcA;
	wc.hInstance     = GetModuleHandleA( NULL );
	wc.lpszClassName = XOR( "ec_d3d_dummy" );
	RegisterClassExA( &wc );

	HWND dummy_wnd = CreateWindowA( wc.lpszClassName, "", WS_OVERLAPPEDWINDOW,
		0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL );

	IDirect3D9* d3d = Direct3DCreate9( D3D_SDK_VERSION );
	if ( !d3d ) {
		if ( dummy_wnd ) DestroyWindow( dummy_wnd );
		UnregisterClassA( wc.lpszClassName, wc.hInstance );
		return;
	}

	D3DPRESENT_PARAMETERS pp{ };
	pp.Windowed         = TRUE;
	pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
	pp.hDeviceWindow    = dummy_wnd;
	pp.BackBufferFormat = D3DFMT_UNKNOWN;

	IDirect3DDevice9* dummy_device = nullptr;
	HRESULT hr = d3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, dummy_wnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
		&pp, &dummy_device );

	if ( FAILED( hr ) || !dummy_device ) {
		d3d->Release( );
		if ( dummy_wnd ) DestroyWindow( dummy_wnd );
		UnregisterClassA( wc.lpszClassName, wc.hInstance );
		return;
	}

	// the vtable is shared by every IDirect3DDevice9 (incl. the game's), so
	// patching it here redirects the game's EndScene/Reset too.
	void** vtable = *( void*** )dummy_device;

	DWORD old_protect{ };

	VirtualProtect( &vtable[ D3D9_ENDSCENE_INDEX ], sizeof( void* ), PAGE_EXECUTE_READWRITE, &old_protect );
	m_EndScene_original = ( EndScene_t )vtable[ D3D9_ENDSCENE_INDEX ];
	vtable[ D3D9_ENDSCENE_INDEX ] = ( void* )&Hooks::EndScene;
	VirtualProtect( &vtable[ D3D9_ENDSCENE_INDEX ], sizeof( void* ), old_protect, &old_protect );

	VirtualProtect( &vtable[ D3D9_RESET_INDEX ], sizeof( void* ), PAGE_EXECUTE_READWRITE, &old_protect );
	m_Reset_original = ( Reset_t )vtable[ D3D9_RESET_INDEX ];
	vtable[ D3D9_RESET_INDEX ] = ( void* )&Hooks::Reset;
	VirtualProtect( &vtable[ D3D9_RESET_INDEX ], sizeof( void* ), old_protect, &old_protect );

	// done grabbing the vtable; drop our throwaway device + window.
	dummy_device->Release( );
	d3d->Release( );

	if ( dummy_wnd )
		DestroyWindow( dummy_wnd );
	UnregisterClassA( wc.lpszClassName, wc.hInstance );
}
