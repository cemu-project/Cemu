package info.cemu.cemu.about

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.LinkAnnotation
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextLinkStyles
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.text.withLink
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mikepenz.aboutlibraries.Libs
import com.mikepenz.aboutlibraries.ui.compose.LibraryDefaults
import com.mikepenz.aboutlibraries.ui.compose.android.produceLibraries
import com.mikepenz.aboutlibraries.ui.compose.m3.LicenseDialog
import com.mikepenz.aboutlibraries.ui.compose.m3.LicenseDialogBody
import com.mikepenz.aboutlibraries.ui.compose.m3.libraryColors
import com.mikepenz.aboutlibraries.ui.compose.util.author
import info.cemu.cemu.BuildConfig
import info.cemu.cemu.R
import info.cemu.cemu.common.ui.components.ScreenContentLazy
import info.cemu.cemu.common.ui.localization.tr

typealias AboutLibrary = com.mikepenz.aboutlibraries.entity.Library

@Composable
fun AboutCemuScreen(navigateBack: () -> Unit) {
    var openLicenseDialog by remember { mutableStateOf<AboutLibrary?>(null) }
    val libraries by produceLibraries(R.raw.aboutlibraries)

    ScreenContentLazy(
        appBarText = tr("About Cemu"),
        navigateBack = navigateBack,
        contentModifier = Modifier
            .fillMaxWidth()
            .padding(8.dp),
        contentVerticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        aboutCemuSection()
        disclaimerSection()
        nativeLibrariesSection()
        kotlinLibrariesSection(
            libraries = libraries,
            onOpenLicense = { lib -> openLicenseDialog = lib },
        )
    }

    openLicenseDialog?.let { library ->
        LicenseDialog(
            library = library,
            body = { library, modifier ->
                LicenseDialogBody(
                    library = library,
                    colors = LibraryDefaults.libraryColors(),
                    modifier = Modifier.padding(4.dp)
                )
            },
            onDismiss = { openLicenseDialog = null },
            confirmText = tr("OK"),
        )
    }
}

private fun LazyListScope.aboutCemuSection() {
    item {
        AboutSection {
            Text(
                text = stringResource(R.string.app_name),
                fontSize = 32.sp,
            )
            Text(
                text = tr("Version: {0}", BuildConfig.VERSION_NAME),
                fontSize = 18.sp,
            )
            Text(
                text = tr(
                    "Original authors: {0}",
                    stringResource(R.string.cemu_original_authors)
                ),
                fontSize = 18.sp,
            )
            CemuWebsite()
        }
    }
}

private fun LazyListScope.disclaimerSection() {
    item {
        AboutSection {
            Text(
                text = tr("Cemu is a Wii U emulator.\n\nWii and Wii U are trademarks of Nintendo.\nCemu is not affiliated with Nintendo."),
                fontSize = 18.sp
            )
        }
    }
}

private fun LazyListScope.nativeLibrariesSection() {
    item {
        AboutSection {
            Text(
                text = tr("Used libraries:"),
                fontSize = 24.sp,
            )
            UsedLibraries.forEach {
                Library(it)
            }
        }
    }
}

private fun LazyListScope.kotlinLibrariesSection(
    libraries: Libs?,
    onOpenLicense: (AboutLibrary) -> Unit,
) {
    val libs = libraries?.libraries ?: listOf()
    item {
        Text(
            modifier = Modifier.padding(horizontal = 8.dp),
            text = tr("Kotlin libraries:"),
            fontSize = 24.sp,
        )
    }
    items(items = libs) { lib ->
        Column(
            verticalArrangement = Arrangement.spacedBy(4.dp),
            modifier = Modifier
                .padding(horizontal = 8.dp)
                .clickable { onOpenLicense(lib) },
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = lib.name,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f)
                )
                Text(lib.artifactVersion ?: "")
            }
            Text(lib.author)
            FlowRow(horizontalArrangement = Arrangement.SpaceBetween) {
                lib.licenses.forEach { license ->
                    Card {
                        Text(
                            text = license.name,
                            modifier = Modifier.padding(horizontal = 4.dp)
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun CemuWebsite() {
    Text(
        buildAnnotatedString {
            withLink(
                LinkAnnotation.Url(
                    stringResource(R.string.cemu_website),
                    TextLinkStyles(
                        style = SpanStyle(
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            textDecoration = TextDecoration.Underline,
                        ),
                    )
                )
            ) {
                append(stringResource(R.string.cemu_website))
            }
        }
    )
}

@Composable
private fun AboutSection(content: @Composable ColumnScope.() -> Unit) {
    SelectionContainer {
        Column(
            verticalArrangement = Arrangement.spacedBy(4.dp),
            modifier = Modifier.padding(8.dp),
            content = content,
        )
    }
}

@Composable
private fun Library(library: Library) {
    val (name, source) = library
    Text(
        buildAnnotatedString {
            append(name)
            append(" (")
            withLink(
                LinkAnnotation.Url(
                    source.url,
                    TextLinkStyles(
                        style = SpanStyle(
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            textDecoration = TextDecoration.Underline,
                        ),
                    )
                )
            ) {
                append(source.name ?: source.url)
            }
            append(")")
        }
    )
}

private data class LibrarySource(val url: String, val name: String? = null)

private data class Library(
    val text: String,
    val source: LibrarySource,
) {
    constructor(text: String, url: String) : this(text, LibrarySource(url))
}

private val UsedLibraries: List<Library> = listOf(
    Library("zlib", "https://www.zlib.net"),
    Library("OpenSSL", "https://www.openssl.org"),
    Library("libcurl", "https://curl.haxx.se/libcurl"),
    Library("imgui", "https://github.com/ocornut/imgui"),
    Library("fontawesome", "https://github.com/FortAwesome/Font-Awesome"),
    Library("boost", "https://www.boost.org"),
    Library("libusb", "https://libusb.info"),
    Library(
        "Modified ih264 from Android project",
        LibrarySource(
            "https://github.com/cemu-project/Cemu/tree/main/dependencies/ih264d",
            "Source"
        )
    )
)
