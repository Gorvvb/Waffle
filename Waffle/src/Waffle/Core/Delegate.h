#pragma once

#include "Waffle/Core/Assert.h"
#include "Waffle/Core/Buffer.h"

#include <list>
#include <map>
#include <memory>
#include <ranges>
#include <functional>
#include <type_traits>
#include <cstring>
#include <cstdint>

namespace Waffle {

	template <class T>
	class Delegate;

	template <class T>
	class MulticastDelegate;

	template<class TReturn, class... TArgs>
	class Delegate<TReturn(TArgs...)>
	{
		friend class MulticastDelegate<TReturn(TArgs...)>;

		using TInstancePtr = void*;
		using TInternalFunction = TReturn(*)(TInstancePtr, TArgs&&...);

		struct InvocationElement
		{
			InvocationElement() = default;
			InvocationElement(TInstancePtr thisPtr, TInternalFunction aStub) : Object(thisPtr), Stub(aStub) {}

			bool operator==(const InvocationElement& another) const { return another.Stub == Stub && another.Object == Object; }
			bool operator!=(const InvocationElement& another) const { return another.Stub != Stub || another.Object != Object; }

			TInstancePtr Object = nullptr;
			TInternalFunction Stub = nullptr;
		};

	public:
		Delegate() = default;
		Delegate(const Delegate& other) { m_Invocation = other.m_Invocation; m_Storage = other.m_Storage; }
		Delegate& operator=(const Delegate& other) { m_Invocation = other.m_Invocation; m_Storage = other.m_Storage; return *this; }
		bool operator==(const Delegate& other) const { return m_Invocation == other.m_Invocation; }
		bool operator!=(const Delegate& other) const { return m_Invocation != other.m_Invocation; }

		template<TReturn(*TFunction)(TArgs...)>
		void Bind()
		{
			m_Storage.reset();
			Assign(nullptr, FreeFunctionStub<TFunction>);
		}

		template<class TLambda>
		void BindLambda(TLambda&& lambda)
		{
			using TLambdaNoRef = typename std::remove_cvref_t<TLambda>;
			m_Storage.reset(new uint8_t[sizeof(TLambdaNoRef)]);
			auto* storage = std::launder(reinterpret_cast<TLambdaNoRef*>(m_Storage.get()));
			std::memcpy(storage, std::addressof(lambda), sizeof(TLambdaNoRef));
			Assign((TInstancePtr)(storage), LambdaStub<TLambdaNoRef>);
		}

		template<auto TFunction, class TClass>
		void Bind(TClass* object)
		{
			m_Storage.reset();

			using TMembFunc = TReturn(TClass::*)(TArgs...);
			using TMembFuncConst = TReturn(TClass::*)(TArgs...) const;

			if constexpr (std::is_same_v<decltype(TFunction), TMembFuncConst>)
			{
				Assign(const_cast<TClass*>(object), ConstMemberFunctionStub<TClass, TFunction>);
			}
			else
			{
				static_assert(std::is_same_v<decltype(TFunction), TMembFunc>, "Invalid function signature.");
				Assign((TInstancePtr)(object), MemberFunctionStub<TClass, TFunction>);
			}
		}

		void Unbind()
		{
			m_Invocation = InvocationElement();
			m_Storage.reset();
		}

		bool IsBound() const { return m_Invocation.Stub != nullptr; }
		operator bool() const { return IsBound(); }

		inline TReturn Invoke(TArgs... args) const
		{
			WF_CORE_ASSERT(IsBound(), "Trying to invoke unbound delegate.");
			return std::invoke(m_Invocation.Stub, m_Invocation.Object, std::forward<TArgs>(args)...);
		}

		inline TReturn operator()(TArgs... args) const
		{
			return Invoke(std::forward<TArgs>(args)...);
		}

	private:
		inline void Assign(TInstancePtr anObject, TInternalFunction aStub)
		{
			m_Invocation.Object = anObject;
			m_Invocation.Stub = aStub;
		}

		template <class TClass, TReturn(TClass::* TFunction)(TArgs...)>
		static TReturn MemberFunctionStub(TInstancePtr thisPtr, TArgs&&... args)
		{
			TClass* p = static_cast<TClass*>(thisPtr);
			return (p->*TFunction)(std::forward<TArgs>(args)...);
		}

		template <class TClass, TReturn(TClass::* TFunction)(TArgs...) const>
		static TReturn ConstMemberFunctionStub(TInstancePtr thisPtr, TArgs&&... args)
		{
			const TClass* p = static_cast<TClass*>(thisPtr);
			return (p->*TFunction)(std::forward<TArgs>(args)...);
		}

		template<TReturn(*TFunction)(TArgs...)>
		static TReturn FreeFunctionStub(TInstancePtr thisPtr, TArgs&&... args)
		{
			return (TFunction)(std::forward<TArgs>(args)...);
		}

		template <typename TLambda>
		static TReturn LambdaStub(TInstancePtr thisPtr, TArgs&&... args)
		{
			TLambda* p = static_cast<TLambda*>(thisPtr);
			return (p->operator())(std::forward<TArgs>(args)...);
		}

	private:
		InvocationElement m_Invocation;
		std::shared_ptr<uint8_t[]> m_Storage;
	};

	template<class TReturn, class... TArgs>
	class MulticastDelegate<TReturn(TArgs...)>
	{
		using TDelegate = Delegate<TReturn(TArgs...)>;
		using TInstancePtr = typename TDelegate::TInstancePtr;
		using TInternalFunction = typename TDelegate::TInternalFunction;
		using InvocationElement = typename TDelegate::InvocationElement;
	public:
		MulticastDelegate() = default;

