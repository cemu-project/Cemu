#include "Account.h"
#include "util/helpers/helpers.h"
#include "util/helpers/SystemException.h"
#include "util/helpers/StringHelpers.h"
#include "config/ActiveSettings.h"
#include "Cafe/IOSU/legacy/iosu_crypto.h"
#include "Common/FileStream.h"
#include <boost/random/uniform_int.hpp>

#include <random>

std::vector<Account> Account::s_account_list;

Account::Account(uint32 persistent_id)
	: m_persistent_id(persistent_id) {}


typedef struct
{
	uint32be high;
	uint32be low;
}FFLDataID_t;

typedef struct
{
	/* +0x00 */ uint32		uknFlags;
	/* +0x04 */ FFLDataID_t miiId; // bytes 8 and 9 are part of the CRC? (miiId is based on account transferable id?)
	/* +0x0C */ uint8		ukn0C[0xA];
	/* +0x16 */ uint8		ukn16[2];
	/* +0x18 */ uint16		ukn18;
	/* +0x1A */ uint16le	miiName[10];
	/* +0x2E */ uint16		ukn2E;
	/* +0x30 */ uint8		ukn30[96 - 0x30];
}FFLData_t;

uint16 FFLCalculateCRC16(uint8* input, sint32 length)
{
	cemu_assert_debug((length % 8) == 0);
	
	uint16 crc = 0;
	for (sint32 c = 0; c < length; c++)
	{
		for (sint32 f = 0; f < 8; f++)
		{
			if ((crc & 0x8000) != 0)
			{
				uint16 t = crc << 1;
				crc = t ^ 0x1021;
			}
			else
			{
				crc <<= 1;
			}
		}
		crc ^= (uint16)input[c];
	}
	return crc;
}

Account::Account(uint32 persistent_id, std::wstring_view mii_name)
	: m_persistent_id(persistent_id)
{
	if (mii_name.empty())
		throw std::system_error(AccountErrc::InvalidMiiName);
	
	static std::random_device s_random_device;
	static std::mt19937 s_mte(s_random_device());

        // use boost library to escape static asserts in linux builds
        boost::random::uniform_int_distribution<uint16> dist(std::numeric_limits<uint8>::min(), std::numeric_limits<uint8>::max());
        
        std::generate(m_uuid.begin(), m_uuid.end(), [&]() { return (uint8)dist(s_mte); });

	// 1000004 or 2000004 | lower uint32 from uuid from uuid
	m_transferable_id_base = (0x2000004ULL << 32);
	m_transferable_id_base |= ((uint64)m_uuid[12] << 24) | ((uint64)m_uuid[13] << 16) | ((uint64)m_uuid[14] << 8) | (uint64)m_uuid[15];
	
	SetMiiName(mii_name);

	// todo: generate mii data
	// iosuAct_generateDefaultMii
	// void* pMiiData = &_actAccountData[accountIndex].miiData;
	uint8* fflByteDataBE = m_mii_data.data();

	uint16* fflDataBE = (uint16*)m_mii_data.data();
	// FFLCreateId is derived from the words at location: 0x7 - 0xb (5 words, so it's a 80 bit id)
	for (sint32 i = 0; i < 96 / 2; i++)
		fflDataBE[i] = _swapEndianU16(1);

	*(uint16*)(fflByteDataBE + 0x3A) = _swapEndianU16(1 | (3 << 9));
	*(uint16*)(fflByteDataBE + 0x2) = _swapEndianU16(1 | (1 << 12));

	//*(uint32*)(fflByteDataBE + 4) = 0; // transferable id high ?
	*(uint32*)(fflByteDataBE + 8) = _swapEndianU32(0x33333 + 0 + 1); // mii id low

	// swap endian (apparently it's stored little-endian)
	for (sint32 i = 0; i < 96 / 2; i++)
		fflDataBE[i] = _swapEndianU16(fflDataBE[i]);

	// set default name
	FFLData_t* fflData = (FFLData_t*)m_mii_data.data();
	const auto tmp_name = GetMiiName();
	memset(fflData->miiName, 0, sizeof(fflData->miiName));
	std::copy(tmp_name.begin(), tmp_name.end(), fflData->miiName);

	// calculate checksum
	uint32 crcCounter = 0;
	while (FFLCalculateCRC16(m_mii_data.data(), 96) != 0)
	{
		*(uint32*)(fflDataBE + 2) = _swapEndianU32(crcCounter);
		crcCounter++;
	}

	const auto error = CheckValid();
	if (error)
		throw std::system_error(error);
}

