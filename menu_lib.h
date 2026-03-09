#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <stdexcept>

// Forward declarations.
class Element;
class Checkbox;
class Dropdown;
class MultiDropdown;
class Slider;
class Keybind;
class Button;
class Colorpicker;
class Edit;
class Tab;
class Form;

// ---------------------------------------------------------------------------
// MenuLib: C++ port of the Lua menu library.
//
// Provides a uniform wrapper (MenuElement) around every concrete Element type,
// a grouping concept (MenuGroup), an alias system, a dependency system,
// safe-callback support, and convenience helpers (contains / find).
// ---------------------------------------------------------------------------
namespace MenuLib {

	// -- Settings -----------------------------------------------------------
	inline bool        DEBUG_MODE       = false;
	inline bool        SAFE_CALLBACKS   = false;
	inline std::string CALLBACK_MESSAGE = "Callback failed: <error>";

	// -- Logging / error helpers -------------------------------------------
	void LogError( const std::string& msg );

	// -- Alias system -------------------------------------------------------
	bool               RegisterAlias( const std::string& word, const std::string& alias );
	const std::string* ResolveAlias( const std::string& word );
	std::string        ApplyAliases( const std::string& input );

	// -----------------------------------------------------------------------
	// MenuElement -- wraps any concrete Element* with a uniform API that
	// mirrors the Lua `struct` returned by menu element constructors.
	// -----------------------------------------------------------------------
	class MenuElement {
	public:
		// Underlying element type tag (matches the Lua menu_type).
		enum class SourceType {
			Element,    // created via the menu lib
			Reference   // obtained via MenuLib::Find
		};

		MenuElement( );
		explicit MenuElement( Element* raw, SourceType src = SourceType::Element );

		// -- value access ---------------------------------------------------
		// Because every concrete Element subclass exposes `get()` in a
		// different way (bool, int, float, size_t ...), generic wrappers are
		// provided.  Callers that know the concrete type can also use typed().
		bool        GetBool( )  const;  // Checkbox
		int         GetInt( )   const;  // Keybind, Edit
		float       GetFloat( ) const;  // Slider
		size_t      GetIndex( ) const;  // Dropdown
		bool        GetMulti( size_t idx ) const; // MultiDropdown

		void SetBool( bool v );
		void SetInt( int v );
		void SetFloat( float v );
		void SetIndex( size_t v );

		// -- raw reference --------------------------------------------------
		Element* Reference( ) const;

		// -- visibility / enabled -------------------------------------------
		void SetVisible( bool visible );
		bool IsVisible( ) const;

		void SetEnabled( bool enabled );

		// -- callback -------------------------------------------------------
		using Callback_t = std::function< void( MenuElement& ) >;
		void SetCallback( Callback_t cb );

		// -- meta-information -----------------------------------------------
		std::string Name( ) const;
		std::string Type( ) const;

		// -- dependency system ----------------------------------------------
		struct Dependency {
			MenuElement* element;
			bool         has_value;
			std::string  value;    // string comparison for multi-type support
		};

		void Depend( MenuElement& dep );
		void Depend( MenuElement& dep, const std::string& value );
		void MultiDepend( const std::vector< std::pair< MenuElement*, std::string > >& deps );
		void EvaluateDependencies( );

		// -- contains -------------------------------------------------------
		bool Contains( const std::string& value ) const;

		// -- list (for dropdown / multiselect items) ------------------------
		std::vector< std::string > List( ) const;

		// -- valid check ----------------------------------------------------
		bool Valid( ) const;
		explicit operator bool( ) const { return Valid( ); }

	private:
		Element*                  m_raw;
		SourceType                m_source;
		std::vector< Dependency > m_dependencies;
		bool                      m_visible;
	};

	// -----------------------------------------------------------------------
	// MenuGroup -- organises elements by (tab, container) pair, mirroring
	// the Lua `menu.group(tab, container)` concept.
	// -----------------------------------------------------------------------
	class MenuGroup {
	public:
		MenuGroup( );
		MenuGroup( Tab* tab, Form* form );

		Tab*  GetTab( )  const;
		Form* GetForm( ) const;

		// Convenience: create elements through the group.
		MenuElement AddCheckbox( const std::string& label, const std::string& file_id,
								bool use_label = true, bool default_val = false, size_t col = 0 );

		MenuElement AddSlider( const std::string& label, const std::string& file_id,
							   float min_val, float max_val, bool use_label = true,
							   int precision = 0, float default_val = 0.f, float step = 1.f,
							   const std::wstring& suffix = L"", size_t col = 0 );

		MenuElement AddDropdown( const std::string& label, const std::string& file_id,
								const std::vector< std::string >& items, bool use_label = true,
								size_t active = 0, size_t col = 0 );

		MenuElement AddMultiDropdown( const std::string& label, const std::string& file_id,
									  const std::vector< std::string >& items, bool use_label = true,
									  const std::vector< size_t >& active = {}, size_t col = 0 );

		MenuElement AddButton( const std::string& label, void( *cb )( ) = nullptr, size_t col = 0 );

		MenuElement AddKeybind( const std::string& label, const std::string& file_id,
								int key = -1, size_t col = 0 );

	private:
		Tab*  m_tab;
		Form* m_form;
	};

	// -- find ---------------------------------------------------------------
	// Wraps an existing element by searching tab elements (mirrors the Lua
	// `menu.find(...)` which calls `ui.reference`).
	MenuElement Find( Tab* tab, const std::string& file_id );
	MenuElement Find( Form* form, const std::string& tab_name, const std::string& file_id );

	// -- contains helper (operates on a MenuElement) -----------------------
	bool Contains( MenuElement& elem, const std::string& value );

} // namespace MenuLib
