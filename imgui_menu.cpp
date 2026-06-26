#include "includes.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

// key-name lookup lives in keybind.cpp.
extern const std::string& key_name( int key );

int ImGuiMenu::s_active_tab = 0;

// keybind currently waiting for a key press.
static Keybind* g_capturing = nullptr;

namespace {
	// neverlose-ish palette.
	constexpr ImU32 COL_ACCENT      = IM_COL32( 45, 140, 255, 255 );
	constexpr ImU32 COL_ACCENT_DIM  = IM_COL32( 45, 140, 255, 90 );
	constexpr ImU32 COL_TRACK       = IM_COL32( 48, 51, 64, 255 );
	constexpr ImU32 COL_CARD        = IM_COL32( 18, 20, 28, 255 );
	constexpr ImU32 COL_CARD_BORDER = IM_COL32( 34, 37, 48, 255 );
	constexpr ImU32 COL_TEXT        = IM_COL32( 224, 226, 232, 255 );
	constexpr ImU32 COL_TEXT_DIM    = IM_COL32( 120, 124, 138, 255 );
	constexpr ImU32 COL_WHITE       = IM_COL32( 255, 255, 255, 255 );

	constexpr float CONTENT_PAD   = 14.f;
	constexpr float CTRL_WIDTH    = 116.f;
	constexpr float ROW_LABEL_PAD = 2.f;

	// label on the left of a row, returns the x at which the control should start.
	float row_label( const std::string& text, float ctrl_w ) {
		float avail = ImGui::GetContentRegionAvail( ).x;
		float y     = ImGui::GetCursorPosY( );

		ImGui::AlignTextToFramePadding( );
		ImGui::TextColored( ImColor( COL_TEXT ), "%s", text.c_str( ) );

		ImGui::SameLine( );
		ImGui::SetCursorPosY( y );
		ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + ( avail - ctrl_w ) - ImGui::GetStyle( ).ItemSpacing.x );
		return ctrl_w;
	}

	// custom pill toggle switch.
	bool toggle_switch( const char* id, bool* v ) {
		ImDrawList* draw = ImGui::GetWindowDrawList( );
		ImVec2      p    = ImGui::GetCursorScreenPos( );

		float h = ImGui::GetFrameHeight( ) * 0.78f;
		float w = h * 1.85f;
		float r = h * 0.5f;

		ImGui::InvisibleButton( id, ImVec2( w, h ) );
		bool clicked = ImGui::IsItemClicked( );
		if( clicked )
			*v = !*v;

		bool   hovered = ImGui::IsItemHovered( );
		ImU32  bg      = *v ? COL_ACCENT : COL_TRACK;
		if( hovered && !*v )
			bg = IM_COL32( 60, 63, 78, 255 );

		draw->AddRectFilled( p, ImVec2( p.x + w, p.y + h ), bg, r );

		float knob_x = *v ? ( p.x + w - r ) : ( p.x + r );
		draw->AddCircleFilled( ImVec2( knob_x, p.y + r ), r - 2.5f, COL_WHITE );
		return clicked;
	}
}

