#pragma once

#include <cstdint>
#include <cstring>

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

		void Allocate(uint64_t size)
		{
			Release();
			Size = size;
			Data = new uint8_t[size > 0 ? size : 1];
			Data[0] = 0;
		}

		void Release()
		{
			delete[] Data;
			Data = nullptr;
			Size = 0;
		}

		explicit operator bool() const { return Data != nullptr; }
	};

}