Account::Account(std::wstring_view file_name)
{
	if (!fs::exists(file_name.data()))
		throw std::runtime_error("given file doesn't exist");

	std::unique_ptr<FileStream> file(FileStream::openFile2(file_name));
	if (!file)
		throw std::runtime_error("can't open file");

	ParseFile(file.get());
	const auto error = CheckValid();
	if (error)
		throw std::system_error(error);
}

std::error_code Account::CheckValid() const
{
	if (m_persistent_id < kMinPersistendId)
		return AccountErrc::InvalidPersistentId;
		
	if (m_mii_name[0] == '\0')
		return AccountErrc::InvalidMiiName;
	
	if (m_mii_data == decltype(m_mii_data){})
		return AccountErrc::InvalidMiiData;

	// todo: check for other needed properties

	return AccountErrc::NoError;
}

std::error_code Account::Load()
{
	const auto persistent_id = m_persistent_id;
	const fs::path path = GetFileName();
	try
	{
		std::unique_ptr<FileStream> file(FileStream::openFile2(path));
		if (!file)
			throw std::runtime_error("can't open file");
		ParseFile(file.get());
		// fix persistent id if it's in the wrong folder
		m_persistent_id = persistent_id;
		return CheckValid();
	}
	catch (const std::system_error& ex)
	{
		return ex.code();
	}
	catch(const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "handled error in Account::Load: {}", ex.what());
		return AccountErrc::ParseError;
	}
}

std::error_code Account::Save()
{
	fs::path path = ActiveSettings::GetMlcPath(fmt::format(L"usr/save/system/act/{:08x}", m_persistent_id));
	if (!fs::exists(path))
	{
		std::error_code ec;
		fs::create_directories(path, ec);
		if (ec)
			return ec;
	}
		
	path /= "account.dat";

	try
	{
		std::ofstream file;
		file.exceptions(std::ios::badbit);
		file.open(path);
	
		file << "AccountInstance_20120705" << std::endl;
		file << fmt::format("PersistentId={:08x}", m_persistent_id) << std::endl;
		file << fmt::format("TransferableIdBase={:x}", m_transferable_id_base) << std::endl;

		file << fmt::format("Uuid=");
		for (const auto& b : m_uuid)
			file << fmt::format("{:02x}", b);
		file << std::endl;

		file << fmt::format("MiiData=");
		for (const auto& b : m_mii_data)
			file << fmt::format("{:02x}", b);
		file << std::endl;
		
		file << fmt::format("MiiName=");
		for (const auto& b : m_mii_name)
			file << fmt::format("{:04x}", (uint16)b);
		file << std::endl;
		
		file << fmt::format("AccountId={}", m_account_id) << std::endl;
		file << fmt::format("BirthYear={:x}", m_birth_year) << std::endl;
		file << fmt::format("BirthMonth={:x}", m_birth_month) << std::endl;
		file << fmt::format("BirthDay={:x}", m_birth_day) << std::endl;
		file << fmt::format("Gender={:x}", m_gender) << std::endl;
		file << fmt::format("EmailAddress={}", m_email) << std::endl;
		file << fmt::format("Country={:x}", m_country) << std::endl;
		file << fmt::format("SimpleAddressId={:x}", m_simple_address_id) << std::endl;
		file << fmt::format("PrincipalId={:x}", m_principal_id) << std::endl;
		file << fmt::format("IsPasswordCacheEnabled={:x}", m_password_cache_enabled) << std::endl;

		file << fmt::format("AccountPasswordCache=");
		for (const auto& b : m_account_password_cache)
			file << fmt::format("{:02x}", b);
		file << std::endl;
		
		// write rest of stuff we got
		for(const auto& [key, value] : m_storage)
		{
			file << fmt::format("{}={}", key, value) << std::endl;
		}
		
		file.flush();
		file.close();
		return CheckValid();
	}
	catch (const std::system_error& e)
	{
		return e.code();
	}
}