void ImGuiMenu::SetupStyle( ) {
	ImGuiStyle& s = ImGui::GetStyle( );

	s.WindowRounding    = 8.f;
	s.ChildRounding     = 6.f;
	s.FrameRounding     = 4.f;
	s.PopupRounding     = 4.f;
	s.GrabRounding      = 4.f;
	s.ScrollbarRounding = 4.f;
	s.WindowBorderSize  = 0.f;
	s.ChildBorderSize   = 1.f;
	s.FrameBorderSize   = 0.f;
	s.PopupBorderSize   = 1.f;
	s.WindowPadding     = ImVec2( 0.f, 0.f );
	s.FramePadding      = ImVec2( 8.f, 4.f );
	s.ItemSpacing       = ImVec2( 8.f, 9.f );
	s.ItemInnerSpacing  = ImVec2( 6.f, 6.f );
	s.GrabMinSize       = 12.f;
	s.ScrollbarSize     = 9.f;

	ImVec4* c = s.Colors;
	c[ ImGuiCol_Text ]            = ImColor( COL_TEXT );
	c[ ImGuiCol_TextDisabled ]    = ImColor( COL_TEXT_DIM );
	c[ ImGuiCol_WindowBg ]        = ImColor( 11, 12, 18, 255 );
	c[ ImGuiCol_ChildBg ]         = ImColor( COL_CARD );
	c[ ImGuiCol_PopupBg ]         = ImColor( 16, 18, 26, 255 );
	c[ ImGuiCol_Border ]          = ImColor( COL_CARD_BORDER );
	c[ ImGuiCol_FrameBg ]         = ImColor( 24, 26, 36, 255 );
	c[ ImGuiCol_FrameBgHovered ]  = ImColor( 30, 33, 45, 255 );
	c[ ImGuiCol_FrameBgActive ]   = ImColor( 34, 38, 52, 255 );
	c[ ImGuiCol_Button ]          = ImColor( 24, 26, 36, 255 );
	c[ ImGuiCol_ButtonHovered ]   = ImColor( 30, 33, 45, 255 );
	c[ ImGuiCol_ButtonActive ]    = ImColor( 45, 140, 255, 255 );
	c[ ImGuiCol_Header ]          = ImColor( 45, 140, 255, 60 );
	c[ ImGuiCol_HeaderHovered ]   = ImColor( 45, 140, 255, 90 );
	c[ ImGuiCol_HeaderActive ]    = ImColor( 45, 140, 255, 130 );
	c[ ImGuiCol_SliderGrab ]      = ImColor( COL_ACCENT );
	c[ ImGuiCol_SliderGrabActive ]= ImColor( 90, 170, 255, 255 );
	c[ ImGuiCol_CheckMark ]       = ImColor( COL_ACCENT );
	c[ ImGuiCol_ScrollbarBg ]     = ImColor( 0, 0, 0, 0 );
	c[ ImGuiCol_ScrollbarGrab ]   = ImColor( 40, 43, 56, 255 );
	c[ ImGuiCol_ScrollbarGrabHovered ] = ImColor( 50, 53, 68, 255 );
}

void ImGuiMenu::RenderCheckbox( Checkbox* cb ) {
	bool v = cb->m_checked;
	if( cb->m_use_label && !cb->m_label.empty( ) )
		row_label( cb->m_label, ImGui::GetFrameHeight( ) * 1.85f );

	if( toggle_switch( "##t", &v ) )
		cb->set( v );
}

void ImGuiMenu::RenderSlider( Slider* sl ) {
	// suffix to narrow string (ascii degree -> 'deg' fallback handled by font).
	std::string suffix;
	for( wchar_t ch : sl->m_suffix )
		suffix += ( ch < 128 ) ? static_cast< char >( ch ) : '\0';

	char fmt[ 32 ];
	if( sl->m_precision <= 0 )
		_snprintf_s( fmt, sizeof( fmt ), "%%.0f%s", suffix.c_str( ) );
	else
		_snprintf_s( fmt, sizeof( fmt ), "%%.%df%s", sl->m_precision, suffix.c_str( ) );

	if( sl->m_use_label && !sl->m_label.empty( ) ) {
		// label left, value right, track fills the middle.
		float avail = ImGui::GetContentRegionAvail( ).x;
		float y     = ImGui::GetCursorPosY( );

		ImGui::AlignTextToFramePadding( );
		ImGui::TextColored( ImColor( COL_TEXT ), "%s", sl->m_label.c_str( ) );

		char val_buf[ 48 ];
		_snprintf_s( val_buf, sizeof( val_buf ), fmt, sl->m_value );
		float val_w = ImGui::CalcTextSize( val_buf ).x;

		float track_w = CTRL_WIDTH;
		ImGui::SameLine( );
		ImGui::SetCursorPosY( y );
		ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + ( avail - track_w - val_w - 10.f ) - ImGui::GetStyle( ).ItemSpacing.x );

		float v = sl->m_value;
		ImGui::PushItemWidth( track_w );
		if( ImGui::SliderFloat( "##s", &v, sl->m_min, sl->m_max, "" ) )
			sl->set( v );
		ImGui::PopItemWidth( );

		ImGui::SameLine( );
		ImGui::SetCursorPosY( y );
		ImGui::AlignTextToFramePadding( );
		ImGui::TextColored( ImColor( COL_TEXT_DIM ), "%s", val_buf );
	}
	else {
		float v = sl->m_value;
		ImGui::PushItemWidth( -1 );
		if( ImGui::SliderFloat( "##s", &v, sl->m_min, sl->m_max, fmt ) )
			sl->set( v );
		ImGui::PopItemWidth( );
	}
}

