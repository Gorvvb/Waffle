#pragma once

#include "Waffle/ImGui/Colors.h"
#include "Waffle/Core/UUID.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <utility>

namespace Waffle::ImGuiEx {

	class ScopedStyle
	{
	public:
		ScopedStyle(const ScopedStyle&) = delete;
		ScopedStyle& operator=(const ScopedStyle&) = delete;
		template<typename T>
		ScopedStyle(ImGuiStyleVar styleVar, T value) { ImGui::PushStyleVar(styleVar, value); }
		~ScopedStyle() { ImGui::PopStyleVar(); }
	};

	class ScopedColour
	{
	public:
		ScopedColour(const ScopedColour&) = delete;
		ScopedColour& operator=(const ScopedColour&) = delete;
		template<typename T>
		ScopedColour(ImGuiCol colourId, T colour) { ImGui::PushStyleColor(colourId, ImColor(colour).Value); }
		~ScopedColour() { ImGui::PopStyleColor(); }
	};

	class ScopedFont
	{
	public:
		ScopedFont(const ScopedFont&) = delete;
		ScopedFont& operator=(const ScopedFont&) = delete;
		ScopedFont(ImFont* font) { ImGui::PushFont(font); }
		~ScopedFont() { ImGui::PopFont(); }
	};

	class ScopedID
	{
	public:
		ScopedID(const ScopedID&) = delete;
		ScopedID& operator=(const ScopedID&) = delete;
		template<typename T>
		ScopedID(T id) { ImGui::PushID(id); }
		~ScopedID() { ImGui::PopID(); }
	};

	template<>
	inline ScopedID::ScopedID(UUID id) { ImGui::PushID(reinterpret_cast<const void*>(static_cast<uint64_t>(id))); }

	class ScopedColourStack
	{
	public:
		ScopedColourStack(const ScopedColourStack&) = delete;
		ScopedColourStack& operator=(const ScopedColourStack&) = delete;

		template <typename ColourType, typename... OtherColours>
		ScopedColourStack(ImGuiCol firstColourID, ColourType firstColour, OtherColours&& ... otherColourPairs)
			: m_Count((sizeof... (otherColourPairs) / 2) + 1)
		{
			static_assert ((sizeof... (otherColourPairs) & 1u) == 0,
				"ScopedColourStack constructor expects a list of pairs of colour IDs and colours as its arguments");

			PushColour(firstColourID, firstColour, std::forward<OtherColours>(otherColourPairs)...);
		}

		~ScopedColourStack() { ImGui::PopStyleColor(m_Count); }

	private:
		int m_Count;

		template <typename ColourType, typename... OtherColours>
		void PushColour(ImGuiCol colourID, ColourType colour, OtherColours&& ... otherColourPairs)
		{
			if constexpr (sizeof... (otherColourPairs) == 0)
			{
				ImGui::PushStyleColor(colourID, ImColor(colour).Value);
			}
			else
			{
				ImGui::PushStyleColor(colourID, ImColor(colour).Value);
				PushColour(std::forward<OtherColours>(otherColourPairs)...);
			}
		}
	};

	class ScopedStyleStack
	{
	public:
		ScopedStyleStack(const ScopedStyleStack&) = delete;
		ScopedStyleStack& operator=(const ScopedStyleStack&) = delete;

		template <typename ValueType, typename... OtherStyles>
		ScopedStyleStack(ImGuiStyleVar firstStyleID, ValueType firstValue, OtherStyles&& ... otherStylePairs)
			: m_Count((sizeof... (otherStylePairs) / 2) + 1)
		{
			static_assert ((sizeof... (otherStylePairs) & 1u) == 0,
				"ScopedStyleStack constructor expects a list of pairs of style IDs and values as its arguments");

			PushStyle(firstStyleID, firstValue, std::forward<OtherStyles>(otherStylePairs)...);
		}

		~ScopedStyleStack() { ImGui::PopStyleVar(m_Count); }

	private:
		int m_Count;

		template <typename ValueType, typename... OtherStyles>
		void PushStyle(ImGuiStyleVar styleID, ValueType value, OtherStyles&& ... otherStylePairs)
		{
			if constexpr (sizeof... (otherStylePairs) == 0)
			{
				ImGui::PushStyleVar(styleID, value);
			}
			else
			{
				ImGui::PushStyleVar(styleID, value);
				PushStyle(std::forward<OtherStyles>(otherStylePairs)...);
			}
		}
	};

	class ScopedItemWidth
	{
	public:
		ScopedItemWidth(float itemWidth) { ImGui::PushItemWidth(itemWidth); }
		~ScopedItemWidth() { ImGui::PopItemWidth(); }
	};

}
