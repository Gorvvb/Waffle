#pragma once

#include <atomic>
#include <type_traits>
#include <typeinfo>
#include <cstddef>
#include <unordered_set>
#include <mutex>
#include <utility>

namespace Waffle {

	namespace RefUtils {
		void AddToLiveReferences(void* instance);
		void RemoveFromLiveReferences(void* instance);
		bool IsLive(void* instance);
	}

	class RefCounted
	{
	public:
		RefCounted();
		virtual ~RefCounted();

		uint32_t GetRefCount() const { return m_RefCount.load(std::memory_order_relaxed); }
		void IncRefCount() const { m_RefCount.fetch_add(1, std::memory_order_relaxed); }
		void DecRefCount() const { m_RefCount.fetch_sub(1, std::memory_order_acq_rel); }

	private:
		mutable std::atomic<uint32_t> m_RefCount{ 0 };
	};

	template<typename T>
	class Ref
	{
		static_assert(std::is_base_of<RefCounted, T>::value, "Class is not RefCounted!");

	public:
		Ref()
			: m_Instance(nullptr)
		{
		}

		Ref(std::nullptr_t)
			: m_Instance(nullptr)
		{
		}

		Ref(T* instance)
			: m_Instance(instance)
		{
			IncRef();
		}

		template<typename T2>
		Ref(const Ref<T2>& other)
			: m_Instance(static_cast<T*>(other.Raw()))
		{
			IncRef();
		}

		template<typename T2>
		Ref(Ref<T2>&& other) noexcept
			: m_Instance(static_cast<T*>(other.Raw()))
		{
			other.m_Instance = nullptr;
		}

		Ref(const Ref<T>& other)
			: m_Instance(other.m_Instance)
		{
			IncRef();
		}

		Ref(Ref<T>&& other) noexcept
			: m_Instance(other.m_Instance)
		{
			other.m_Instance = nullptr;
		}

		~Ref()
		{
			DecRef();
		}

		Ref<T>& operator=(std::nullptr_t)
		{
			DecRef();
			m_Instance = nullptr;
			return *this;
		}

		Ref<T>& operator=(T* instance)
		{
			if (m_Instance != instance)
			{
				DecRef();
				m_Instance = instance;
				IncRef();
			}
			return *this;
		}

		Ref<T>& operator=(const Ref<T>& other)
		{
			if (this != &other)
			{
				if (m_Instance != other.m_Instance)
				{
					DecRef();
					m_Instance = other.m_Instance;
					IncRef();
				}
			}
			return *this;
		}

		template<typename T2>
		Ref<T>& operator=(const Ref<T2>& other)
		{
			if (m_Instance != static_cast<T*>(other.Raw()))
			{
				DecRef();
				m_Instance = static_cast<T*>(other.Raw());
				IncRef();
			}
			return *this;
		}

		Ref<T>& operator=(Ref<T>&& other) noexcept
		{
			if (this != &other)
			{
				DecRef();
				m_Instance = other.m_Instance;
				other.m_Instance = nullptr;
			}
			return *this;
		}

		template<typename T2>
		Ref<T>& operator=(Ref<T2>&& other) noexcept
		{
			DecRef();
			m_Instance = static_cast<T*>(other.Raw());
			other.m_Instance = nullptr;
			return *this;
		}

		explicit operator bool() const { return m_Instance != nullptr; }

		T& operator*() const { return *m_Instance; }
		T* operator->() const { return m_Instance; }
		T* Raw() const { return m_Instance; }
		T* get() const { return m_Instance; }

		bool Equals(const Ref<T>& other) const
		{
			return m_Instance == other.m_Instance;
		}

		template<typename T2>
		Ref<T2> As() const
		{
			return Ref<T2>(static_cast<T2*>(m_Instance));
		}

		template<typename T2>
		Ref<T2> DynamicAs() const
		{
			return Ref<T2>(dynamic_cast<T2*>(m_Instance));
		}

		template<typename... Args>
		static Ref<T> Create(Args&&... args)
		{
			return Ref<T>(new T(std::forward<Args>(args)...));
		}

		bool operator==(const Ref<T>& other) const { return m_Instance == other.m_Instance; }
		bool operator!=(const Ref<T>& other) const { return !(*this == other); }
		bool operator==(std::nullptr_t) const { return m_Instance == nullptr; }
		bool operator!=(std::nullptr_t) const { return m_Instance != nullptr; }
		bool operator==(const T* other) const { return m_Instance == other; }
		bool operator!=(const T* other) const { return m_Instance != other; }

	private:
		void IncRef() const
		{
			if (m_Instance)
			{
				m_Instance->IncRefCount();
			}
		}

		void DecRef()
		{
			if (m_Instance)
			{
				m_Instance->DecRefCount();
				if (m_Instance->GetRefCount() == 0)
				{
					delete m_Instance;
					m_Instance = nullptr;
				}
			}
		}

		template<typename T2>
		friend class Ref;

		T* m_Instance = nullptr;
	};

}