void ImGuiMenu::RenderDropdown( Dropdown* dd ) {
	if( dd->m_use_label && !dd->m_label.empty( ) )
		row_label( dd->m_label, CTRL_WIDTH );
	else
		ImGui::PushItemWidth( -1 );

	float w = ( dd->m_use_label && !dd->m_label.empty( ) ) ? CTRL_WIDTH : ImGui::GetContentRegionAvail( ).x;
	ImGui::SetNextItemWidth( w );

	std::string preview = dd->GetActiveItem( );
	if( ImGui::BeginCombo( "##d", preview.c_str( ) ) ) {
		for( size_t i{}; i < dd->m_items.size( ); ++i ) {
			bool selected = ( dd->m_active_item == i );
			if( ImGui::Selectable( dd->m_items[ i ].c_str( ), selected ) )
				dd->set( i );
			if( selected )
				ImGui::SetItemDefaultFocus( );
		}
		ImGui::EndCombo( );
	}

	if( !( dd->m_use_label && !dd->m_label.empty( ) ) )
		ImGui::PopItemWidth( );
}

void ImGuiMenu::RenderMultiDropdown( MultiDropdown* mdd ) {
	if( mdd->m_use_label && !mdd->m_label.empty( ) )
		row_label( mdd->m_label, CTRL_WIDTH );

	// build preview text.
	std::string preview;
	auto active = mdd->GetActiveItems( );
	for( size_t i{}; i < active.size( ); ++i ) {
		preview += active[ i ];
		if( i + 1 < active.size( ) )
			preview += ", ";
	}
	if( preview.empty( ) )
		preview = XOR( "none" );

	float w = ( mdd->m_use_label && !mdd->m_label.empty( ) ) ? CTRL_WIDTH : ImGui::GetContentRegionAvail( ).x;
	ImGui::SetNextItemWidth( w );

	if( ImGui::BeginCombo( "##md", preview.c_str( ) ) ) {
		for( size_t i{}; i < mdd->m_items.size( ); ++i ) {
			bool selected = mdd->get( i );
			if( ImGui::Selectable( mdd->m_items[ i ].c_str( ), selected, ImGuiSelectableFlags_DontClosePopups ) ) {
				if( selected ) {
					auto it = std::find( mdd->m_active_items.begin( ), mdd->m_active_items.end( ), i );
					if( it != mdd->m_active_items.end( ) )
						mdd->m_active_items.erase( it );
					if( mdd->m_callback )
						mdd->m_callback( );
				}
				else
					mdd->select( i );
			}
		}
		ImGui::EndCombo( );
	}
}

void ImGuiMenu::RenderKeybind( Keybind* kb ) {
	if( !kb->m_label.empty( ) )
		row_label( kb->m_label, CTRL_WIDTH );

	std::string text;
	if( g_capturing == kb )
		text = XOR( "..." );
	else if( kb->m_key >= 0 && kb->m_key <= 0xFE )
		text = key_name( kb->m_key );
	else
		text = XOR( "none" );

	if( ImGui::Button( ( text + "##kb" ).c_str( ), ImVec2( CTRL_WIDTH, 0.f ) ) )
		g_capturing = kb;

	// right-click clears the bind.
	if( ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
		kb->set( -1 );
		if( g_capturing == kb )
			g_capturing = nullptr;
	}

	// capture next key.
	if( g_capturing == kb ) {
		for( int vk = 1; vk <= 0xFE; ++vk ) {
			if( vk == VK_LBUTTON )
				continue;
			if( g_input.GetKeyPress( vk ) ) {
				if( vk == VK_ESCAPE )
					kb->set( -1 );
				else
					kb->set( vk );
				g_capturing = nullptr;
				break;
			}
		}
	}
}

void ImGuiMenu::RenderColorpicker( Colorpicker* cp ) {
	Color col = cp->m_color;
	float c[ 4 ] = {
		col.r( ) / 255.f, col.g( ) / 255.f, col.b( ) / 255.f, col.a( ) / 255.f
	};

	if( !cp->m_label.empty( ) )
		row_label( cp->m_label, 28.f );

	ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf;
	if( ImGui::ColorEdit4( "##cp", c, flags ) ) {
		cp->set( Color{ int( c[ 0 ] * 255.f ), int( c[ 1 ] * 255.f ), int( c[ 2 ] * 255.f ), int( c[ 3 ] * 255.f ) } );
	}
}

