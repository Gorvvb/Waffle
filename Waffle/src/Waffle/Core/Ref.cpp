#include "wfpch.h"
#include "Ref.h"

namespace Waffle {

	namespace RefUtils {
		// Tracking is always on: WeakRef::IsValid/Lock depend on it, and with
		// it disabled they degenerated into `instance != nullptr` - a freed
		// object "resurrected" through Lock() (UB). The mutex + set cost is
		// negligible next to a heap allocation.
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