OnlineAccountError Account::GetOnlineAccountError() const
{
	if (m_account_id.empty())
		return OnlineAccountError::kNoAccountId;

	if (!IsPasswordCacheEnabled())
		return OnlineAccountError::kNoPasswordCached;

	if (m_account_password_cache == decltype(m_account_password_cache){})
		return OnlineAccountError::kPasswordCacheEmpty;

	/*if (m_simple_address_id == 0) not really needed
		return false;*/
	
	if (m_principal_id == 0)
		return OnlineAccountError::kNoPrincipalId;
	
	// TODO
	return OnlineAccountError::kNone;
}

bool Account::IsValidOnlineAccount() const
{
	return GetOnlineAccountError() == OnlineAccountError::kNone;
}

fs::path Account::GetFileName() const
{
	return GetFileName(m_persistent_id);
}

std::wstring_view Account::GetMiiName() const
{
	const auto it = std::find(m_mii_name.cbegin(), m_mii_name.cend(), '\0');
	if(it == m_mii_name.cend())
		return { m_mii_name.data(), m_mii_name.size() - 1 };

	const size_t count = std::distance(m_mii_name.cbegin(), it);
	return { m_mii_name.data(),  count}; 
}

std::string_view Account::GetStorageValue(std::string_view key) const
{
	const auto it = m_storage.find(key.data());
	if (it == m_storage.cend())
		return {};

	return it->second;
}

void Account::SetMiiName(std::wstring_view name)
{
	m_mii_name = {};
	std::copy(name.data(), name.data() + std::min(name.size(), m_mii_name.size() - 1), m_mii_name.begin());
}

const std::vector<Account>& Account::RefreshAccounts()
{
	std::vector<Account> result;
	const fs::path path = ActiveSettings::GetMlcPath("usr/save/system/act");
	if (fs::exists(path))
	{
		for (const auto& it : fs::directory_iterator(path))
		{
			if (!fs::is_directory(it))
				continue;

			const auto file_name = it.path().filename().string();
			if (file_name.size() != 8)
				continue;

			const auto persistent_id = ConvertString<uint32>(file_name, 16);
			if (persistent_id < kMinPersistendId)
				continue;

			Account account(persistent_id);
			const auto error = account.Load();
			if (!error)
				result.emplace_back(account);
		}
	}
	
	// we always force at least one account
	if (result.empty())
	{
		result.emplace_back(kMinPersistendId, L"default");
		result.begin()->Save();
	}

	s_account_list = result;
	UpdatePersisidDat();
	return s_account_list;
}

void Account::UpdatePersisidDat()
{
	const auto max_id = std::max(kMinPersistendId, GetNextPersistentId() - 1);
	const auto file = ActiveSettings::GetMlcPath("usr/save/system/act/persisid.dat");
	std::ofstream f(file);
	if(f.is_open())
	{
		f << "PersistentIdManager_20120607" << std::endl << "PersistentIdHead=" << std::hex << max_id << std::endl << std::endl;
		f.flush();
		f.close();
	}
	else
		cemuLog_log(LogType::Force, "Unable to save persisid.dat");
}

bool Account::HasFreeAccountSlots()
{
	return s_account_list.size() < 12;
}

