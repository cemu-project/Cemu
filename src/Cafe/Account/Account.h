#pragma once

#include "AccountError.h"

#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <optional>

enum class OnlineAccountError
{
	kNone,
	kNoAccountId,
	kNoPasswordCached,
	kPasswordCacheEmpty,
	kNoPrincipalId,
};

struct OnlineValidator
{
	enum class FileState
	{
		Missing,
		Corrupted,
		Ok,
	};

	bool valid_account = false;
	FileState otp = FileState::Missing;
	FileState seeprom = FileState::Missing;
	std::vector<std::wstring> missing_files;
	OnlineAccountError account_error = OnlineAccountError::kNone;

	bool IsValid() const
	{
		return valid_account && otp == FileState::Ok && seeprom == FileState::Ok && missing_files.empty();
	}

	explicit operator bool() const
	{
		return IsValid();
	}
};

class Account
{
public:
	static constexpr uint32 kMinPersistendId = 0x80000001;
	
	// create dummy account object from scratch
	Account(uint32 persistent_id, std::wstring_view mii_name);

	// load an existing account
	Account(std::wstring_view file_name);

	std::error_code Load();
	std::error_code Save();

	[[nodiscard]] std::wstring ToString() const { return fmt::format(L"{} ({:x})", GetMiiName(), GetPersistentId()); }

	// test if the account file has all fields set required for online play
	[[nodiscard]] bool IsValidOnlineAccount() const;
	[[nodiscard]] OnlineAccountError GetOnlineAccountError() const;
	[[nodiscard]] fs::path GetFileName() const;

	[[nodiscard]] uint32 GetPersistentId() const { return m_persistent_id; }
	[[nodiscard]] uint64 GetTransferableIdBase() const { return m_transferable_id_base; }
	[[nodiscard]] const std::array<uint8, 16>& GetUuid() const { return m_uuid; }
	[[nodiscard]] const std::array<uint8, 96>& GetMiiData() const { return m_mii_data; }
	[[nodiscard]] std::wstring_view GetMiiName() const; // only max 10 characters excluding '\0'
	[[nodiscard]] std::string_view GetAccountId() const { return m_account_id; }
	[[nodiscard]] uint16 GetBirthYear() const { return m_birth_year; }
	[[nodiscard]] uint8 GetBirthMonth() const { return m_birth_month; }
	[[nodiscard]] uint8 GetBirthDay() const { return m_birth_day; }
	[[nodiscard]] uint8 GetGender() const { return m_gender; }
	[[nodiscard]] std::string_view GetEmail() const { return m_email; }
	[[nodiscard]] uint32 GetCountry() const { return m_country; }
	[[nodiscard]] uint32 GetSimpleAddressId() const { return m_simple_address_id; }
	[[nodiscard]] uint32 GetPrincipalId() const { return m_principal_id; }
	[[nodiscard]] bool IsPasswordCacheEnabled() const { return m_password_cache_enabled != 0; }
	[[nodiscard]] const std::array<uint8, 32>& GetAccountPasswordCache() const { return m_account_password_cache; }

	[[nodiscard]] std::string_view GetStorageValue(std::string_view key) const;

	void SetMiiName(std::wstring_view name);
	void SetBirthYear(uint16 birth_year) { m_birth_year = birth_year; }
	void SetBirthMonth(uint8 birth_month) { m_birth_month = birth_month; }
	void SetBirthDay(uint8 birth_day) { m_birth_day = birth_day; }
	void SetGender(uint8 gender) { m_gender = gender; }
	void SetEmail(std::string_view email) { m_email = email; }
	void SetCountry(uint32 country) { m_country = country; }

	// this will always return at least one account (default one)
	static const std::vector<Account>& RefreshAccounts();
	static void UpdatePersisidDat();
	
	[[nodiscard]] static bool HasFreeAccountSlots();
	[[nodiscard]] static const std::vector<Account>& GetAccounts();
	[[nodiscard]] static const Account& GetAccount(uint32 persistent_id);
	[[nodiscard]] static const Account& GetCurrentAccount();
	[[nodiscard]] static uint32 GetNextPersistentId();
	[[nodiscard]] static fs::path GetFileName(uint32 persistent_id);
	[[nodiscard]] OnlineValidator ValidateOnlineFiles() const;
private:
	Account(uint32 persistent_id);

	[[nodiscard]] std::error_code CheckValid() const;
	void ParseFile(class FileStream* file);

	uint32 m_persistent_id = 0;
	uint64 m_transferable_id_base = 0;
	std::array<uint8, 16> m_uuid {};
	std::array<uint8, 96> m_mii_data{};
	std::array<wchar_t, 11> m_mii_name{};
	std::string m_account_id;

	uint16 m_birth_year = 0;
	uint8 m_birth_month = 0;
	uint8 m_birth_day = 0;
	uint8 m_gender = 0;

	std::string m_email;
	uint32 m_country = 0;
	uint32 m_simple_address_id = 0;
	uint32 m_principal_id = 0;
	uint8 m_password_cache_enabled = 0;
	std::array<uint8, 32> m_account_password_cache{};

	// misc storage for unused local properties
	std::unordered_map<std::string, std::string> m_storage;

	static std::vector<Account> s_account_list;
};
