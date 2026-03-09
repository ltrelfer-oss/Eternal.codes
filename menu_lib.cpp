#include "includes.h"

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
namespace {
	std::unordered_map< std::string, std::string > g_aliases;
}

// ---------------------------------------------------------------------------
// Logging / error helpers
// ---------------------------------------------------------------------------
void MenuLib::LogError( const std::string& msg ) {
	// Re-use the existing notification / console system.
	g_notify.add( msg + "\n" );
}

// ---------------------------------------------------------------------------
// Alias system
// ---------------------------------------------------------------------------
bool MenuLib::RegisterAlias( const std::string& word, const std::string& alias ) {
	if( word.find( ' ' ) != std::string::npos ) {
		LogError( "[MenuLib::RegisterAlias] Spaces are not allowed in alias keys (use underscores)." );
		return false;
	}

	if( g_aliases.count( word ) ) {
		LogError( tfm::format( "[MenuLib::RegisterAlias] '%s' is already aliased as '%s'.", word, g_aliases[ word ] ) );
		return false;
	}

	g_aliases[ word ] = alias;
	return true;
}

const std::string* MenuLib::ResolveAlias( const std::string& word ) {
	auto it = g_aliases.find( word );
	if( it != g_aliases.end( ) )
		return &it->second;
	return nullptr;
}

std::string MenuLib::ApplyAliases( const std::string& input ) {
	std::string result = input;
	for( const auto& pair : g_aliases ) {
		std::string token = "<" + pair.first + ">";
		size_t pos = 0;
		while( ( pos = result.find( token, pos ) ) != std::string::npos ) {
			result.replace( pos, token.size( ), pair.second );
			pos += pair.second.size( );
		}
	}
	return result;
}

// ---------------------------------------------------------------------------
// MenuElement
// ---------------------------------------------------------------------------
MenuLib::MenuElement::MenuElement( )
	: m_raw{ nullptr }, m_source{ SourceType::Element }, m_visible{ true } {}

MenuLib::MenuElement::MenuElement( Element* raw, SourceType src )
	: m_raw{ raw }, m_source{ src }, m_visible{ true } {}

// -- value access -----------------------------------------------------------
bool MenuLib::MenuElement::GetBool( ) const {
	if( !m_raw ) return false;
	if( m_raw->m_type == ElementTypes::CHECKBOX )
		return static_cast< Checkbox* >( m_raw )->get( );
	return false;
}

int MenuLib::MenuElement::GetInt( ) const {
	if( !m_raw ) return -1;
	if( m_raw->m_type == ElementTypes::KEYBIND )
		return static_cast< Keybind* >( m_raw )->get( );
	if( m_raw->m_type == ElementTypes::EDIT )
		return static_cast< Edit* >( m_raw )->get( );
	return -1;
}

float MenuLib::MenuElement::GetFloat( ) const {
	if( !m_raw ) return 0.f;
	if( m_raw->m_type == ElementTypes::SLIDER )
		return static_cast< Slider* >( m_raw )->get( );
	return 0.f;
}

size_t MenuLib::MenuElement::GetIndex( ) const {
	if( !m_raw ) return 0;
	if( m_raw->m_type == ElementTypes::DROPDOWN )
		return static_cast< Dropdown* >( m_raw )->get( );
	return 0;
}

bool MenuLib::MenuElement::GetMulti( size_t idx ) const {
	if( !m_raw ) return false;
	if( m_raw->m_type == ElementTypes::MULTI_DROPDOWN )
		return static_cast< MultiDropdown* >( m_raw )->get( idx );
	return false;
}

void MenuLib::MenuElement::SetBool( bool v ) {
	if( !m_raw ) return;
	if( m_raw->m_type == ElementTypes::CHECKBOX )
		static_cast< Checkbox* >( m_raw )->set( v );
}