		MulticastDelegate(const MulticastDelegate& other) { m_InvocationList = other.m_InvocationList; m_Storage = other.m_Storage; }
		MulticastDelegate& operator=(const MulticastDelegate& other) { m_InvocationList = other.m_InvocationList; m_Storage = other.m_Storage; return *this; }
		bool operator==(const MulticastDelegate& other) const { return m_InvocationList == other.m_InvocationList; }
		bool operator!=(const MulticastDelegate& other) const { return m_InvocationList != other.m_InvocationList; }

		void operator+=(const MulticastDelegate& other)
		{
			m_InvocationList.insert(m_InvocationList.end(), other.m_InvocationList.begin(), other.m_InvocationList.end());
			m_Storage.insert(other.m_Storage.begin(), other.m_Storage.end());
		}

		template<TReturn(*TFunction)(TArgs...)>
		void Bind()
		{
			Add(nullptr, FreeFunctionStub<TFunction>);
		}

		template<class TLambda>
		void BindLambda(TLambda&& lambda)
		{
			if (m_Storage.contains((std::uintptr_t)std::addressof(lambda)))
				return;

			using TLambdaNoRef = typename std::remove_cvref_t<TLambda>;
			auto& data = m_Storage[(std::uintptr_t)std::addressof(lambda)];
			data.reset(new uint8_t[sizeof(TLambdaNoRef)]);

			auto* storage = std::launder(reinterpret_cast<TLambdaNoRef*>(data.get()));
			std::memcpy(storage, std::addressof(lambda), sizeof(TLambdaNoRef));

			Add((typename TDelegate::TInstancePtr)(storage), LambdaStub<TLambdaNoRef>);
		}

		template<auto TFunction, class TClass>
		void Bind(TClass* object)
		{
			using TMembFunc = TReturn(TClass::*)(TArgs...);
			using TMembFuncConst = TReturn(TClass::*)(TArgs...) const;

			if constexpr (std::is_same_v<decltype(TFunction), TMembFuncConst>)
			{
				Add(const_cast<TClass*>(object), ConstMemberFunctionStub<TClass, TFunction>);
			}
			else
			{
				static_assert(std::is_same_v<decltype(TFunction), TMembFunc>, "Invalid function signature.");
				Add((TInstancePtr)(object), MemberFunctionStub<TClass, TFunction>);
			}
		}

		template<TReturn(*TFunction)(TArgs...)>
		void Unbind()
		{
			Remove(nullptr, FreeFunctionStub<TFunction>);
		}

		template<class TLambda>
		void Unbind(const TLambda& lambda)
		{
			Remove((TDelegate::TInstancePtr)(&lambda), LambdaStub<TLambda>);
			m_Storage.erase((std::uintptr_t)std::addressof(lambda));
		}

		template<class TClass, TReturn(TClass::* TFunction)(TArgs...)>
		void Unbind(TClass* object)
		{
			Remove((TDelegate::TInstancePtr)(object), MemberFunctionStub<TClass, TFunction>);
		}

		template <class TClass, TReturn(TClass::* TFunction)(TArgs...) const>
		void Unbind(const TClass* object)
		{
			Remove(const_cast<TClass*>(object), ConstMemberFunctionStub<TClass, TFunction>);
		}

		bool IsBound() const { return !m_InvocationList.empty(); }
		operator bool() const { return IsBound(); }

		void Broadcast(TArgs... args) const
		{
			WF_CORE_ASSERT(IsBound(), "Trying to invoke unbound delegate.");
			const size_t numberOfInvocations = m_InvocationList.size();
			for (const auto& element : m_InvocationList | std::views::take(numberOfInvocations))
			{
				(*(element.Stub))(element.Object, std::forward<TArgs>(args)...);
			}
		}

		inline void operator()(TArgs... args) const
		{
			Broadcast(std::forward<TArgs>(args)...);
		}

	private:
		inline void Add(TInstancePtr anObject, TInternalFunction aStub)
		{
			m_InvocationList.push_back(InvocationElement{ anObject, aStub });
		}

		inline void Remove(TInstancePtr anObject, TInternalFunction aStub)
		{
			m_InvocationList.remove(InvocationElement{ anObject, aStub });
		}

		template <class TClass, TReturn(TClass::* TFunction)(TArgs...)>
		static TReturn MemberFunctionStub(TInstancePtr thisPtr, TArgs&&... args)
		{
			TClass* p = static_cast<TClass*>(thisPtr);
			return (p->*TFunction)(std::forward<TArgs>(args)...);
		}

		template <class TClass, TReturn(TClass::* TFunction)(TArgs...) const>
		static TReturn ConstMemberFunctionStub(TInstancePtr thisPtr, TArgs&&... args)
		{
			const TClass* p = static_cast<TClass*>(thisPtr);
			return (p->*TFunction)(std::forward<TArgs>(args)...);
		}

		template<TReturn(*TFunction)(TArgs...)>
		static TReturn FreeFunctionStub(TInstancePtr thisPtr, TArgs&&... args)
		{
			return (TFunction)(std::forward<TArgs>(args)...);
		}

		template <typename TLambda>
		static TReturn LambdaStub(TInstancePtr thisPtr, TArgs&&... args)
		{
			TLambda* p = static_cast<TLambda*>(thisPtr);
			return (p->operator())(std::forward<TArgs>(args)...);
		}

	private:
		std::list<InvocationElement> m_InvocationList;
		std::map<std::uintptr_t, std::shared_ptr<uint8_t[]>> m_Storage;
	};

}