void ImGuiMenu::RenderEdit( Edit* ed ) {
	if( !ed->m_label.empty( ) )
		row_label( ed->m_label, CTRL_WIDTH );

	char buf[ 64 ];
	_snprintf_s( buf, sizeof( buf ), "%s", ed->m_text.c_str( ) );

	ImGui::SetNextItemWidth( CTRL_WIDTH );
	if( ImGui::InputText( "##e", buf, sizeof( buf ), ImGuiInputTextFlags_CharsDecimal ) ) {
		ed->m_text = buf;
		if( ed->m_callback )
			ed->m_callback( );
	}
}

void ImGuiMenu::RenderButton( Button* bt ) {
	if( ImGui::Button( ( bt->m_label + "##b" ).c_str( ), ImVec2( -1, 0.f ) ) ) {
		if( bt->m_callback )
			bt->m_callback( );
	}
}

void ImGuiMenu::RenderElement( Element* e ) {
	if( !e || !e->m_show )
		return;

	ImGui::PushID( e );

	switch( e->m_type ) {
	case ElementTypes::CHECKBOX:       RenderCheckbox( ( Checkbox* )e );          break;
	case ElementTypes::SLIDER:         RenderSlider( ( Slider* )e );              break;
	case ElementTypes::KEYBIND:        RenderKeybind( ( Keybind* )e );            break;
	case ElementTypes::DROPDOWN:       RenderDropdown( ( Dropdown* )e );          break;
	case ElementTypes::COLORPICKER:    RenderColorpicker( ( Colorpicker* )e );    break;
	case ElementTypes::MULTI_DROPDOWN: RenderMultiDropdown( ( MultiDropdown* )e );break;
	case ElementTypes::EDIT:           RenderEdit( ( Edit* )e );                  break;
	case ElementTypes::BUTTON:         RenderButton( ( Button* )e );              break;
	default: break;
	}

	ImGui::PopID( );
}