void MenuLib::MenuElement::SetInt( int v ) {
	if( !m_raw ) return;
	if( m_raw->m_type == ElementTypes::KEYBIND )
		static_cast< Keybind* >( m_raw )->set( v );
	if( m_raw->m_type == ElementTypes::EDIT )
		static_cast< Edit* >( m_raw )->set( v );
}

void MenuLib::MenuElement::SetFloat( float v ) {
	if( !m_raw ) return;
	if( m_raw->m_type == ElementTypes::SLIDER )
		static_cast< Slider* >( m_raw )->set( v );
}

void MenuLib::MenuElement::SetIndex( size_t v ) {
	if( !m_raw ) return;
	if( m_raw->m_type == ElementTypes::DROPDOWN )
		static_cast< Dropdown* >( m_raw )->set( v );
}

// -- raw reference ----------------------------------------------------------
Element* MenuLib::MenuElement::Reference( ) const {
	return m_raw;
}

// -- visibility / enabled ---------------------------------------------------
void MenuLib::MenuElement::SetVisible( bool visible ) {
	m_visible = visible;
	// There is no dedicated set_visible on Element, but we can toggle the
	// show flag which the GUI honours.
	if( m_raw )
		m_raw->m_show = visible;
}

bool MenuLib::MenuElement::IsVisible( ) const {
	return m_visible;
}

void MenuLib::MenuElement::SetEnabled( bool enabled ) {
	if( !m_raw ) return;
	if( enabled )
		m_raw->AddFlags( ElementFlags::CLICK );
	else
		m_raw->RemoveFlags( ElementFlags::CLICK );
}

// -- callback ---------------------------------------------------------------
void MenuLib::MenuElement::SetCallback( Callback_t cb ) {
	if( !m_raw || !cb ) return;

	// We capture `this` and the user callback inside a lambda that matches
	// the raw Element callback signature (void(*)()).  A static map keeps
	// the std::function alive.
	static std::unordered_map< Element*, std::pair< Callback_t, MenuElement* > > s_callbacks;
	s_callbacks[ m_raw ] = { cb, this };

	Element* raw_ptr = m_raw;
	m_raw->SetCallback( [ raw_ptr ]( ) {
		auto it = s_callbacks.find( raw_ptr );
		if( it == s_callbacks.end( ) )
			return;

		auto& [ fn, self ] = it->second;

		if( SAFE_CALLBACKS ) {
			try {
				fn( *self );
			}
			catch( const std::exception& ex ) {
				std::string msg = CALLBACK_MESSAGE;
				const std::string placeholder = "<error>";
				size_t pos = msg.find( placeholder );
				if( pos != std::string::npos )
					msg.replace( pos, placeholder.size( ), ex.what( ) );
				LogError( msg );
			}
			catch( ... ) {
				LogError( CALLBACK_MESSAGE );
			}
		}
		else {
			fn( *self );
		}
	} );
}

// -- meta-information -------------------------------------------------------
std::string MenuLib::MenuElement::Name( ) const {
	if( !m_raw ) return "";
	return m_raw->m_label;
}

std::string MenuLib::MenuElement::Type( ) const {
	if( !m_raw ) return "unknown";
	switch( m_raw->m_type ) {
	case ElementTypes::CHECKBOX:       return "checkbox";
	case ElementTypes::SLIDER:         return "slider";
	case ElementTypes::KEYBIND:        return "keybind";
	case ElementTypes::DROPDOWN:       return "dropdown";
	case ElementTypes::COLORPICKER:    return "colorpicker";
	case ElementTypes::MULTI_DROPDOWN: return "multiselect";
	case ElementTypes::EDIT:           return "edit";
	case ElementTypes::BUTTON:         return "button";
	default:                           return "custom";
	}
}

// -- dependency system ------------------------------------------------------
void MenuLib::MenuElement::Depend( MenuElement& dep ) {
	m_dependencies.push_back( { &dep, false, "" } );
	EvaluateDependencies( );
}

