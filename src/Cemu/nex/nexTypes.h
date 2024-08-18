#pragma once
#include<string>

#ifndef FFL_SIZE
#define FFL_SIZE	(0x60)
#endif

class nexPacketBuffer;

class nexMetaType
{
public:
	virtual const char* getMetaName() const = 0;
	virtual void writeData(nexPacketBuffer* pb) const = 0;
};

class nexType
{
  public:
	virtual ~nexType(){};

	virtual const char* getMetaName()
	{
		cemu_assert_unimplemented();
		return "";
	}
	virtual void writeData(nexPacketBuffer* pb) const = 0;
	virtual void readData(nexPacketBuffer* pb) = 0;
};

class nexPacketBuffer
{
public:
	nexPacketBuffer(uint8* data, sint32 size, bool isWrite)
	{
		this->buffer = data;
		this->size = size;
		this->currentIndex = 0;
		this->isWrite = isWrite;
		this->readOutOfBounds = false;
	}

	nexPacketBuffer()
	{
		this->buffer = 0;
		this->size = 0;
		this->currentIndex = 0;
	}

	void writeData(const uint8* data, sint32 len)
	{
		if (this->currentIndex + len > this->size)
			return;
		memcpy(this->buffer + this->currentIndex, data, len);
		this->currentIndex += len;
	}

	void writeU8(uint8 v)
	{
		if (this->currentIndex + sizeof(uint8) > this->size)
			return;
		*(uint8*)(this->buffer + this->currentIndex) = v;
		this->currentIndex += sizeof(uint8);
	}

	void writeU16(uint16 v)
	{
		if (this->currentIndex + sizeof(uint16) > this->size)
			return;
		*(uint16*)(this->buffer + this->currentIndex) = v;
		this->currentIndex += sizeof(uint16);
	}

	void writeU32(uint32 v)
	{
		if (this->currentIndex + sizeof(uint32) > this->size)
			return;
		*(uint32*)(this->buffer + this->currentIndex) = v;
		this->currentIndex += sizeof(uint32);
	}

	void writeU64(uint64 v)
	{
		if (this->currentIndex + sizeof(uint64) > this->size)
			return;
		*(uint64*)(this->buffer + this->currentIndex) = v;
		this->currentIndex += sizeof(uint64);
	}

	void writeString(const char* v)
	{
		sint32 len = (sint32)strlen(v) + 1;
		writeU16(len);
		writeData((uint8*)v, len);
	}

	void writeBuffer(const uint8* v, sint32 len)
	{
		writeU32(len);
		writeData(v, len);
	}

	void writeCustomType(const nexMetaType& v)
	{
		// write meta name
		writeString(v.getMetaName());
		// write length 0 placeholder
		uint32* lengthPtr0 = (uint32*)(this->buffer + this->currentIndex);
		writeU32(0);
		// write length 1 placeholder
		uint32* lengthPtr1 = (uint32*)(this->buffer + this->currentIndex);
		writeU32(0);
		// write data
		uint32 preWriteIndex = this->currentIndex;
		v.writeData(this);
		uint32 writeSize = this->currentIndex - preWriteIndex;
		// update lengths
		*lengthPtr1 = writeSize;
		*lengthPtr0 = writeSize + 4;
	}

	uint64 readU64()
	{
		if (this->currentIndex + sizeof(uint64) > this->size)
		{
			readOutOfBounds = true;
			return 0;
		}
		uint64 v = *(uint64*)(this->buffer + this->currentIndex);
		this->currentIndex += sizeof(uint64);
		return v;
	}

	uint32 readU32()
	{
		if (this->currentIndex + sizeof(uint32) > this->size)
		{
			readOutOfBounds = true;
			return 0;
		}
		uint32 v = *(uint32*)(this->buffer + this->currentIndex);
		this->currentIndex += sizeof(uint32);
		return v;
	}

	uint16 readU16()
	{
		if (this->currentIndex + sizeof(uint16) > this->size)
		{
			readOutOfBounds = true;
			return 0;
		}
		uint16 v = *(uint16*)(this->buffer + this->currentIndex);
		this->currentIndex += sizeof(uint16);
		return v;
	}

	uint8 readU8()
	{
		if (this->currentIndex + sizeof(uint8) > this->size)
		{
			readOutOfBounds = true;
			return 0;
		}
		uint8 v = *(uint8*)(this->buffer + this->currentIndex);
		this->currentIndex += sizeof(uint8);
		return v;
	}

	sint32 readData(void* buffer, sint32 length)
	{
		if (this->currentIndex + length > this->size)
		{
			readOutOfBounds = true;
			return 0;
		}
		memcpy(buffer, (this->buffer + this->currentIndex), length);
		this->currentIndex += length;
		return length;
	}

