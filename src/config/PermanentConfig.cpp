#include "PermanentConfig.h"

#include "pugixml.hpp"

#include "PermanentStorage.h"

struct xml_string_writer : pugi::xml_writer
{
	std::string result;

	void write(const void* data, size_t size) override
	{
		result.append(static_cast<const char*>(data), size);
	}
};

std::string PermanentConfig::ToXMLString() const noexcept
{
	pugi::xml_document doc;
	doc.append_child(pugi::node_declaration).append_attribute("encoding") = "UTF-8";
	auto root = doc.append_child("config");
	root.append_child("MlcPath").text().set(this->custom_mlc_path.c_str());

	xml_string_writer writer;
	doc.save(writer);
	return writer.result;
}

PermanentConfig PermanentConfig::FromXMLString(std::string_view str) noexcept
{
	PermanentConfig result{};

	pugi::xml_document doc;
	if(doc.load_buffer(str.data(), str.size()))
	{
		result.custom_mlc_path = doc.select_node("/config/MlcPath").node().text().as_string();
	}

	return result;
}

PermanentConfig PermanentConfig::Load()
{
	PermanentStorage storage;

	const auto str = storage.ReadFile(kFileName);
	if (!str.empty())
		return FromXMLString(str);

	return {};
}

bool PermanentConfig::Store() noexcept
{
	try
	{
		PermanentStorage storage;
		storage.WriteStringToFile(kFileName, ToXMLString());
	}
	catch (...)
	{
		return false;
	}
	return true;
}