const std::vector<Account>& Account::GetAccounts()
{
	if (!s_account_list.empty())
		return s_account_list;

	return RefreshAccounts();
}

const Account& Account::GetAccount(uint32 persistent_id)
{
	for (const auto& account : GetAccounts())
	{
		if (account.GetPersistentId() == persistent_id)
			return account;
	}

	return *GetAccounts().begin();
}

const Account& Account::GetCurrentAccount()
{
	return GetAccount(ActiveSettings::GetPersistentId());
}

uint32 Account::GetNextPersistentId()
{
	uint32 result = kMinPersistendId;
	const auto file = ActiveSettings::GetMlcPath("usr/save/system/act/persisid.dat");
	if(fs::exists(file))
	{
		std::ifstream f(file);
		if(f.is_open())
		{
			std::string line;
			while(std::getline(f, line))
			{
				if(boost::starts_with(line, "PersistentIdHead="))
				{
					result = ConvertString<uint32>(line.data() + sizeof("PersistentIdHead=") - 1, 16);
					break;
				}
			}
		}
	}
	
	// next id
	++result;
	
	const auto it = std::max_element(s_account_list.cbegin(), s_account_list.cend(), [](const Account& acc1, const Account& acc2) {return acc1.GetPersistentId() < acc2.GetPersistentId(); });
	if (it != s_account_list.cend())
		return std::max(result, it->GetPersistentId() + 1);
	else
		return result;
}

fs::path Account::GetFileName(uint32 persistent_id)
{
	if (persistent_id < kMinPersistendId)
		throw std::invalid_argument(fmt::format("persistent id {:#x} is invalid", persistent_id));
	
	return ActiveSettings::GetMlcPath(fmt::format("usr/save/system/act/{:08x}/account.dat", persistent_id));
}

OnlineValidator Account::ValidateOnlineFiles() const
{
	OnlineValidator result{};
	
	const auto otp = ActiveSettings::GetUserDataPath("otp.bin");
	if (!fs::exists(otp))
		result.otp = OnlineValidator::FileState::Missing;
	else if (fs::file_size(otp) != 1024)
		result.otp = OnlineValidator::FileState::Corrupted;
	else
		result.otp = OnlineValidator::FileState::Ok;
	
	const auto seeprom = ActiveSettings::GetUserDataPath("seeprom.bin");
	if (!fs::exists(seeprom))
		result.seeprom = OnlineValidator::FileState::Missing;
	else if (fs::file_size(seeprom) != 512)
		result.seeprom = OnlineValidator::FileState::Corrupted;
	else
		result.seeprom = OnlineValidator::FileState::Ok;

	for(const auto& v : iosuCrypt_getCertificateKeys())
	{
		const auto p = ActiveSettings::GetMlcPath(L"sys/title/0005001b/10054000/content/{}", v);
		if (!fs::exists(p) || !fs::is_regular_file(p))
			result.missing_files.emplace_back(p.generic_wstring());
	}

	for (const auto& v : iosuCrypt_getCertificateNames())
	{
		const auto p = ActiveSettings::GetMlcPath(L"sys/title/0005001b/10054000/content/{}", v);
		if (!fs::exists(p) || !fs::is_regular_file(p))
			result.missing_files.emplace_back(p.generic_wstring());
	}

	result.valid_account = IsValidOnlineAccount();
	result.account_error = GetOnlineAccountError();
	
	return result;
}

