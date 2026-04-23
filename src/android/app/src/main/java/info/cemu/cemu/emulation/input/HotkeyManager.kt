package info.cemu.cemu.emulation.input

import android.view.KeyEvent
import info.cemu.cemu.common.android.inputevent.isFromPhysicalController
import info.cemu.cemu.common.settings.HotkeyAction
import info.cemu.cemu.common.settings.HotkeyCombo
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlin.collections.iterator

object HotkeyManager {
    private val pressedKeys: MutableSet<Int> = mutableSetOf()
    private val activeActions: MutableSet<HotkeyAction> = mutableSetOf()

    private var hotkeyMappings: Map<HotkeyAction, HotkeyCombo> = emptyMap()
    fun setHotkeyMappings(hotkeyMappings: Map<HotkeyAction, HotkeyCombo>) {
        this.hotkeyMappings = hotkeyMappings
    }

    fun onKeyEvent(keyEvent: KeyEvent) {
        if (!keyEvent.isFromPhysicalController()) {
            return
        }

        if (keyEvent.action == KeyEvent.ACTION_DOWN) {
            pressedKeys.add(keyEvent.keyCode)
        } else {
            pressedKeys.remove(keyEvent.keyCode)
        }

        checkHotkeys()
    }

    private fun checkHotkeys() {
        for ((action, combo) in hotkeyMappings) {
            val comboKeys = combo.keys

            val isActive = pressedKeys.containsAll(comboKeys)

            if (!isActive) {
                activeActions.remove(action)
                continue
            }

            if (activeActions.add(action)) {
                _actions.tryEmit(action)
            }
        }
    }


    private val _actions =
        MutableSharedFlow<HotkeyAction>(
            extraBufferCapacity = 1,
            onBufferOverflow = BufferOverflow.DROP_OLDEST
        )
    val actions = _actions.asSharedFlow()
}