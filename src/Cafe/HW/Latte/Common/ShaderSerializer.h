#pragma once
#include "util/helpers/Serializer.h"

namespace Latte
{
	void SerializeShaderProgram(void* shaderProg, uint32 size, MemStreamWriter& memWriter);
	bool DeserializeShaderProgram(std::vector<uint8>& progData, MemStreamReader& memReader);
};