	// reads buffer data from packet and returns amount of bytes copied into buffer
	// if buffer is larger than maxLength, the read data is silently truncated
	sint32 readBuffer(void* buffer, sint32 maxLength)
	{
		sint32 bufferLength = (sint32)readU32();
		if (bufferLength < 0 || bufferLength >= 0x10000000)
		{
			readOutOfBounds = true;
			return 0;
		}
		if (this->currentIndex + bufferLength > this->size)
		{
			readOutOfBounds = true;
			return 0;
		}
		uint32 copiedLength = std::min(bufferLength, maxLength);

		memcpy(buffer, (this->buffer + this->currentIndex), copiedLength);
		this->currentIndex += bufferLength;
		return copiedLength;
	}

	sint32 readString(char* buffer, sint32 maxLength)
	{
		sint32 bufferLength = readU16();
		if (this->currentIndex + bufferLength > this->size)
		{
			readOutOfBounds = true;
			buffer[0] = '\0';
			return 0;
		}
		uint32 copiedLength = std::min(bufferLength, maxLength - 1);

		memcpy(buffer, (this->buffer + this->currentIndex), copiedLength);
		buffer[copiedLength] = '\0';
		this->currentIndex += bufferLength;
		return copiedLength;
	}

	sint32 readStdString(std::string& outputStr)
	{
		sint32 bufferLength = readU16();
		if (this->currentIndex + bufferLength > this->size)
		{
			readOutOfBounds = true;
			outputStr.clear();
			return 0;
		}
		sint32 copiedLength = bufferLength;
		if (bufferLength > 0 && this->buffer[this->currentIndex+bufferLength-1] == '\0')
		{
			// cut off trailing '\0'
			copiedLength--;
		}
		outputStr = std::string((char*)(this->buffer + this->currentIndex), copiedLength);
		this->currentIndex += bufferLength;
		return copiedLength;
	}

	bool readPlaceholderType(nexType& v)
	{
		char name[128];
		readString(name, sizeof(name));
		name[sizeof(name)-1] = '\0';
		if (hasReadOutOfBounds())
			return false;
		if (strcmp(name, v.getMetaName()) != 0)
			return false;
		// read sizes
		uint32 len0 = readU32();
		uint32 len1 = readU32();
		if (hasReadOutOfBounds())
			return false;
		if (len1 != (len0 - 4))
			return false;
		// parse type data
		v.readData(this);
		if (hasReadOutOfBounds())
			return false;
		return true;
		//// write meta name
		//writeString(v.getMetaName());
		//// write length 0 placeholder
		//uint32* lengthPtr0 = (uint32*)(this->buffer + this->currentIndex);
		//writeU32(0);
		//// write length 1 placeholder
		//uint32* lengthPtr1 = (uint32*)(this->buffer + this->currentIndex);
		//writeU32(0);
		//// write data
		//uint32 preWriteIndex = this->currentIndex;
		//v.writeData(this);
		//uint32 writeSize = this->currentIndex - preWriteIndex;
		//// update lengths
		//*lengthPtr1 = writeSize;
		//*lengthPtr0 = writeSize + 4;
	}

	bool hasReadOutOfBounds()
	{
		return readOutOfBounds;
	}

	uint8* getDataPtr()
	{
		return buffer;
	}

	sint32 getWriteIndex()
	{
		return currentIndex;
	}

	sint32 getSize()
	{
		return size;
	}

	void setSize(sint32 length)
	{
		size = length;
	}

private:
	uint8* buffer;
	sint32 size;
	sint32 currentIndex;
	bool isWrite;
	bool readOutOfBounds; // set if any read operation failed because it exceeded the buffer
};

class nexNintendoLoginData : public nexMetaType
{
public:
	nexNintendoLoginData(const char* nexToken)
	{
		this->nexToken = new std::string(nexToken);
	}

	const char* getMetaName() const override
	{
		return "NintendoLoginData";
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		pb->writeString(nexToken->c_str());
	}
private:
	std::string* nexToken;
};

class nexNotificationEventGeneral : public nexType
{
public:
	nexNotificationEventGeneral()
	{

	}

	nexNotificationEventGeneral(nexPacketBuffer* pb)
	{
		readData(pb);
	}

	const char* getMetaName() override
	{
		return "NintendoNotificationEventGeneral";
	}

	void writeData(nexPacketBuffer* pb) const override
	{
		cemu_assert_unimplemented();
	}

	void readData(nexPacketBuffer* pb) override
	{
		param1 = pb->readU32();
		param2 = pb->readU64();
		param3 = pb->readU64();
		pb->readStdString(paramStr);
	}
public:
	uint32 param1;
	uint64 param2;
	uint64 param3;
	std::string paramStr;
};
