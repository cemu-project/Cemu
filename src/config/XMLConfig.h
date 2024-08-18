#pragma once

#include "util/tinyxml2/tinyxml2.h"
#include "Common/FileStream.h"
#include "config/ConfigValue.h"

#include <string>
#include <mutex>

class XMLConfigParser
{
public:
	XMLConfigParser(tinyxml2::XMLDocument* document)
		: m_document(document), m_current_element(nullptr), m_is_root(true) {}

private:
	XMLConfigParser(tinyxml2::XMLDocument* document, tinyxml2::XMLElement* element)
		: m_document(document), m_current_element(element), m_is_root(false) {}

public:
	template <typename T>
	auto get(const char* name, T default_value = {})
	{
		if constexpr(std::is_enum_v<T>)
			return static_cast<T>(get(name, static_cast<std::underlying_type_t<T>>(default_value)));

		const auto* element = m_current_element
			? m_current_element->FirstChildElement(name)
			: m_document->FirstChildElement(name);
		
		if (element == nullptr)
			return default_value;

		if constexpr (std::is_same_v<T, bool>)
			return element->BoolText(default_value);
		else if constexpr (std::is_same_v<T, float>)
			return element->FloatText(default_value);
		else if constexpr (std::is_same_v<T, sint32>)
			return element->IntText(default_value);
		else if constexpr (std::is_same_v<T, uint32>)
			return element->UnsignedText(default_value);
		else if constexpr (std::is_same_v<T, sint64>)
			return element->Int64Text(default_value);
		else if constexpr (std::is_same_v<T, uint64>) // doesnt support real uint64...
			return (uint64)element->Int64Text((sint64)default_value);
		else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, std::string>)
		{
			const char* text = element->GetText();
			return text ? text : default_value;
		}
		
