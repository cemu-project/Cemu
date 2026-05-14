package info.cemu.cemu.common.either

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.withContext
import kotlin.coroutines.CoroutineContext

data class Success<out T, out E>(val data: T) : Either<T, E>()
data class Error<out T, out E>(val error: E) : Either<T, E>()

sealed class Either<out T, out E> {
    inline fun <R> fold(onSuccess: (T) -> R, onError: (E) -> R): R = when (this) {
        is Success -> onSuccess(data)
        is Error -> onError(error)
    }

    inline fun onError(block: (E) -> Unit) {
        if (this is Error) {
            block(error)
        }
    }
}

inline fun <T> attempt(block: () -> T): Either<T, String> = try {
    Success(block())
} catch (e: Exception) {
    Error(e.message ?: "Unknown error")
}

suspend inline fun <T> attemptWithContext(
    context: CoroutineContext,
    noinline block: suspend CoroutineScope.() -> T
): Either<T, String> = withContext(context) {
    return@withContext attempt { block() }
}

inline fun <T, E, F> Either<T, E>.mapError(
    transform: (E) -> F
): Either<T, F> = fold(
    onSuccess = { Success(it) },
    onError = { Error(transform(it)) }
)

inline fun <T, E, R> Either<T, E>.mapSuccess(
    transform: (T) -> R
): Either<R, E> = fold(
    onSuccess = { Success(transform(it)) },
    onError = { Error(it) }
)

inline fun <T, E, R> Either<T, E>.bind(
    next: (T) -> Either<R, E>
): Either<R, E> = fold(
    onSuccess = { next(it) },
    onError = { Error(it) }
)
