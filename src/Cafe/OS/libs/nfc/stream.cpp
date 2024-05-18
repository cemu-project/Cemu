#include "stream.h"

#include <algorithm>

Stream::Stream(std::endian endianness)
 : mError(ERROR_OK), mEndianness(endianness)
{
}

Stream::~Stream()
{
}

Stream::Error Stream::GetError() const
{
	return mError;
}

void Stream::SetEndianness(std::endian endianness)
{
	mEndianness = endianness;
}

std::endian Stream::GetEndianness() const
{
	return mEndianness;
}

Stream& Stream::operator>>(bool& val)
{
	uint8 i;
	*this >> i;
	val = !!i;

	return *this;
}

Stream& Stream::operator>>(float& val)
{
	uint32 i;
	*this >> i;
	val = std::bit_cast<float>(i);

	return *this;
}

Stream& Stream::operator>>(double& val)
{
	uint64 i;
	*this >> i;
	val = std::bit_cast<double>(i);

	return *this;
}

Stream& Stream::operator<<(bool val)
{
	uint8 i = val;
	*this >> i;

	return *this;
}

Stream& Stream::operator<<(float val)
{
	uint32 i = std::bit_cast<uint32>(val);
	*this >> i;

	return *this;
}

Stream& Stream::operator<<(double val)
{
	uint64 i = std::bit_cast<uint64>(val);
	*this >> i;

	return *this;
}

void Stream::SetError(Error error)
{
	mError = error;
}

bool Stream::NeedsSwap()
{
	return mEndianness != std::endian::native;
}

VectorStream::VectorStream(std::vector<std::byte>& vector, std::endian endianness)
 : Stream(endianness), mVector(vector), mPosition(0)
{
}

VectorStream::~VectorStream()
{
}

std::size_t VectorStream::Read(const std::span<std::byte>& data)
{
	if (data.size() > GetRemaining())
	{
		SetError(ERROR_READ_FAILED);
		std::fill(data.begin(), data.end(), std::byte(0));
		return 0;
	}

	std::copy_n(mVector.get().begin() + mPosition, data.size(), data.begin());
	mPosition += data.size();
	return data.size();
}

std::size_t VectorStream::Write(const std::span<const std::byte>& data)
{
	// Resize vector if not enough bytes remain
	if (mPosition + data.size() > mVector.get().size())
	{
		mVector.get().resize(mPosition + data.size());
	}

	std::copy(data.begin(), data.end(), mVector.get().begin() + mPosition);
	mPosition += data.size();
	return data.size();
}

bool VectorStream::SetPosition(std::size_t position)
{
	if (position >= mVector.get().size())
	{
		return false;
	}

	mPosition = position;
	return true;
}

std::size_t VectorStream::GetPosition() const
{
	return mPosition;
}

std::size_t VectorStream::GetRemaining() const
{
	return mVector.get().size() - mPosition;
}

SpanStream::SpanStream(std::span<const std::byte> span, std::endian endianness)
 : Stream(endianness), mSpan(std::move(span)), mPosition(0)
{
}

SpanStream::~SpanStream()
{
}

std::size_t SpanStream::Read(const std::span<std::byte>& data)
{
	if (data.size() > GetRemaining())
	{
		SetError(ERROR_READ_FAILED);
		std::fill(data.begin(), data.end(), std::byte(0));
		return 0;
	}

	std::copy_n(mSpan.begin() + mPosition, data.size(), data.begin());
	mPosition += data.size();
	return data.size();
}

std::size_t SpanStream::Write(const std::span<const std::byte>& data)
{
	// Cannot write to const span
	SetError(ERROR_WRITE_FAILED);
	return 0;
}

bool SpanStream::SetPosition(std::size_t position)
{
	if (position >= mSpan.size())
	{
		return false;
	}

	mPosition = position;
	return true;
}

std::size_t SpanStream::GetPosition() const
{
	return mPosition;
}

std::size_t SpanStream::GetRemaining() const
{
	if (mPosition > mSpan.size())
	{
		return 0;
	}

	return mSpan.size() - mPosition;
}