void ImGuiMenu::Render( ) {
	// the main form (first registered).
	if( g_gui.m_forms.empty( ) )
		return;

	Form* form = g_gui.m_forms.front( );
	if( !form || form->m_tabs.empty( ) )
		return;

	ImGuiIO& io = ImGui::GetIO( );
	ImGui::SetNextWindowSize( ImVec2( 720.f, 470.f ), ImGuiCond_FirstUseEver );
	ImGui::SetNextWindowPos( ImVec2( io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f ), ImGuiCond_FirstUseEver, ImVec2( 0.5f, 0.5f ) );

	ImGui::Begin( XOR( "Eternal.Codes##main" ), nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse );

	ImVec2      win_pos  = ImGui::GetWindowPos( );
	ImVec2      win_size = ImGui::GetWindowSize( );
	ImDrawList* draw     = ImGui::GetWindowDrawList( );

	const float sidebar_w = 168.f;
	const float topbar_h  = 46.f;

	// sidebar background.
	draw->AddRectFilled( win_pos, ImVec2( win_pos.x + sidebar_w, win_pos.y + win_size.y ),
		IM_COL32( 14, 15, 22, 255 ), 8.f, ImDrawFlags_RoundCornersLeft );

	// logo.
	draw->AddText( ImVec2( win_pos.x + 18.f, win_pos.y + 16.f ), COL_WHITE, XOR( "ETERNAL" ) );
	draw->AddText( ImVec2( win_pos.x + 18.f + ImGui::CalcTextSize( XOR( "ETERNAL" ) ).x, win_pos.y + 16.f ), COL_ACCENT, XOR( ".CODES" ) );

	// ---- sidebar tabs ----
	ImGui::SetCursorPos( ImVec2( 0.f, topbar_h + 6.f ) );
	ImGui::BeginChild( "##sidebar", ImVec2( sidebar_w, win_size.y - topbar_h - 6.f ), false );
	{
		// group labels by tab title.
		auto group_for = []( const std::string& t ) -> const char* {
			if( t == XOR( "aimbot" ) || t == XOR( "anti-aim" ) || t == XOR( "weapons" ) )
				return "Aimbot";
			if( t == XOR( "players" ) || t == XOR( "visuals" ) )
				return "Visuals";
			return "Miscellaneous";
		};

		const char* last_group = nullptr;
		for( size_t i{}; i < form->m_tabs.size( ); ++i ) {
			Tab* tab = form->m_tabs[ i ];
			if( !tab )
				continue;

			const char* grp = group_for( tab->m_title );
			if( !last_group || strcmp( grp, last_group ) != 0 ) {
				ImGui::Dummy( ImVec2( 0.f, 6.f ) );
				ImGui::SetCursorPosX( 18.f );
				ImGui::TextColored( ImColor( COL_TEXT_DIM ), "%s", grp );
				last_group = grp;
			}

			bool active = ( (int)i == s_active_tab );

			ImGui::SetCursorPosX( 10.f );
			ImVec2 item_pos = ImGui::GetCursorScreenPos( );
			float  item_w   = sidebar_w - 20.f;
			float  item_h   = 30.f;

			ImGui::PushID( (int)i );
			if( ImGui::InvisibleButton( "##tab", ImVec2( item_w, item_h ) ) )
				s_active_tab = (int)i;
			bool hovered = ImGui::IsItemHovered( );
			ImGui::PopID( );

			if( active )
				draw->AddRectFilled( item_pos, ImVec2( item_pos.x + item_w, item_pos.y + item_h ), IM_COL32( 26, 34, 54, 255 ), 5.f );
			else if( hovered )
				draw->AddRectFilled( item_pos, ImVec2( item_pos.x + item_w, item_pos.y + item_h ), IM_COL32( 22, 24, 34, 255 ), 5.f );

			if( active )
				draw->AddRectFilled( item_pos, ImVec2( item_pos.x + 3.f, item_pos.y + item_h ), COL_ACCENT, 2.f );

			// capitalize first letter for display.
			std::string disp = tab->m_title;
			if( !disp.empty( ) )
				disp[ 0 ] = (char)toupper( disp[ 0 ] );

			draw->AddText( ImVec2( item_pos.x + 16.f, item_pos.y + ( item_h - ImGui::GetTextLineHeight( ) ) * 0.5f ),
				active ? COL_WHITE : COL_TEXT, disp.c_str( ) );
		}
	}
	ImGui::EndChild( );

	// ---- top bar ----
	draw->AddLine( ImVec2( win_pos.x + sidebar_w, win_pos.y + topbar_h ), ImVec2( win_pos.x + win_size.x, win_pos.y + topbar_h ), COL_CARD_BORDER, 1.f );

	ImGui::SetCursorPos( ImVec2( sidebar_w + CONTENT_PAD, 13.f ) );
	if( ImGui::Button( XOR( "Save##savebtn" ), ImVec2( 76.f, 22.f ) ) )
		g_config.save( form, XOR( "default" ) );
	ImGui::SameLine( );
	if( ImGui::Button( XOR( "Load##loadbtn" ), ImVec2( 76.f, 22.f ) ) )
		g_config.load( form, XOR( "default" ) );

	// ---- content ----
	if( s_active_tab < 0 || s_active_tab >= (int)form->m_tabs.size( ) )
		s_active_tab = 0;

	Tab* active_tab = form->m_tabs[ s_active_tab ];
	form->m_active_tab = active_tab;

	ImGui::SetCursorPos( ImVec2( sidebar_w + CONTENT_PAD, topbar_h + CONTENT_PAD ) );

	float content_w = win_size.x - sidebar_w - CONTENT_PAD * 2.f;
	float content_h = win_size.y - topbar_h - CONTENT_PAD * 2.f;
	float col_w     = ( content_w - CONTENT_PAD ) * 0.5f;

	for( int col = 0; col < 2; ++col ) {
		if( col == 1 ) {
			ImGui::SameLine( );
			ImGui::SetCursorPosX( sidebar_w + CONTENT_PAD + col_w + CONTENT_PAD );
		}

		char id[ 16 ];
		_snprintf_s( id, sizeof( id ), "##col%d", col );

		ImGui::BeginChild( id, ImVec2( col_w, content_h ), true, ImGuiWindowFlags_AlwaysUseWindowPadding );
		{
			// card header = tab title (left col) / "settings" (right col).
			std::string header = active_tab->m_title;
			if( !header.empty( ) )
				header[ 0 ] = (char)toupper( header[ 0 ] );
			if( col == 1 )
				header += XOR( " options" );

			ImGui::PushStyleColor( ImGuiCol_Text, COL_WHITE );
			ImGui::TextUnformatted( header.c_str( ) );
			ImGui::PopStyleColor( );
			ImGui::Separator( );
			ImGui::Dummy( ImVec2( 0.f, 2.f ) );

			for( auto& e : active_tab->m_elements ) {
				if( !e || (int)e->m_col != col || !e->m_show )
					continue;
				RenderElement( e );
			}
		}
		ImGui::EndChild( );
	}

	ImGui::End( );
}
