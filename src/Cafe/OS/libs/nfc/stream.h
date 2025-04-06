#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <bit>

#include "Common/precompiled.h"

class Stream
{
public:
	enum Error
	{
		ERROR_OK,
		ERROR_READ_FAILED,
		ERROR_WRITE_FAILED,
	};

public:
	Stream(std::endian endianness = std::endian::native);
	virtual ~Stream();

	Error GetError() const;

	void SetEndianness(std::endian endianness);
	std::endian GetEndianness() const;

	virtual std::size_t Read(const std::span<std::byte>& data) = 0;
	virtual std::size_t Write(const std::span<const std::byte>& data) = 0;

	virtual bool SetPosition(std::size_t position) = 0;
	virtual std::size_t GetPosition() const = 0;

	virtual std::size_t GetRemaining() const = 0;

	// Stream read operators
	template<std::integral T>
	Stream& operator>>(T& val)
	{
		val = 0;
		if (Read(std::as_writable_bytes(std::span(std::addressof(val), 1))) == sizeof(val))
		{
			if (NeedsSwap())
			{
				if (sizeof(T) == 2)
				{
					val = _swapEndianU16(val);
				}
				else if (sizeof(T) == 4)
				{
					val = _swapEndianU32(val);
				}
				else if (sizeof(T) == 8)
				{
					val = _swapEndianU64(val);
				}
			}
		}

		return *this;
	}
	Stream& operator>>(bool& val);
	Stream& operator>>(float& val);
	Stream& operator>>(double& val);

	// Stream write operators
	template<std::integral T>
	Stream& operator<<(T val)
	{
		if (NeedsSwap())
		{
			if (sizeof(T) == 2)
			{
				val = _swapEndianU16(val);
			}
			else if (sizeof(T) == 4)
			{
				val = _swapEndianU32(val);
			}
			else if (sizeof(T) == 8)
			{
				val = _swapEndianU64(val);
			}
		}

		Write(std::as_bytes(std::span(std::addressof(val), 1)));
		return *this;
	}
	Stream& operator<<(bool val);
	Stream& operator<<(float val);
	Stream& operator<<(double val);

protected:
	void SetError(Error error);

	bool NeedsSwap();

	Error mError;
	std::endian mEndianness;
};

class VectorStream : public Stream
{
public:
	VectorStream(std::vector<std::byte>& vector, std::endian endianness = std::endian::native);
	virtual ~VectorStream();

	virtual std::size_t Read(const std::span<std::byte>& data) override;
	virtual std::size_t Write(const std::span<const std::byte>& data) override;

	virtual bool SetPosition(std::size_t position) override;
	virtual std::size_t GetPosition() const override;

	virtual std::size_t GetRemaining() const override;

private:
	std::reference_wrapper<std::vector<std::byte>> mVector;
	std::size_t mPosition;
};

class SpanStream : public Stream
{
public:
	SpanStream(std::span<const std::byte> span, std::endian endianness = std::endian::native);
	virtual ~SpanStream();

	virtual std::size_t Read(const std::span<std::byte>& data) override;
	virtual std::size_t Write(const std::span<const std::byte>& data) override;

	virtual bool SetPosition(std::size_t position) override;
	virtual std::size_t GetPosition() const override;

	virtual std::size_t GetRemaining() const override;

private:
	std::span<const std::byte> mSpan;
	std::size_t mPosition;
};
