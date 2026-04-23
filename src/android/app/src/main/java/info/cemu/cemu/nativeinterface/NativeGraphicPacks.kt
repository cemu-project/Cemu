package info.cemu.cemu.nativeinterface

import androidx.annotation.Keep
import java.util.Objects

object NativeGraphicPacks {
    @JvmStatic
    external fun getGraphicPackBasicInfos(): Array<GraphicPackBasicInfo>

    @JvmStatic
    external fun refreshGraphicPacks()

    @JvmStatic
    external fun getGraphicPack(id: Long): GraphicPack?

    @JvmStatic
    external fun setGraphicPackActive(id: Long, active: Boolean)

    @JvmStatic
    external fun setGraphicPackActivePreset(id: Long, category: String?, preset: String?)

    @JvmStatic
    external fun getGraphicPackPresets(id: Long): Array<GraphicPackPreset>

    @Keep
    data class GraphicPackBasicInfo(
        val id: Long,
        val virtualPath: String,
        val enabled: Boolean,
        val titleIds: LongArray
    ) {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (javaClass != other?.javaClass) return false

            other as GraphicPackBasicInfo

            if (id != other.id) return false
            if (enabled != other.enabled) return false
            if (virtualPath != other.virtualPath) return false
            if (!titleIds.contentEquals(other.titleIds)) return false

            return true
        }

        override fun hashCode(): Int {
            var result = id.hashCode()
            result = 31 * result + enabled.hashCode()
            result = 31 * result + virtualPath.hashCode()
            result = 31 * result + titleIds.contentHashCode()
            return result
        }
    }

    @Keep
    class GraphicPackPreset(
        private val graphicPackId: Long,
        val category: String?,
        val presets: Array<String>,
        private var _activePreset: String
    ) {
        override fun hashCode(): Int {
            return Objects.hash(graphicPackId, category, presets, _activePreset)
        }

        override fun equals(other: Any?): Boolean {
            if (other == null) {
                return false
            }
            if (other === this) {
                return true
            }
            if (other is GraphicPackPreset) {
                return this.hashCode() == other.hashCode()
            }
            return false
        }

        var activePreset: String
            get() = _activePreset
            set(value) {
                require(presets.any { it == value }) { "Trying to set an invalid preset: $value" }
                setGraphicPackActivePreset(graphicPackId, category, value)
                _activePreset = value
            }
    }

    @Keep
    class GraphicPack(
        val id: Long,
        private var active: Boolean,
        val name: String,
        val description: String,
        private var _presets: Array<GraphicPackPreset>
    ) {
        fun isActive(): Boolean {
            return active
        }

        val presets: Array<GraphicPackPreset>
            get() = _presets

        fun reloadPresets() {
            _presets = getGraphicPackPresets(id)
        }

        fun setActive(active: Boolean) {
            this.active = active
            setGraphicPackActive(id, active)
        }
    }
}
