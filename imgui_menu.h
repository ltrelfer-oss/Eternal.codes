#pragma once

class ImGuiMenu {
	friend class GUI;

public:
	static void SetupStyle( );
	static void Render( );

private:
	static void RenderElement( Element* e );

	static void RenderCheckbox( Checkbox* cb );
	static void RenderSlider( Slider* sl );
	static void RenderDropdown( Dropdown* dd );
	static void RenderMultiDropdown( MultiDropdown* mdd );
	static void RenderKeybind( Keybind* kb );
	static void RenderColorpicker( Colorpicker* cp );
	static void RenderEdit( Edit* ed );
	static void RenderButton( Button* bt );

	static int s_active_tab;
};
