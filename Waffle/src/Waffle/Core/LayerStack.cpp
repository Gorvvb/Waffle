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

		// Deferred while iterating - see header. NOTE: ownership of `layer`
		// transfers at call time either way; the layer is kept alive in the
		// pending list until the flush applies it.
		if (IsIterating())
		{
			m_PendingOps.push_back({ PendingOp::PushLayer, layer });
			return;
		}

		layer->OnAttach();
		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
		m_LayerInsertIndex++;
	}

	void LayerStack::PushOverlay(Layer* overlay)
	{
		if (!overlay) return;

		if (IsIterating())
		{
			m_PendingOps.push_back({ PendingOp::PushOverlay, overlay });
			return;
		}

		overlay->OnAttach();
		m_Layers.emplace_back(overlay);
	}

	void LayerStack::PopLayer(Layer* layer)
	{
		if (!layer) return;

		// Already pending a push from this iteration - cancel it instead.
		for (auto it = m_PendingOps.begin(); it != m_PendingOps.end(); ++it)
		{
			if (it->second == layer &&
				(it->first == PendingOp::PushLayer || it->first == PendingOp::PushOverlay))
			{
				delete layer;
				m_PendingOps.erase(it);
				return;
			}
		}

		if (IsIterating())
		{
			m_PendingOps.push_back({ PendingOp::PopLayer, layer });
			return;
		}

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

		for (auto it = m_PendingOps.begin(); it != m_PendingOps.end(); ++it)
		{
			if (it->second == overlay &&
				(it->first == PendingOp::PushLayer || it->first == PendingOp::PushOverlay))
			{
				delete overlay;
				m_PendingOps.erase(it);
				return;
			}
		}

		if (IsIterating())
		{
			m_PendingOps.push_back({ PendingOp::PopOverlay, overlay });
			return;
		}

		auto it = std::find_if(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(),
			[overlay](const Scope<Layer>& l) { return l.get() == overlay; });

		if (it != m_Layers.end())
		{
			(*it)->OnDetach();
			m_Layers.erase(it);
		}
	}

	void LayerStack::EndIteration()
	{
		if (m_IterationDepth > 0)
			m_IterationDepth--;
		if (m_IterationDepth == 0)
			FlushPendingOperations();
	}

	void LayerStack::FlushPendingOperations()
	{
		// Swap first: OnAttach/OnDetach may (legally) push or pop more layers,
		// which append to the (fresh) pending list and flush at the next
		// EndIteration boundary.
		std::vector<std::pair<PendingOp, Layer*>> ops = std::move(m_PendingOps);
		m_PendingOps.clear();

		for (auto& [op, layer] : ops)
		{
			switch (op)
			{
			case PendingOp::PushLayer:
				if (!HasLayer(layer))
				{
					layer->OnAttach();
					m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
					m_LayerInsertIndex++;
				}
				else
					delete layer; // duplicate push - don't leak it
				break;
			case PendingOp::PushOverlay:
				if (!HasLayer(layer))
				{
					layer->OnAttach();
					m_Layers.emplace_back(layer);
				}
				else
					delete layer;
				break;
			case PendingOp::PopLayer:
			{
				auto it = std::find_if(m_Layers.begin(), m_Layers.begin() + m_LayerInsertIndex,
					[layer](const Scope<Layer>& l) { return l.get() == layer; });
				if (it != m_Layers.begin() + m_LayerInsertIndex)
				{
					(*it)->OnDetach();
					m_Layers.erase(it);
					m_LayerInsertIndex--;
				}
				break;
			}
			case PendingOp::PopOverlay:
			{
				auto it = std::find_if(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(),
					[layer](const Scope<Layer>& l) { return l.get() == layer; });
				if (it != m_Layers.end())
				{
					(*it)->OnDetach();
					m_Layers.erase(it);
				}
				break;
			}
			}
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

		// Pending pushes never attached - free the raw pointers.
		for (auto& [op, layer] : m_PendingOps)
		{
			if (op == PendingOp::PushLayer || op == PendingOp::PushOverlay)
				delete layer;
		}
		m_PendingOps.clear();
		m_IterationDepth = 0;
	}

	bool LayerStack::HasLayer(Layer* layer) const
	{
		return std::find_if(m_Layers.begin(), m_Layers.end(),
			[layer](const Scope<Layer>& l) { return l.get() == layer; }) != m_Layers.end();
	}
}