void MenuLib::MenuElement::Depend( MenuElement& dep, const std::string& value ) {
	m_dependencies.push_back( { &dep, true, value } );
	EvaluateDependencies( );
}

void MenuLib::MenuElement::MultiDepend(
	const std::vector< std::pair< MenuElement*, std::string > >& deps )
{
	for( const auto& d : deps ) {
		if( d.second.empty( ) )
			m_dependencies.push_back( { d.first, false, "" } );
		else
			m_dependencies.push_back( { d.first, true, d.second } );
	}
	EvaluateDependencies( );
}

void MenuLib::MenuElement::EvaluateDependencies( ) {
	bool vis = true;

	for( const auto& dep : m_dependencies ) {
		if( !dep.element || !dep.element->Valid( ) ) {
			vis = false;
			break;
		}

		if( dep.has_value ) {
			// Compare using the Contains helper.
			vis = vis && dep.element->Contains( dep.value );
		}
		else {
			// Boolean dependency: the element must evaluate to true.
			vis = vis && dep.element->GetBool( );
		}
	}

	m_visible = vis;
	SetVisible( vis );
}

// -- contains ---------------------------------------------------------------
bool MenuLib::MenuElement::Contains( const std::string& value ) const {
	if( !m_raw ) return false;

	switch( m_raw->m_type ) {
	case ElementTypes::DROPDOWN: {
		auto* dd = static_cast< Dropdown* >( m_raw );
		return dd->GetActiveItem( ) == value;
	}
	case ElementTypes::MULTI_DROPDOWN: {
		auto* mdd  = static_cast< MultiDropdown* >( m_raw );
		auto  items = mdd->GetActiveItems( );
		for( const auto& item : items )
			if( item == value ) return true;
		return false;
	}
	case ElementTypes::CHECKBOX: {
		// Treat "true"/"false" strings.
		bool checked = static_cast< Checkbox* >( m_raw )->get( );
		return ( value == "true" && checked ) || ( value == "false" && !checked );
	}
	default:
		return false;
	}
}

// -- list -------------------------------------------------------------------
std::vector< std::string > MenuLib::MenuElement::List( ) const {
	if( !m_raw ) return {};

	if( m_raw->m_type == ElementTypes::DROPDOWN ) {
		auto* dd = static_cast< Dropdown* >( m_raw );
		// Dropdown stores items in m_items (protected). Access through active item iteration.
		// We return just the active item name.
		std::vector< std::string > out;
		out.push_back( dd->GetActiveItem( ) );
		return out;
	}

	if( m_raw->m_type == ElementTypes::MULTI_DROPDOWN )
		return static_cast< MultiDropdown* >( m_raw )->GetActiveItems( );

	return {};
}

// -- valid ------------------------------------------------------------------
bool MenuLib::MenuElement::Valid( ) const {
	return m_raw != nullptr;
}

// ---------------------------------------------------------------------------
// MenuGroup
// ---------------------------------------------------------------------------
MenuLib::MenuGroup::MenuGroup( )
	: m_tab{ nullptr }, m_form{ nullptr } {}

MenuLib::MenuGroup::MenuGroup( Tab* tab, Form* form )
	: m_tab{ tab }, m_form{ form } {}

Tab*  MenuLib::MenuGroup::GetTab( )  const { return m_tab; }
Form* MenuLib::MenuGroup::GetForm( ) const { return m_form; }

MenuLib::MenuElement MenuLib::MenuGroup::AddCheckbox(
	const std::string& label, const std::string& file_id,
	bool use_label, bool default_val, size_t col )
{
	if( !m_tab ) {
		LogError( "[MenuGroup::AddCheckbox] Group has no associated tab." );
		return MenuElement( );
	}

	// Allocate via new -- lifetime managed by the tab/form.
	auto* cb = new Checkbox( );
	cb->setup( label, file_id, use_label, default_val );
	m_tab->RegisterElement( cb, col );
	return MenuElement( cb );
}

