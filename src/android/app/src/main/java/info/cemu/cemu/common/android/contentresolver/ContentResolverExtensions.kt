package info.cemu.cemu.common.android.contentresolver

import android.content.ContentResolver
import android.net.Uri
import android.provider.DocumentsContract
import kotlinx.coroutines.yield

sealed class DocumentEntry {
    data class File(val uri: Uri, val size: Long) : DocumentEntry()
    data class Directory(val uri: Uri) : DocumentEntry()
}

suspend fun ContentResolver.walkDocumentTree(
    dirUri: Uri,
    onEntry: (DocumentEntry) -> Unit,
) {
    val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
        dirUri,
        DocumentsContract.getDocumentId(dirUri)
    )

    val cursor = query(
        childrenUri,
        arrayOf(
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_SIZE,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
        ),
        null,
        null,
        null
    )

    cursor?.use {
        while (it.moveToNext()) {
            yield()

            val documentId = it.getString(0)
            val documentUri = DocumentsContract.buildDocumentUriUsingTree(dirUri, documentId)

            val mimeType = it.getString(2)
            if (mimeType != DocumentsContract.Document.MIME_TYPE_DIR) {
                val sizeInBytes = it.getLong(1)
                onEntry(DocumentEntry.File(documentUri, sizeInBytes))
                continue
            }

            onEntry(DocumentEntry.Directory(documentUri))

            walkDocumentTree(
                DocumentsContract.buildDocumentUriUsingTree(dirUri, documentId),
                onEntry,
            )
        }
    }
}
