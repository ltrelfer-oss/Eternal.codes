#include "includes.h"

GUI	  g_gui{};;
Input g_input{};;

void GUI::think( ) {
	m_open = false;

	// update keyboard input.
	g_input.update( );

	// run bound key callbacks.
	if( !m_forms.empty( ) ) {
		bool done = false;

		for( const auto& f : m_forms ) {
			if( !f || f->m_tabs.empty( ) )
				continue;

			for( const auto& t : f->m_tabs ) {
				if( !t || t->m_elements.empty( ) )
					continue;

				for( const auto& e : t->m_elements ) {
					if( !e || e->m_type != ElementTypes::KEYBIND )
						continue;

					// big cast.
					Keybind* k = ( Keybind* )e;

					if( k->m_toggle && g_input.GetKeyPress( k->get( ) ) ) {
						k->m_toggle( );
						done = true;
						break;
					}	
				}

				if( done )
					break;
			}

			if( done )
				break;
		}
	}

	// sort forms based on last touched tick.
	std::sort( m_forms.begin( ), m_forms.end( ), []( Form* a, Form* b ) {
		return a->m_tick > b->m_tick;
	} );

	// update form open states
	// iterage forms.
	for( const auto& f : m_forms ) {
		if( !f )
			continue;

		if( f->m_key != -1 && g_input.GetKeyPress( f->m_key ) )
			f->toggle( );

		if( !m_open && f->m_open )
			m_open = true;

		// if closed, deactivate any active elements.
		if( !f->m_open && f->m_active_element )
			f->m_active_element = nullptr;
	}

	// update element visibility.
	for( const auto& f : m_forms ) {
		if( !f || !f->m_active_tab || f->m_active_tab->m_elements.empty( ) )
			continue;

		// reset.
		f->m_active_tab->m_element_offset.fill( ELEMENT_BORDER );

		for( auto& e : f->m_active_tab->m_elements ) {
			// default to true.
			e->m_show = true;

			// this element has show callbacks.
			if( e->m_show_callbacks.size( ) ) {

				// iterate all callbacks.
				for( auto& cb : e->m_show_callbacks ) {

					// if one callback evaluates to false.
					// set show to false.
					if( !cb( ) )
						e->m_show = false;
				}
			}

			// this element will be shown, fix its y coordinate.
			if( e->m_show ) {
				// slight correction.
				if( !e->m_use_label )
					f->m_active_tab->m_element_offset[ e->m_col ] -= ( ELEMENT_DISTANCE - ELEMENT_COMBINED_DISTANCE );

				e->m_pos.y = f->m_active_tab->m_element_offset[ e->m_col ];

				// set offset for next element.
				f->m_active_tab->m_element_offset[ e->m_col ] += ( ELEMENT_COMBINED_DISTANCE + e->m_base_h );
			}
		}
	}

	// note: rendering + mouse interaction are handled by ImGui (see imgui_menu.cpp).
}

void GUI::draw( ) {
	// sort forms based on last touched tick ( in reverse ).
	std::sort( m_forms.begin( ), m_forms.end( ), []( Form* a, Form* b ) {
		return a->m_tick < b->m_tick;
	} );

	// iterate forms.
	for( const auto& f : m_forms ) {
		if( !f )
			continue;

		f->draw( );
	}
}