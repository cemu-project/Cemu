package info.cemu.cemu.settings.account

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import info.cemu.cemu.nativeinterface.NativeAccount
import info.cemu.cemu.nativeinterface.NativeAccount.MAX_ACCOUNT_COUNT
import info.cemu.cemu.nativeinterface.NativeAccount.MIN_ACCOUNT_COUNT
import info.cemu.cemu.nativeinterface.NativeActiveSettings
import info.cemu.cemu.nativeinterface.NativeSettings
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn

data class CreateAccount(
    val persistentId: Int?,
    val miiName: String,
)

data class ActiveAccountData(
    val account: NativeAccount.Account,
    val networkService: Int,
)

data class OnlineFilesStatus(
    val hasRequiredOnlineFiles: Boolean,
    val isOTPPresent: Boolean,
    val isSEEPREOMPresent: Boolean,
)

sealed class CreateAccountError {
    object InvalidPersistentId : CreateAccountError()
    object EmptyPersistentId : CreateAccountError()
    data class ConflictingPersistentId(
        val existingPersistentId: Int,
        val existingMiiName: String,
    ) : CreateAccountError()

    object EmptyMiiName : CreateAccountError()
}

class AccountsViewModel : ViewModel() {
    private val _accounts = MutableStateFlow(NativeAccount.getAccounts().toList())
    val accounts = _accounts.asStateFlow()

    private val activeAccountPersistentId =
        MutableStateFlow(NativeSettings.getAccountPersistentId())

    private val activeAccountNetworkService =
        MutableStateFlow(NativeSettings.getAccountNetworkService(activeAccountPersistentId.value))

    val onlineFilesStatus = OnlineFilesStatus(
        hasRequiredOnlineFiles = NativeActiveSettings.hasRequiredOnlineFiles(),
        isOTPPresent = NativeAccount.isOTPPresent(),
        isSEEPREOMPresent = NativeAccount.isSEEPROMPresent(),
    )

    val activeAccountData =
        combine(
            accounts,
            activeAccountPersistentId,
            activeAccountNetworkService
        ) { accounts, activeAccountPersistentId, activeAccountNetworkService ->
            val account = accounts.firstOrNull { it.persistentId == activeAccountPersistentId }
                ?: accounts.first()

            ActiveAccountData(account, activeAccountNetworkService)
        }.stateIn(
            viewModelScope,
            SharingStarted.WhileSubscribed(5000),
            ActiveAccountData(NativeAccount.Account(), NativeSettings.NetworkService.OFFLINE)
        )


    fun setActiveAccount(persistentId: Int) {
        if (!accounts.value.any { it.persistentId == persistentId }) {
            return
        }

        saveActiveAccount()

        NativeSettings.setAccountPersistentId(persistentId)
        activeAccountPersistentId.value = persistentId
    }

    fun setNetworkServiceForActiveAccount(networkService: Int) {
        NativeSettings.setAccountNetworkService(activeAccountPersistentId.value, networkService)
        activeAccountNetworkService.value = networkService
    }

    fun deleteActiveAccount() {
        if (accounts.value.size <= MIN_ACCOUNT_COUNT) {
            return
        }

        NativeAccount.deleteAccount(activeAccountPersistentId.value)
        activeAccountPersistentId.value = accounts.value.first().persistentId
        refreshAccountList()
    }


    fun validateCreateAccount(createAccountData: CreateAccount): CreateAccountError? {
        if (createAccountData.persistentId == null) {
            return CreateAccountError.EmptyPersistentId
        }

        if (createAccountData.persistentId.toUInt() < NativeAccount.MIN_PERSISTENT_ID) {
            return CreateAccountError.InvalidPersistentId
        }

        val existingAccount =
            accounts.value.firstOrNull { it.persistentId == createAccountData.persistentId }
        if (existingAccount != null) {
            return CreateAccountError.ConflictingPersistentId(
                existingAccount.persistentId,
                existingAccount.miiName
            )
        }

        if (createAccountData.miiName.isBlank()) {
            return CreateAccountError.EmptyMiiName
        }

        return null
    }

    fun updateActiveAccount(account: NativeAccount.Account) {
        _accounts.value =
            _accounts.value.map { if (it.persistentId == account.persistentId) account else it }
    }

    fun saveActiveAccount() {
        var account = activeAccountData.value.account
        if (account.miiName.isEmpty()) {
            account = account.copy(miiName = NativeAccount.DEFAULT_MII_NAME)
        }
        NativeAccount.saveAccount(account)
        refreshAccountList()
    }

    fun createAccount(createAccountData: CreateAccount) {
        if (validateCreateAccount(createAccountData) != null) {
            return
        }

        if (accounts.value.size >= MAX_ACCOUNT_COUNT) {
            return
        }

        NativeAccount.createAccount(createAccountData.persistentId!!, createAccountData.miiName)
        refreshAccountList()
    }

    fun getActiveAccountValidationErrors(): Array<NativeAccount.OnlineValidationError> {
        val persistentId = activeAccountData.value.account.persistentId

        return NativeAccount.getAccountValidationErrors(persistentId)
    }

    fun refreshAccountList() {
        _accounts.value = NativeAccount.getAccounts().toList()
    }
}