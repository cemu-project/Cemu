package info.cemu.cemu.common.settings

enum class GamePadPosition {
    ABOVE,
    BELOW,
    LEFT,
    RIGHT;

    fun isVertical() = this == ABOVE || this == BELOW
    fun appearsAfterTV() = this == BELOW || this == RIGHT
}