void Account::ParseFile(class FileStream* file)
{
	std::vector<uint8> buffer;
	buffer.resize(file->GetSize());
	if( file->readData(buffer.data(), buffer.size()) != buffer.size())
		throw std::system_error(AccountErrc::ParseError);
	for (const auto& s : StringHelpers::StringLineIterator(buffer))
	{
		std::string_view view = s;
		const auto find = view.find('=');
		if (find == std::string_view::npos)
			continue;

		const auto key = view.substr(0, find);
		const auto value = view.substr(find + 1);
		if (key == "PersistentId")
			m_persistent_id = ConvertString<uint32>(value, 16);
		else if (key == "TransferableIdBase")
			m_transferable_id_base = ConvertString<uint64>(value, 16);
		else if (key == "Uuid")
		{
			if (value.size() != m_uuid.size() * 2) // = 32
				throw std::system_error(AccountErrc::InvalidUuid);
			
			for (size_t i = 0; i < m_uuid.size(); ++i)
			{
				m_uuid[i] = ConvertString<uint8>(value.substr(i * 2, 2), 16);
			}
		}
		else if (key == "MiiData")
		{
			if (value.size() != m_mii_data.size() * 2) // = 192
				throw std::system_error(AccountErrc::InvalidMiiData);
			
			for (size_t i = 0; i < m_mii_data.size(); ++i)
			{
				m_mii_data[i] = ConvertString<uint8>(value.substr(i * 2, 2), 16);
			}
		}
		else if (key == "MiiName")
		{
			if(value.size() != m_mii_name.size() * 4) // = 44
				throw std::system_error(AccountErrc::InvalidMiiName);
			
			for (size_t i = 0; i < m_mii_name.size(); ++i)
			{
				m_mii_name[i] = (wchar_t)ConvertString<uint16>(value.substr(i * 4, 4), 16);
			}
		}
		else if (key == "AccountId")
			m_account_id = value;
		else if (key == "BirthYear")
			m_birth_year = ConvertString<uint16>(value, 16);
		else if (key == "BirthMonth")
			m_birth_month = ConvertString<uint8>(value, 16);
		else if (key == "BirthDay")
			m_birth_day = ConvertString<uint8>(value, 16);
		else if (key == "Gender")
			m_gender = ConvertString<uint8>(value, 16);
		else if (key == "EmailAddress")
			m_email = value;
		else if (key == "Country")
			m_country = ConvertString<uint32>(value, 16);
		else if (key == "SimpleAddressId")
			m_simple_address_id = ConvertString<uint32>(value, 16);
		else if (key == "PrincipalId")
			m_principal_id = ConvertString<uint32>(value, 16);
		else if (key == "IsPasswordCacheEnabled")
			m_password_cache_enabled = ConvertString<uint8>(value, 16);
		else if (key == "AccountPasswordCache")
		{
			for (size_t i = 0; i < m_account_password_cache.size(); ++i)
			{
				m_account_password_cache[i] = ConvertString<uint8>(value.substr(i * 2, 2), 16);
			}
		}
		else // store anything else not needed for now
			m_storage[std::string(key)] = value;
	}
}

#include"openssl/sha.h"

void makePWHash(uint8* input, sint32 length, uint32 magic, uint8* output)
{
	uint8 buffer[64 + 8];
	if (length > (sizeof(buffer) - 8))
	{
		cemu_assert_debug(false);
		memset(output, 0, 32);
		return;
	}
	buffer[4] = 0x02;
	buffer[5] = 0x65;
	buffer[6] = 0x43;
	buffer[7] = 0x46;
	buffer[0] = (magic >> 0) & 0xFF;
	buffer[1] = (magic >> 8) & 0xFF;
	buffer[2] = (magic >> 16) & 0xFF;
	buffer[3] = (magic >> 24) & 0xFF;
	memcpy(buffer + 8, input, length);
	uint8 md[SHA256_DIGEST_LENGTH];
	SHA256(buffer, 8 + length, md);
	memcpy(output, md, SHA256_DIGEST_LENGTH);
}

void actPwTest()
{
	uint8 pwHash[32];

	uint32 principalId = 0x12345678;

	uint32 pid = 0x12345678;

	makePWHash((uint8*)"pass123", 7, pid, pwHash); // calculates AccountPasswordCache

	makePWHash(pwHash, 32, pid, pwHash); // calculates AccountPasswordHash

	assert_dbg();
}