MenuLib::MenuElement MenuLib::MenuGroup::AddSlider(
	const std::string& label, const std::string& file_id,
	float min_val, float max_val, bool use_label,
	int precision, float default_val, float step,
	const std::wstring& suffix, size_t col )
{
	if( !m_tab ) {
		LogError( "[MenuGroup::AddSlider] Group has no associated tab." );
		return MenuElement( );
	}

	auto* sl = new Slider( );
	sl->setup( label, file_id, min_val, max_val, use_label, precision, default_val, step, suffix );
	m_tab->RegisterElement( sl, col );
	return MenuElement( sl );
}

MenuLib::MenuElement MenuLib::MenuGroup::AddDropdown(
	const std::string& label, const std::string& file_id,
	const std::vector< std::string >& items, bool use_label,
	size_t active, size_t col )
{
	if( !m_tab ) {
		LogError( "[MenuGroup::AddDropdown] Group has no associated tab." );
		return MenuElement( );
	}

	auto* dd = new Dropdown( );
	dd->setup( label, file_id, items, use_label, active );
	m_tab->RegisterElement( dd, col );
	return MenuElement( dd );
}

MenuLib::MenuElement MenuLib::MenuGroup::AddMultiDropdown(
	const std::string& label, const std::string& file_id,
	const std::vector< std::string >& items, bool use_label,
	const std::vector< size_t >& active, size_t col )
{
	if( !m_tab ) {
		LogError( "[MenuGroup::AddMultiDropdown] Group has no associated tab." );
		return MenuElement( );
	}

	auto* mdd = new MultiDropdown( );
	mdd->setup( label, file_id, items, use_label, active );
	m_tab->RegisterElement( mdd, col );
	return MenuElement( mdd );
}

MenuLib::MenuElement MenuLib::MenuGroup::AddButton(
	const std::string& label, void( *cb )( ), size_t col )
{
	if( !m_tab ) {
		LogError( "[MenuGroup::AddButton] Group has no associated tab." );
		return MenuElement( );
	}

	auto* btn = new Button( );
	btn->setup( label );
	if( cb )
		btn->SetCallback( cb );
	m_tab->RegisterElement( btn, col );
	return MenuElement( btn );
}

MenuLib::MenuElement MenuLib::MenuGroup::AddKeybind(
	const std::string& label, const std::string& file_id,
	int key, size_t col )
{
	if( !m_tab ) {
		LogError( "[MenuGroup::AddKeybind] Group has no associated tab." );
		return MenuElement( );
	}

	auto* kb = new Keybind( );
	kb->setup( label, file_id, key );
	m_tab->RegisterElement( kb, col );
	return MenuElement( kb );
}

// ---------------------------------------------------------------------------
// Find
// ---------------------------------------------------------------------------
MenuLib::MenuElement MenuLib::Find( Tab* tab, const std::string& file_id ) {
	if( !tab ) {
		LogError( "[MenuLib::Find] Tab is null." );
		return MenuElement( );
	}

	Element* found = tab->GetElementByName( file_id );
	if( !found ) {
		LogError( tfm::format( "[MenuLib::Find] Element '%s' not found.", file_id ) );
		return MenuElement( );
	}

	return MenuElement( found, MenuElement::SourceType::Reference );
}

MenuLib::MenuElement MenuLib::Find( Form* form, const std::string& tab_name,
									const std::string& file_id )
{
	if( !form ) {
		LogError( "[MenuLib::Find] Form is null." );
		return MenuElement( );
	}

	Tab* tab = form->GetTabByName( tab_name );
	if( !tab ) {
		LogError( tfm::format( "[MenuLib::Find] Tab '%s' not found.", tab_name ) );
		return MenuElement( );
	}

	return Find( tab, file_id );
}

// ---------------------------------------------------------------------------
// Contains (free function)
// ---------------------------------------------------------------------------
bool MenuLib::Contains( MenuElement& elem, const std::string& value ) {
	return elem.Contains( value );
}
