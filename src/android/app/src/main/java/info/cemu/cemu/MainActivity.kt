package info.cemu.cemu

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.ShortcutInfo
import android.content.pm.ShortcutManager
import android.graphics.drawable.Icon
import android.os.Bundle
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.rememberNavController
import info.cemu.cemu.about.AboutCemuRoute
import info.cemu.cemu.about.aboutCemuNavigation
import info.cemu.cemu.common.input.GamepadInputSource
import info.cemu.cemu.common.ui.components.ActivityContent
import info.cemu.cemu.common.ui.localization.TranslatableContent
import info.cemu.cemu.emulation.EmulationActivity
import info.cemu.cemu.games.GameListRoute
import info.cemu.cemu.games.gamesNavigation
import info.cemu.cemu.graphicpacks.GraphicPacksRoute
import info.cemu.cemu.graphicpacks.graphicPacksNavigation
import info.cemu.cemu.nativeinterface.NativeGameTitles.Game
import info.cemu.cemu.nativeinterface.NativeSettings
import info.cemu.cemu.settings.SettingsRoute
import info.cemu.cemu.settings.settingsNavigation
import info.cemu.cemu.titlemanager.TitleManagerRoute
import info.cemu.cemu.titlemanager.titleManagerNavigation

class MainActivity : AppCompatActivity() {
    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        GamepadInputSource.emitMotion(event)

        if (GamepadInputSource.hasMotionSubscribers) {
            return true
        }

        return super.dispatchGenericMotionEvent(event)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        GamepadInputSource.emitKey(event)

        if (GamepadInputSource.hasKeySubscribers) {
            return true
        }

        return super.dispatchKeyEvent(event)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            TranslatableContent {
                ActivityContent {
                    MainNav()
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        NativeSettings.saveSettings()
    }

    override fun onPause() {
        super.onPause()
        NativeSettings.saveSettings()
    }
}

@Composable
private fun MainNav() {
    val navController = rememberNavController()
    val context = LocalContext.current

    NavHost(
        navController = navController,
        startDestination = GameListRoute,
        enterTransition = { EnterTransition.None },
        exitTransition = { ExitTransition.None }) {
        gamesNavigation(
            navController = navController,
            startGame = { startGame(context, it) },
            tryCreateShortcut = { tryCreateShortcutForGame(context, it) },
            goToSettings = { navController.navigate(SettingsRoute) },
            goToTitleManager = { navController.navigate(TitleManagerRoute) },
            goToGraphicPacks = { navController.navigate(GraphicPacksRoute) },
            goToAboutCemu = { navController.navigate(AboutCemuRoute) },
        )
        settingsNavigation(navController)
        titleManagerNavigation(navController)
        graphicPacksNavigation(navController)
        aboutCemuNavigation(navController)
    }
}

private fun createIntentForGame(context: Context, game: Game): Intent {
    val intent = Intent(
        context, EmulationActivity::class.java
    )
    intent.action = Intent.ACTION_VIEW
    intent.putExtra(EmulationActivity.EXTRA_LAUNCH_PATH, game.path)

    return intent
}

private fun startGame(context: Context, game: Game) {
    NativeSettings.saveSettings()

    val intent = createIntentForGame(context, game)
    context.startActivity(intent)
}

private fun tryCreateShortcutForGame(
    context: Context,
    game: Game,
): Boolean {
    try {
        val shortcutManager = context.getSystemService(
            ShortcutManager::class.java
        )
        if (!shortcutManager.isRequestPinShortcutSupported) {
            return false
        }

        val icon = game.icon?.asAndroidBitmap().let {
            if (it != null) Icon.createWithBitmap(it)
            else Icon.createWithResource(context, R.mipmap.ic_launcher)
        }

        val intent = createIntentForGame(context, game)

        val pinShortcutInfo =
            ShortcutInfo.Builder(context, game.titleId.toString()).setShortLabel(game.name!!)
                .setIntent(intent).setIcon(icon).build()

        val pinnedShortcutCallbackIntent =
            shortcutManager.createShortcutResultIntent(pinShortcutInfo)

        val successCallback = PendingIntent.getBroadcast(
            context, 0, pinnedShortcutCallbackIntent, PendingIntent.FLAG_IMMUTABLE
        )

        shortcutManager.requestPinShortcut(pinShortcutInfo, successCallback.intentSender)
        return true
    } catch (_: Exception) {
        return false
    }
}
