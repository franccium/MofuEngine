#pragma once
#include "CommonHeaders.h"

namespace mofu::util {
// NOTE: intended for local use only, don't keep instances as members
class BlobStreamReader
{
public:
	DISABLE_COPY_AND_MOVE(BlobStreamReader);
	explicit BlobStreamReader(const u8* buffer) : _buffer{ buffer }, _position{ buffer } { assert(buffer); }

	template<typename T>
	[[nodiscard]] constexpr T Read()
	{
		static_assert(std::is_arithmetic_v<T>, "Template argument has to be a primitive type.");
		T value{ *((T*)_position) };
		_position += sizeof(T);
		return value;
	}

	// NOTE: the caller is responsible for allocating enough memory for the buffer
	void ReadBytes(u8* dstBuffer, u64 length)
	{
		memcpy(dstBuffer, _position, length);
		_position += length;
	}

	template<typename T>
	void ReadVector(T* vectorStart, u32 count)
	{
		static_assert(std::is_arithmetic_v<T>, "Template argument has to be a primitive type.");
		u32 size{ count * sizeof(T) };
		memcpy(vectorStart, _position, size);
		_position += size;
	}

	const char* ReadStringWithLength()
	{
		u32 length{ Read<u32>() };
		char* buffer{ (char*)_alloca(length + 1) };
		ReadBytes((u8*)buffer, length);
		buffer[length] = '\0';
		return buffer;
	}

	constexpr void Skip(u64 offset)
	{
		_position += offset;
	}

	[[nodiscard]] constexpr const u8* const BufferStart() const { return _buffer; }
	[[nodiscard]] constexpr const u8* const Position() const { return _position; }
	[[nodiscard]] constexpr const u64 Offset() const { return _position - _buffer; }

private:
	const u8* _buffer;
	const u8* _position;
};

// NOTE: intended for local use only, don't keep instances as members
class BlobStreamWriter
{
public:
	explicit BlobStreamWriter(u8* buffer, u64 bufferSize) : _buffer{ buffer }, _position{ buffer }, _bufferSize{ bufferSize }
	{
		assert(buffer && bufferSize);
	}
	DISABLE_COPY_AND_MOVE(BlobStreamWriter);

	template<typename T>
	constexpr void Write(T value)
	{
		static_assert(std::is_arithmetic_v<T>, "Template argument has to be a primitive type.");
		assert(&_position[sizeof(T)] <= &_buffer[_bufferSize]);
		*((T*)_position) = value;
		_position += sizeof(T);
	}

	void WriteChars(const char* chars, u64 length)
	{
		assert(&_position[length] <= &_buffer[_bufferSize]);
		memcpy(_position, chars, length);
		_position += length;
	}

	void WriteStringWithLength(std::string_view sv)
	{
		u32 size = static_cast<u32>(sv.size());
		Write(size);
		WriteChars(sv.data(), size);
	}

	void WriteBytes(const u8* bytes, u64 length)
	{
		assert(&_position[length] <= &_buffer[_bufferSize]);
		memcpy(_position, bytes, length);
		_position += length;
	}

	template<typename T>
	void WriteVector(const T* vectorStart, u32 count)
	{
		static_assert(std::is_arithmetic_v<T>, "Template argument has to be a primitive type.");
		u32 size{ sizeof(T) * count };
		assert(&_position[size] <= &_buffer[_bufferSize]);
		memcpy(_position, vectorStart, size);
		_position += size;
		/*for (u32 i{ 0 }; i < count; ++i)
		{
			*((T*)_position) = vectorStart[i];
			_position += sizeof(T);
		}*/
	}

	constexpr void Skip(u64 offset)
	{
		assert(&_position[offset] <= &_buffer[_bufferSize]);
		_position += offset;
	}

	constexpr void JumpTo(u8* position)
	{
		assert(position >= _buffer && position <= &_buffer[_bufferSize]);
		_position = position;
	}

	constexpr void JumpTo(u32 offset)
	{
		assert(&_buffer[offset] <= &_buffer[_bufferSize]);
		_position = (u8*)_buffer[offset];
	}

	[[nodiscard]] constexpr const u8* const BufferStart() const { return _buffer; }
	[[nodiscard]] constexpr const u8* const BufferEnd() const { return &_buffer[_bufferSize]; }
	[[nodiscard]] constexpr const u8* const Position() const { return _position; }
	[[nodiscard]] constexpr const u64 Offset() const { return _position - _buffer; }

private:
	const u8* _buffer;
	u8* _position;
	u64 _bufferSize;
};
}