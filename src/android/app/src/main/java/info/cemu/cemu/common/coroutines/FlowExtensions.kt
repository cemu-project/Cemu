package info.cemu.cemu.common.coroutines

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class RefreshableStateFlow<T>(
    private val getter: () -> T
) {
    private val _state = MutableStateFlow(getter())
    val state: StateFlow<T> = _state.asStateFlow()

    fun refresh() {
        _state.value = getter()
    }
}