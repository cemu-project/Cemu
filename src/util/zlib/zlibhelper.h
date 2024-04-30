#pragma once
namespace zlibhelper
{
	std::optional<std::vector<uint8>> decompress(const std::vector<uint8>& compressed);
}