		return default_value;
	}
	
	template <typename T>
	T get(const char* name, const ConfigValue<T>& default_value)
	{
		return get(name, default_value.GetInitValue());
	}
	
	template <typename T>
	T get(const char* name, const ConfigValueBounds<T>& default_value)
	{
		return get(name, default_value.GetInitValue());
	}

	template <typename T>
	auto get_attribute(const char* name, T default_value = {})
	{
		if constexpr (std::is_enum_v<T>)
			return static_cast<T>(get_attribute(name, static_cast<std::underlying_type_t<T>>(default_value)));

		if (m_current_element == nullptr)
			return default_value;

		if constexpr (std::is_same_v<T, bool>)
			return m_current_element->BoolAttribute(name, default_value);
		else if constexpr (std::is_same_v<T, float>)
			return m_current_element->FloatAttribute(name, default_value);
		else if constexpr (std::is_same_v<T, sint32>)
			return m_current_element->IntAttribute(name, default_value);
		else if constexpr (std::is_same_v<T, uint32>)
			return m_current_element->UnsignedAttribute(name, default_value);
		else if constexpr (std::is_same_v<T, sint64>)
			return m_current_element->Int64Attribute(name, default_value);
		else if constexpr (std::is_same_v<T, uint64>) // doesnt support real uint64...
			return (uint64)m_current_element->Int64Attribute(name, (sint64)default_value);
		else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, std::string>)
		{
			const char* text = m_current_element->Attribute(name);
			return text ? text : default_value;
		}

		return default_value;
	}

	template <typename T>
	T get_attribute(const char* name, const ConfigValue<T>& default_value)
	{
		return get_attribute(name, default_value.GetInitValue());
	}

	template <typename T>
	T get_attribute(const char* name, const ConfigValueBounds<T>& default_value)
	{
		return get_attribute(name, default_value.GetInitValue());
	}

	int char2int(char input)
	{
	  if(input >= '0' && input <= '9')
	    return input - '0';
	  if(input >= 'A' && input <= 'F')
	    return input - 'A' + 10;
	  if(input >= 'a' && input <= 'f')
	    return input - 'a' + 10;
	  throw std::invalid_argument("Invalid input string");
	}

	template <typename TType, size_t TSize>
	std::array<TType, TSize>& get(const char* name, std::array<TType, TSize>& arr)
	{
		arr = {};
		const auto element = m_current_element
			                     ? m_current_element->FirstChildElement(name)
			                     : m_document->FirstChildElement(name);
		if (element == nullptr)
			return arr;

		const char* text = element->GetText();
		if(text)
		{
			assert(strlen(text) == (arr.size() * 2));
			std::istringstream iss(text);
			for(int i = 0; i < arr.size(); ++i)
			{
				arr[i] = (char2int(text[i*2]) << 4) + char2int(text[i * 2 + 1]);
			}
		}

		return arr;
	}
	
	template <typename T>
	T value(T default_value = T())
	{
		//peterBreak();
		return default_value;
	}

	bool value(bool default_value)
	{
		return m_current_element ? m_current_element->BoolText(default_value) : default_value;
	}

	float value(float default_value)
	{
		return m_current_element ? m_current_element->FloatText(default_value) : default_value;
	}

	double value(double default_value)
	{
		return m_current_element ? m_current_element->DoubleText(default_value) : default_value;
	}

	uint32 value(uint32 default_value)
	{
		return m_current_element ? m_current_element->UnsignedText(default_value) : default_value;
	}

	sint32 value(sint32 default_value)
	{
		return m_current_element ? m_current_element->IntText(default_value) : default_value;
	}

	uint64 value(uint64 default_value)
	{
		return m_current_element ? (uint64)m_current_element->Int64Text(default_value) : default_value;
	}

	sint64 value(sint64 default_value)
	{
		return m_current_element ? m_current_element->Int64Text(default_value) : default_value;
	}

	const char* value(const char* default_value)
	{
		if (m_current_element)
		{
			if (m_current_element->GetText())
				return m_current_element->GetText();
		}

		return default_value;
	}

	template <typename TType, size_t TSize>
	void set(const char* name, const std::array<TType, TSize>& value)
	{
		auto element = m_document->NewElement(name);

		std::stringstream str;
		for(const auto& v : value)
		{
			str << fmt::format("{:02x}", v);
		}

		element->SetText(str.str().c_str());

		if (m_current_element)
			m_current_element->InsertEndChild(element);
		else
			m_document->InsertEndChild(element);
	}

	template <typename T>
	void set(const char* name, T value)
	{
		auto* element = m_document->NewElement(name);

		if constexpr (std::is_enum<T>::value)
			element->SetText(fmt::format("{}", static_cast<typename std::underlying_type<T>::type>(value)).c_str());
		else
			element->SetText(fmt::format("{}", value).c_str());

		if (m_current_element)
			m_current_element->InsertEndChild(element);
		else
			m_document->InsertEndChild(element);
	}

	template <typename T>
	void set(const char* name, const std::atomic<T>& value)
	{
		set(name, value.load());
	}

	template <typename T>
	void set(const char* name, const ConfigValue<T>& value)
	{
		set(name, value.GetValue());
	}

	void set(const char* name, uint64 value)
	{
		set(name, (sint64)value);
	}

	tinyxml2::XMLElement* GetCurrentElement() const { return m_current_element; }

	XMLConfigParser get(const char* name) const
	{
		const auto element = m_current_element
			                     ? m_current_element->FirstChildElement(name)
			                     : m_document->FirstChildElement(name);
		return {m_document, element};
	}

	XMLConfigParser get(const char* name, const XMLConfigParser& parser)
	{
		const auto element = parser.m_current_element
			? parser.m_current_element->NextSiblingElement(name)
			: parser.m_document->NextSiblingElement(name);
		return { m_document, element };
	}

	XMLConfigParser set(const char* name) const
	{
		const auto element = m_document->NewElement(name);
		if (m_current_element)
			m_current_element->InsertEndChild(element);
		else
			m_document->InsertEndChild(element);

		return {m_document, element};
	}

	template <typename T>
	XMLConfigParser& set_attribute(const char* name, const T& value)
	{
		cemu_assert_debug(m_current_element != nullptr);
		if (m_current_element)
			m_current_element->SetAttribute(name, value);
		
		return *this;
	}

	template <typename T>
	XMLConfigParser& set_attribute(const char* name, const ConfigValue<T>& value)
	{
		return set_attribute(name, value.GetValue());
	}

	template <typename T>
	XMLConfigParser& set_attribute(const char* name, const ConfigValueBounds<T>& value)
	{
		return set_attribute(name, value.GetValue());
	}
	
	XMLConfigParser& set_attribute(const char* name, const std::string& value)
	{
		return set_attribute(name, value.c_str());
	}


	bool valid() const
	{
		if (m_is_root)
			return m_document != nullptr;

		return m_document != nullptr && m_current_element != nullptr;
	}

private:
	tinyxml2::XMLDocument* m_document;
	tinyxml2::XMLElement* m_current_element;
	bool m_is_root;
};

