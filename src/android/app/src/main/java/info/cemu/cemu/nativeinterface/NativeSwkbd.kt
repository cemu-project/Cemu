package info.cemu.cemu.nativeinterface

import androidx.annotation.Keep
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

object NativeSwkbd {

    object SwkbdState {
        data class State(
            val text: String,
            val maxLength: Int,
        )

        private val _data = MutableStateFlow<State?>(null)
        val data = _data.asStateFlow()

        private val BlacklistedCharactersRegex =
            Regex("[^\\da-zA-Z \\-/;:',.?!#\\[\\]$%^&*()_@\\\\<>+=]")

        fun updateText(text: String) {
            val currentState = _data.value ?: return

            val numChars = if (currentState.maxLength > 0) currentState.maxLength else text.length
            val newText = BlacklistedCharactersRegex.replace(text, "").take(numChars)
            _data.value = currentState.copy(
                text = newText
            )

            onTextChanged(newText)
        }

        @Keep
        @JvmStatic
        @Suppress("unused")
        private fun makeVisible(initialText: String?, maxLength: Int) {
            _data.value = State(initialText ?: "", maxLength)
        }

        @Keep
        @JvmStatic
        @Suppress("unused")
        private fun hide() {
            _data.value = null
        }
    }

    @JvmStatic
    external fun initializeSwkbd()

    @JvmStatic
    private external fun onTextChanged(text: String?)

    @JvmStatic
    external fun onFinishedInputEdit()
}
