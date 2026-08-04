#pragma once

#include "Waffle/Core/Base.h"
#include "Layer.h"
#include <vector>

namespace Waffle {

	class LayerStack
	{
	public:
		LayerStack() = default;
		~LayerStack();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void PopLayer(Layer* layer);
		void PopOverlay(Layer* overlay);

		void Clear();
		size_t GetCount() const { return m_Layers.size(); }
		bool HasLayer(Layer* layer) const;

		// Standard Iterators
		std::vector<Scope<Layer>>::iterator begin() { return m_Layers.begin(); }
		std::vector<Scope<Layer>>::iterator end() { return m_Layers.end(); }
		std::vector<Scope<Layer>>::const_iterator begin() const { return m_Layers.begin(); }
		std::vector<Scope<Layer>>::const_iterator end() const { return m_Layers.end(); }

		// Reverse Iterators (for Event Propagation & Top-to-Bottom destruction)
		std::vector<Scope<Layer>>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
		std::vector<Scope<Layer>>::reverse_iterator rend() { return m_Layers.rend(); }
		std::vector<Scope<Layer>>::const_reverse_iterator rbegin() const { return m_Layers.rbegin(); }
		std::vector<Scope<Layer>>::const_reverse_iterator rend() const { return m_Layers.rend(); }

		// Const Iterators
		std::vector<Scope<Layer>>::const_iterator cbegin() const { return m_Layers.cbegin(); }
		std::vector<Scope<Layer>>::const_iterator cend() const { return m_Layers.cend(); }
		std::vector<Scope<Layer>>::const_reverse_iterator crbegin() const { return m_Layers.crbegin(); }
		std::vector<Scope<Layer>>::const_reverse_iterator crend() const { return m_Layers.crend(); }

	private:
		std::vector<Scope<Layer>> m_Layers;
		uint32_t m_LayerInsertIndex = 0;
	};
}