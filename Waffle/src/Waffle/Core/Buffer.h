#pragma once

#include "Waffle/Core/Assert.h"

#include <cstdint>
#include <cstring>
#include <array>
#include <vector>

namespace Waffle {

	struct Buffer
	{
		uint8_t* Data = nullptr;
		uint64_t Size = 0;

		Buffer() = default;

		Buffer(uint64_t size)
		{
			Allocate(size);
		}

		Buffer(const void* data, uint64_t size)
		{
			Allocate(size);
			if (data && size > 0)
				std::memcpy(Data, data, size);
		}

		template<typename T, size_t S>
		Buffer(const std::array<T, S>& arr)
			: Data((uint8_t*)arr.data()), Size(arr.size() * sizeof(T)) { }

		template<typename T>
		Buffer(const std::vector<T>& vec)
			: Data((uint8_t*)vec.data()), Size(vec.size() * sizeof(T)) { }

		Buffer(const Buffer& other)
		{
			Allocate(other.Size);
			if (other.Data && other.Size > 0)
				std::memcpy(Data, other.Data, other.Size);
		}

		Buffer& operator=(const Buffer& other)
		{
			if (this != &other)
			{
				Allocate(other.Size);
				if (other.Data && other.Size > 0)
					std::memcpy(Data, other.Data, other.Size);
			}
			return *this;
		}

		Buffer(Buffer&& other) noexcept
		{
			Data = other.Data;
			Size = other.Size;
			other.Data = nullptr;
			other.Size = 0;
		}

		Buffer& operator=(Buffer&& other) noexcept
		{
			if (this != &other)
			{
				Release();
				Data = other.Data;
				Size = other.Size;
				other.Data = nullptr;
				other.Size = 0;
			}
			return *this;
		}

		~Buffer()
		{
			Release();
		}

		static Buffer Copy(const Buffer& other)
		{
			Buffer buffer;
			buffer.Allocate(other.Size);
			if (other.Data && other.Size > 0)
				std::memcpy(buffer.Data, other.Data, other.Size);
			return buffer;
		}

		static Buffer Copy(const void* data, uint64_t size)
		{
			Buffer buffer;
			buffer.Allocate(size);
			if (data && size > 0)
				std::memcpy(buffer.Data, data, size);
			return buffer;
		}

		void Allocate(uint64_t size)
		{
			Release();
			Size = size;
			if (size == 0)
				return;
			Data = new uint8_t[size];
			std::memset(Data, 0, size);
		}

		void Reallocate(uint64_t size)
		{
			Release();
			Allocate(size);
		}

		void Release()
		{
			delete[] Data;
			Data = nullptr;
			Size = 0;
		}

		void ZeroInitialize()
		{
			if (Data && Size > 0)
				std::memset(Data, 0, Size);
		}

		template<typename T>
		T& Read(uint64_t offset = 0)
		{
			return *(T*)(Data + offset);
		}

		template<typename T>
		const T& Read(uint64_t offset = 0) const
		{
			return *(T*)(Data + offset);
		}

		uint8_t* ReadBytes(uint64_t size, uint64_t offset) const
		{
			WF_CORE_VERIFY(offset + size <= Size, "Buffer overflow!");
			uint8_t* buffer = new uint8_t[size];
			std::memcpy(buffer, Data + offset, size);
			return buffer;
		}

		void Write(Buffer buffer, uint64_t offset = 0)
		{
			WF_CORE_VERIFY(offset + buffer.Size <= Size, "Buffer overflow!");
			std::memcpy(Data + offset, buffer.Data, buffer.Size);
		}

		void Write(const void* data, uint64_t size, uint64_t offset = 0)
		{
			Write(Buffer((void*)data, size), offset);
		}

		explicit operator bool() const { return Data != nullptr; }

		uint8_t& operator[](int index)
		{
			return Data[index];
		}

		uint8_t operator[](int index) const
		{
			return Data[index];
		}

		template<typename T>
		T* As() const
		{
			return (T*)Data;
		}

		inline uint64_t GetSize() const { return Size; }
	};

	struct BufferSafe : public Buffer
	{
		BufferSafe() = default;

		BufferSafe(uint64_t size)
			: Buffer(size) {}

		BufferSafe(const void* data, uint64_t size)
			: Buffer(data, size) {}

		~BufferSafe()
		{
			Release();
		}

		static BufferSafe Copy(const void* data, uint64_t size)
		{
			BufferSafe buffer;
			buffer.Allocate(size);
			if (data && size > 0)
				std::memcpy(buffer.Data, data, size);
			return buffer;
		}
	};

	template<uint64_t MaxSize>
	struct StaticBuffer
	{
		uint8_t Data[MaxSize];
		uint64_t Size = 0;

		template<typename T>
		void Write(const T& data, uint64_t offset = 0)
		{
			constexpr size_t dataSize = sizeof(T);
			WF_CORE_VERIFY(offset + dataSize <= MaxSize, "Buffer overflow!");
			std::memcpy(Data + offset, &data, dataSize);

			if (Size < offset + dataSize)
				Size = offset + dataSize;
		}

		void Write(Buffer buffer, uint64_t offset = 0)
		{
			WF_CORE_VERIFY(offset + buffer.Size <= MaxSize, "Buffer overflow!");
			std::memcpy(Data + offset, buffer.Data, buffer.Size);

			if (Size < offset + buffer.Size)
				Size = offset + buffer.Size;
		}
	};

}
