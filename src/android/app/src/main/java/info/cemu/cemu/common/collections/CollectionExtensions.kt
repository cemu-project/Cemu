package info.cemu.cemu.common.collections

fun <T> Set<T>.toggleInSet(item: T): Set<T> {
    return if (item in this) this - item else this + item
}

fun <R : Any> IntArray.mapNotNull(transform: (Int) -> R?) = map { transform(it) }.filterNotNull()
