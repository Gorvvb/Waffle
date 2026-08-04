#include "wfpch.h"
#include "LayerStack.h"

namespace Waffle {

	LayerStack::~LayerStack()
	{
		Clear();
	}

	void LayerStack::PushLayer(Layer* layer)
	{
		if (!layer) return;

		layer->OnAttach();
		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
		m_LayerInsertIndex++;
	}

	void LayerStack::PushOverlay(Layer* overlay)
	{
		if (!overlay) return;

		overlay->OnAttach();
		m_Layers.emplace_back(overlay);
	}

	void LayerStack::PopLayer(Layer* layer)
	{
		if (!layer) return;

		auto it = std::find_if(m_Layers.begin(), m_Layers.begin() + m_LayerInsertIndex,
			[layer](const Scope<Layer>& l) { return l.get() == layer; });

		if (it != m_Layers.begin() + m_LayerInsertIndex)
		{
			(*it)->OnDetach();
			m_Layers.erase(it);
			m_LayerInsertIndex--;
		}
	}

	void LayerStack::PopOverlay(Layer* overlay)
	{
		if (!overlay) return;

		auto it = std::find_if(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(),
			[overlay](const Scope<Layer>& l) { return l.get() == overlay; });

		if (it != m_Layers.end())
		{
			(*it)->OnDetach();
			m_Layers.erase(it);
		}
	}

	void LayerStack::Clear()
	{
		// Detach and destroy layers in REVERSE order (top-to-bottom: overlays first down to base layers)
		for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it)
		{
			if (*it)
			{
				(*it)->OnDetach();
			}
		}
		m_Layers.clear();
		m_LayerInsertIndex = 0;
	}

	bool LayerStack::HasLayer(Layer* layer) const
	{
		return std::find_if(m_Layers.begin(), m_Layers.end(),
			[layer](const Scope<Layer>& l) { return l.get() == layer; }) != m_Layers.end();
	}
}