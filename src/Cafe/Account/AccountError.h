#pragma once

#include <system_error>

enum class AccountErrc
{
	NoError = 0,
	ParseError,
	InvalidPersistentId,
	InvalidUuid,
	InvalidMiiName,
	InvalidMiiData,
};
namespace std
{
	template <> struct is_error_code_enum<AccountErrc> : true_type {};
}
namespace detail
{
	// Define a custom error code category derived from std::error_category
	class AccountErrc_category : public std::error_category
	{
	public:
		// Return a short descriptive name for the category
		[[nodiscard]] const char* name() const noexcept override final { return "AccountError"; }

		// Return what each enum means in text
		[[nodiscard]] std::string message(int c) const override final
		{
			switch (static_cast<AccountErrc>(c))
			{
			case AccountErrc::NoError:
				return "no error";
			case AccountErrc::InvalidPersistentId:
				return "invalid PersistentId";
			case AccountErrc::InvalidMiiName:
				return "invalid MiiName";
			default:
				return "unknown error";
			}
		}

		[[nodiscard]] std::error_condition default_error_condition(int c) const noexcept override final
		{
			switch (static_cast<AccountErrc>(c))
			{
			case AccountErrc::InvalidPersistentId:
				return make_error_condition(std::errc::invalid_argument);
			default:
				return std::error_condition(c, *this);
			}
		}
	};
}

inline std::error_code make_error_code(AccountErrc e)
{
	static detail::AccountErrc_category c;
	return { static_cast<int>(e), c };
}