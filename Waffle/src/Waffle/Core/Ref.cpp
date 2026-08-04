#include "wfpch.h"
#include "Ref.h"

namespace Waffle {

	namespace RefUtils {
		struct LiveRefContext
		{
			std::unordered_set<void*> LiveReferences;
			std::mutex Mutex;
			bool IsActive = true;

			~LiveRefContext()
			{
				IsActive = false;
			}
		};

		static LiveRefContext& GetContext()
		{
			static LiveRefContext context;
			return context;
		}

		void AddToLiveReferences(void* instance)
		{
			if (!instance) return;
			auto& ctx = GetContext();
			if (!ctx.IsActive) return;
			std::lock_guard<std::mutex> lock(ctx.Mutex);
			ctx.LiveReferences.insert(instance);
		}

		void RemoveFromLiveReferences(void* instance)
		{
			if (!instance) return;
			auto& ctx = GetContext();
			if (!ctx.IsActive) return;
			std::lock_guard<std::mutex> lock(ctx.Mutex);
			ctx.LiveReferences.erase(instance);
		}

		bool IsLive(void* instance)
		{
			if (!instance) return false;
			auto& ctx = GetContext();
			if (!ctx.IsActive) return false;
			std::lock_guard<std::mutex> lock(ctx.Mutex);
			return ctx.LiveReferences.find(instance) != ctx.LiveReferences.end();
		}
	}

	RefCounted::RefCounted()
	{
		RefUtils::AddToLiveReferences(this);
	}

	RefCounted::~RefCounted()
	{
		RefUtils::RemoveFromLiveReferences(this);
	}

}