template <typename T, void(T::*L)(XMLConfigParser&) = nullptr, void(T::*S)(XMLConfigParser&) = nullptr>
class XMLConfig
{
public:
	XMLConfig() = delete;

	XMLConfig(T& instance)
		: m_instance(instance)
	{}

	XMLConfig(T& instance, std::wstring_view filename)
		: m_instance(instance), m_filename(filename) {}

	XMLConfig(const XMLConfig&) = delete;

	virtual ~XMLConfig() = default;

	bool Load()
	{
		if (L == nullptr)
			return false;

		if (m_filename.empty())
			return false;

		return Load(m_filename);
	}

	bool Load(const std::wstring& filename)
	{
		if (L == nullptr)
			return false;
		FileStream* fs = FileStream::openFile(filename.c_str());
		if (!fs)
		{
			cemuLog_logDebug(LogType::Force, "XMLConfig::Load > failed \"{}\" with error {}", boost::nowide::narrow(filename), errno);
			return false;
		}
		std::vector<uint8> xmlData;
		xmlData.resize(fs->GetSize());
		fs->readData(xmlData.data(), xmlData.size());
		delete fs;

		tinyxml2::XMLDocument doc;		
		const tinyxml2::XMLError error = doc.Parse((const char*)xmlData.data(), xmlData.size());
		const bool success = error == tinyxml2::XML_SUCCESS;
		if (error != 0)
		{
			cemuLog_logDebug(LogType::Force, "XMLConfig::Load > LoadFile {}", error);
		}
		if (success)
		{
			auto parser = XMLConfigParser(&doc);
			(m_instance.*L)(parser);
		}
		return true;
	}

	bool Save()
	{
		if (S == nullptr)
			return false;

		if (m_filename.empty())
			return false;

		return Save(m_filename);
	}

	bool Save(const std::wstring& filename)
	{
		if (S == nullptr)
			return false;

		std::wstring tmp_name = fmt::format(L"{}_{}.tmp", filename,rand() % 1000);
		std::error_code err;
		fs::create_directories(fs::path(filename).parent_path(), err);
		if (err)
		{
			cemuLog_log(LogType::Force, "can't create parent path for save file: {}", err.message());
			return false;
		}

		FILE* file = nullptr;
#if BOOST_OS_WINDOWS
        file = _wfopen(tmp_name.c_str(), L"wb");
#else
		file = fopen(boost::nowide::narrow(tmp_name).c_str(), "wb");
#endif
        if (!file)
        {
			cemuLog_logDebug(LogType::Force, "XMLConfig::Save > failed \"{}\" with error {}", boost::nowide::narrow(filename), errno);
            return false;
        }

		tinyxml2::XMLDocument doc;
		const auto declaration = doc.NewDeclaration();
		doc.InsertFirstChild(declaration);

		auto parser = XMLConfigParser(&doc);
		(m_instance.*S)(parser);

		const tinyxml2::XMLError error = doc.SaveFile(file);
		const bool success = error == tinyxml2::XML_SUCCESS;
		if(error != 0)
			cemuLog_logDebug(LogType::Force, "XMLConfig::Save > SaveFile {}", error);

		fflush(file);
		fclose(file);

		fs::rename(tmp_name, filename, err);
		if(err)
		{
			cemuLog_log(LogType::Force, "Unable to save settings to file: {}", err.message().c_str());
			fs::remove(tmp_name, err);
		}

		return success;
	}

	[[nodiscard]] const std::wstring& GetFilename() const { return m_filename; }
	void SetFilename(const std::wstring& filename) { m_filename = filename; }

	std::unique_lock<std::mutex> Lock() { return std::unique_lock(m_mutex); }

private:
	T& m_instance;
	std::wstring m_filename;
	std::mutex m_mutex;
};

template <typename T, void(T::*L)(XMLConfigParser&), void(T::*S)(XMLConfigParser&)>
class XMLDataConfig : public XMLConfig<T, L, S>
{
public:
	XMLDataConfig()
		: XMLConfig<T, L, S>::XMLConfig(m_data), m_data() {}

	XMLDataConfig(std::wstring_view filename)
		: XMLConfig<T, L, S>::XMLConfig(m_data, filename), m_data() {}

	XMLDataConfig(std::wstring_view filename, T init_data)
		: XMLConfig<T, L, S>::XMLConfig(m_data, filename), m_data(std::move(init_data)) {}

	XMLDataConfig(const XMLDataConfig& o) = delete;

	T& data() { return m_data; }

private:
	T m_data;
};
