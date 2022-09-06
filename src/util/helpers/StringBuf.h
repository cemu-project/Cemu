#pragma once

class StringBuf
{
public:
	StringBuf(uint32 bufferSize)
	{
		this->str = (uint8*)malloc(bufferSize + 4);
		this->allocated = true;
		this->length = 0;
		this->limit = bufferSize;
	}

	~StringBuf()
	{
		if (this->allocated)
			free(this->str);
	}

	template<typename TFmt, typename ... TArgs>
	void addFmt(const TFmt& format, TArgs&&... args)
	{
		auto r = fmt::vformat_to_n((char*)(this->str + this->length), (size_t)(this->limit - this->length), fmt::detail::to_string_view(format), fmt::make_format_args(args...));
		this->length += (uint32)r.size;
	}

	void add(const char* appendedStr)
	{
		const char* outputStart = (char*)(this->str + this->length);
		char* output = (char*)outputStart;
		const char* outputEnd = (char*)(this->str + this->limit - 1);
		while (output < outputEnd)
		{
			char c = *appendedStr;
			if (c == '\0')
				break;
			*output = c;
			appendedStr++;
			output++;
		}
		this->length += (uint32)(output - outputStart);
		*output = '\0';
	}

	void add(std::string_view appendedStr)
	{
		size_t remainingLen = this->limit - this->length;
		size_t copyLen = appendedStr.size();
		if (remainingLen > copyLen)
			copyLen = remainingLen;
		char* outputStart = (char*)(this->str + this->length);
		std::copy(appendedStr.data(), appendedStr.data() + copyLen, outputStart);
		outputStart[copyLen] = '\0';
	}

	void reset()
	{
		length = 0;
	}

	uint32 getLen() const
	{
		return length;
	}

	const char* c_str() const
	{
		str[length] = '\0';
		return (const char*)str;
	}

	void shrink_to_fit()
	{
		if (!this->allocated)
			return;
		uint32 newLimit = this->length;
		this->str = (uint8*)realloc(this->str, newLimit + 4);
		this->limit = newLimit;
	}

private:
	uint8*	str;
	uint32	length; /* in bytes */
	uint32	limit; /* in bytes */
	bool	allocated;
};