package info.cemu.cemu.nativeinterface

import androidx.annotation.Keep

object NativeAccount {
    const val MAX_ACCOUNT_COUNT = 12
    const val MIN_ACCOUNT_COUNT = 1
    const val MIN_PERSISTENT_ID: UInt = 0x80000001u
    const val DEFAULT_MII_NAME = "default"

    object AccountGender {
        const val FEMALE: Byte = 0
        const val MALE: Byte = 1
    }

    @JvmStatic
    external fun createAccount(persistentId: Int, miiName: String)

    @JvmStatic
    external fun deleteAccount(persistentId: Int)

    @Keep
    data class Account(
        val persistentId: Int = 0,
        val miiName: String = DEFAULT_MII_NAME,
        val birthday: Long = 0,
        val gender: Byte = AccountGender.FEMALE,
        val email: String = "",
        val country: Int = 0,
        val isValid: Boolean = false,
    )

    @JvmStatic
    external fun getAccounts(): Array<Account>

    @JvmStatic
    external fun saveAccount(account: Account)

    @Keep
    data class AccountCountry(
        val index: Int,
        val name: String,
    )

    @JvmStatic
    external fun getAccountCountries(): Array<AccountCountry>

    object OnlineAccountError {
        const val NO_ACCOUNT_ID = 1
        const val NO_PASSWORD_CACHED = 2
        const val PASSWORD_CACHE_EMPTY = 3
        const val NO_PRINCIPAL_ID = 4
    }

    @Keep
    sealed interface OnlineValidationError

    @Keep
    class MissingOTP : OnlineValidationError

    @Keep
    class CorruptedOTP : OnlineValidationError

    @Keep
    class MissingSEEPROM : OnlineValidationError

    @Keep
    class CorruptedSEEPROM : OnlineValidationError

    @Keep
    data class MissingFile(val file: String) : OnlineValidationError

    @Keep
    data class AccountError(val accountError: Int) : OnlineValidationError

    @JvmStatic
    external fun getAccountValidationErrors(persistentId: Int): Array<OnlineValidationError>

    @JvmStatic
    external fun isOTPPresent(): Boolean

    @JvmStatic
    external fun isSEEPROMPresent(): Boolean
}
