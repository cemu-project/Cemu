#include "util/helpers/SystemException.h"
#include "JNIUtils.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cemu/ncrypto/ncrypto.h"
#include "Cafe/Account/Account.h"

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeAccount_createAccount(JNIEnv* env, [[maybe_unused]] jclass clazz, jint persistentId, jstring miiName)
{
	Account account(persistentId, boost::nowide::widen(JNIUtils::FromJString(env, miiName)));
	account.Save();
	Account::RefreshAccounts();
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeAccount_deleteAccount([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint persistentId)
{
	const auto& account = Account::GetAccount(persistentId);

	if (account.GetPersistentId() != persistentId)
	{
		return;
	}

	const fs::path path = account.GetFileName();

	try
	{
		fs::remove_all(path.parent_path());
		Account::RefreshAccounts();
	} catch (const std::exception& ex)
	{
		cemuLog_log(LogType::Force, "Failed to delete account {} {}", path.c_str(), ex.what());
	}
}

extern "C" [[maybe_unused]] JNIEXPORT void JNICALL
Java_info_cemu_cemu_nativeinterface_NativeAccount_saveAccount(JNIEnv* env, [[maybe_unused]] jclass clazz, jobject accountJava)
{
	using namespace std::chrono;

	jclass accountClass = env->FindClass("info/cemu/cemu/nativeinterface/NativeAccount$Account");
	auto getAccountField = [&](const char* fieldName, const char* sig, auto getFieldFn) -> auto {
		auto getField = std::bind(getFieldFn, env, accountJava, std::placeholders::_1);
		return getField(env->GetFieldID(accountClass, fieldName, sig));
	};
	uint32 persistentId = getAccountField("persistentId", "I", &JNIEnv::GetIntField);
	auto account = Account::GetAccount(persistentId);

	if (account.GetPersistentId() != persistentId)
	{
		return;
	}

	jstring miiNameJava = static_cast<jstring>(getAccountField("miiName", "Ljava/lang/String;", &JNIEnv::GetObjectField));
	account.SetMiiName(boost::nowide::widen(JNIUtils::FromJString(env, miiNameJava)));
	account.SetCountry(getAccountField("country", "I", &JNIEnv::GetIntField));
	account.SetGender(getAccountField("gender", "B", &JNIEnv::GetByteField));
	jstring emailJava = static_cast<jstring>(getAccountField("email", "Ljava/lang/String;", &JNIEnv::GetObjectField));
	account.SetEmail(JNIUtils::FromJString(env, emailJava));
	auto birthdayMillis = milliseconds(getAccountField("birthday", "J", &JNIEnv::GetLongField));
	system_clock::time_point birthdayTimePoint(birthdayMillis);
	year_month_day birthdayYMD(floor<days>(time_point(birthdayTimePoint)));
	if (birthdayYMD.ok())
	{
		account.SetBirthYear(static_cast<sint32>(birthdayYMD.year()));
		account.SetBirthMonth(static_cast<uint32>(birthdayYMD.month()));
		account.SetBirthDay(static_cast<uint32>(birthdayYMD.day()));
	}
	account.Save();

	Account::RefreshAccounts();
}

jlong toUnixTimestampMillis(std::chrono::year_month_day ymd)
{
	using namespace std::chrono;
	if (!ymd.ok())
		return 0;

	auto millis = duration_cast<milliseconds>(system_clock::time_point(sys_days(ymd)).time_since_epoch());
	return millis.count();
}

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeAccount_getAccounts(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	using namespace std::chrono;
	jclass accountClass = env->FindClass("info/cemu/cemu/nativeinterface/NativeAccount$Account");
	jmethodID accountCtrId = env->GetMethodID(accountClass, "<init>", "(ILjava/lang/String;JBLjava/lang/String;IZ)V");

	const auto& accounts = Account::GetAccounts();
	jsize accountsCount = static_cast<jsize>(accounts.size());
	auto accountsJArray = env->NewObjectArray(accountsCount, accountClass, nullptr);

	for (jint i = 0; i < accounts.size(); i++)
	{
		const auto& account = accounts[i];
		jint persistentId = static_cast<jint>(account.GetPersistentId());
		jstring miiName = JNIUtils::ToJString(env, account.GetMiiName());
		jlong birthday = toUnixTimestampMillis(year_month_day(year(account.GetBirthYear()), month(account.GetBirthMonth()), day(account.GetBirthDay())));
		jbyte gender = static_cast<jbyte>(account.GetGender());
		jstring email = JNIUtils::ToJString(env, account.GetEmail());
		jint country = static_cast<jint>(account.GetCountry());
		jboolean isValid = account.IsValidOnlineAccount();

		jobject accountJObj = env->NewObject(
			accountClass,
			accountCtrId,
			persistentId,
			miiName,
			birthday,
			gender,
			email,
			country,
			isValid);
		env->SetObjectArrayElement(accountsJArray, i, accountJObj);
	}

	return accountsJArray;
}

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeAccount_getAccountCountries(JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	jclass countryClass = env->FindClass("info/cemu/cemu/nativeinterface/NativeAccount$AccountCountry");
	jmethodID countryCtrId = env->GetMethodID(countryClass, "<init>", "(ILjava/lang/String;)V");

	struct Country
	{
		jint index;
		const char* name;
	};

	std::vector<Country> countries;
	for (int i = 0; i < NCrypto::GetCountryCount(); ++i)
	{
		const auto countryName = NCrypto::GetCountryAsString(i);
		if (countryName && (i == 0 || !boost::equals(countryName, "NN")))
		{
			countries.push_back({.index = i, .name = countryName});
		}
	}

	jobjectArray countriesJava = env->NewObjectArray(countries.size(), countryClass, nullptr);
	for (int i = 0; i < countries.size(); ++i)
	{
		const auto& country = countries[i];
		jobject countryJava = env->NewObject(countryClass,
											 countryCtrId,
											 country.index,
											 env->NewStringUTF(country.name));
		env->SetObjectArrayElement(countriesJava, i, countryJava);
	}

	return countriesJava;
}

extern "C" [[maybe_unused]] JNIEXPORT jobjectArray JNICALL
Java_info_cemu_cemu_nativeinterface_NativeAccount_getAccountValidationErrors(JNIEnv* env, [[maybe_unused]] jclass clazz, jint persistentId)
{
	using ErrorType = std::pair<jclass, jmethodID>;
	auto getErrorType = [&](const char* className, const char* ctrSig = "()V") -> ErrorType {
		using namespace std::placeholders;
		jclass errorClass = env->FindClass(className);
		jmethodID ctrMID = env->GetMethodID(errorClass, "<init>", ctrSig);
		return std::make_pair(errorClass, ctrMID);
	};
	auto newError = [&](const ErrorType& error, auto... args) {
		return env->NewObject(error.first, error.second, args...);
	};
	auto missingOTPError = getErrorType("info/cemu/cemu/nativeinterface/NativeAccount$MissingOTP");
	auto corruptedOTPError = getErrorType("info/cemu/cemu/nativeinterface/NativeAccount$CorruptedOTP");
	auto missingSEEPROMError = getErrorType("info/cemu/cemu/nativeinterface/NativeAccount$MissingSEEPROM");
	auto corruptedSEEPROMError = getErrorType("info/cemu/cemu/nativeinterface/NativeAccount$CorruptedSEEPROM");
	auto missingFileError = getErrorType("info/cemu/cemu/nativeinterface/NativeAccount$MissingFile", "(Ljava/lang/String;)V");
	auto accountError = getErrorType("info/cemu/cemu/nativeinterface/NativeAccount$AccountError", "(I)V");
	auto baseErrorType = env->FindClass("info/cemu/cemu/nativeinterface/NativeAccount$OnlineValidationError");

	auto account = Account::GetAccount(persistentId);
	const auto validator = account.ValidateOnlineFiles();

	if (account.GetPersistentId() != persistentId || validator.IsValid())
	{
		return env->NewObjectArray(0, baseErrorType, nullptr);
	}

	std::vector<jobject> errors;

	if (validator.otp == OnlineValidator::FileState::Missing)
		errors.push_back(newError(missingOTPError));
	else if (validator.otp == OnlineValidator::FileState::Corrupted)
		errors.push_back(newError(corruptedOTPError));

	if (validator.seeprom == OnlineValidator::FileState::Missing)
		errors.push_back(newError(missingSEEPROMError));
	else if (validator.seeprom == OnlineValidator::FileState::Corrupted)
		errors.push_back(newError(corruptedSEEPROMError));

	if (!validator.missing_files.empty())
	{
		int counter = 0;
		for (const auto& missingFile : validator.missing_files)
		{
			errors.push_back(newError(missingFileError, JNIUtils::ToJString(env, missingFile)));

			++counter;
			if (counter > 10)
			{
				break;
			}
		}
	}

	if (!validator.valid_account && validator.account_error != OnlineAccountError::kNone)
	{
		errors.push_back(newError(accountError, validator.account_error));
	}

	jobjectArray errorsJava = env->NewObjectArray(errors.size(), baseErrorType, nullptr);

	for (int i = 0; i < errors.size(); i++)
	{
		env->SetObjectArrayElement(errorsJava, i, errors[i]);
	}

	return errorsJava;
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeAccount_isOTPPresent([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return NCrypto::OTP_IsPresent();
}

extern "C" [[maybe_unused]] JNIEXPORT jboolean JNICALL
Java_info_cemu_cemu_nativeinterface_NativeAccount_isSEEPROMPresent([[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz)
{
	return NCrypto::SEEPROM_IsPresent();
}
