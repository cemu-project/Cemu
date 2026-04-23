package info.cemu.cemu.settings

import androidx.navigation.NavGraphBuilder
import androidx.navigation.NavHostController
import androidx.navigation.compose.composable
import androidx.navigation.compose.navigation
import androidx.navigation.toRoute
import info.cemu.cemu.settings.account.AccountSettingsScreen
import info.cemu.cemu.settings.audio.AudioSettingsScreen
import info.cemu.cemu.settings.customdrivers.CustomDriversScreen
import info.cemu.cemu.settings.emulatedusbdevices.EmulatedUSBDevicesSettingsScreen
import info.cemu.cemu.settings.gamespath.GamePathsScreen
import info.cemu.cemu.settings.general.GeneralSettingsScreen
import info.cemu.cemu.settings.graphics.GraphicsSettingsScreen
import info.cemu.cemu.settings.input.controller.ControllerInputSettingsScreen
import info.cemu.cemu.settings.input.device.DeviceInputSettingsScreen
import info.cemu.cemu.settings.input.hotkeys.HotkeySettingsScreen
import info.cemu.cemu.settings.input.InputSettingsScreen
import info.cemu.cemu.settings.inputoverlay.InputOverlaySettingsScreen
import info.cemu.cemu.settings.overlay.OverlaySettingsScreen
import kotlinx.serialization.Serializable

@Serializable
object SettingsRoute

private object SettingsRoutes {
    @Serializable
    object GeneralSettings

    @Serializable
    object GeneralSettingsScreenRoute

    @Serializable
    object InputSettingsRoute

    @Serializable
    object SettingsHomeScreenRoute

    @Serializable
    object AudioSettingsScreenRoute

    @Serializable
    object GraphicsSettingsScreenRoute

    @Serializable
    object CustomDriversScreenRoute

    @Serializable
    object GamePathsScreenRoute

    @Serializable
    object OverlaySettingsScreenRoute

    @Serializable
    object InputSettingsScreenRoute

    @Serializable
    object EmulatedUSBDevicesSettingsScreenRoute

    @Serializable
    data class ControllerInputSettingsScreenRoute(val index: Int)

    @Serializable
    object InputOverlaySettingsScreenRoute

    @Serializable
    object DeviceInputSettingsScreenRoute

    @Serializable
    object HotkeySettingsScreenRoute

    @Serializable
    object AccountSettingsScreenRoute
}

fun NavGraphBuilder.settingsNavigation(navController: NavHostController) {
    navigation<SettingsRoute>(startDestination = SettingsRoutes.SettingsHomeScreenRoute) {
        composable<SettingsRoutes.SettingsHomeScreenRoute> {
            SettingsHomeScreen(
                navigateBack = { navController.popBackStack() },
                goToGeneralSettings = { navController.navigate(SettingsRoutes.GeneralSettings) },
                goToInputSettings = { navController.navigate(SettingsRoutes.InputSettingsRoute) },
                goToGraphicsSettings = { navController.navigate(SettingsRoutes.GraphicsSettingsScreenRoute) },
                goToAudioSettings = { navController.navigate(SettingsRoutes.AudioSettingsScreenRoute) },
                goToOverlaySettings = { navController.navigate(SettingsRoutes.OverlaySettingsScreenRoute) },
                goToAccountSettings = { navController.navigate(SettingsRoutes.AccountSettingsScreenRoute) },
                goToEmulatedUSBDevicesSettings = { navController.navigate(SettingsRoutes.EmulatedUSBDevicesSettingsScreenRoute) },
            )
        }
        composable<SettingsRoutes.AudioSettingsScreenRoute> {
            AudioSettingsScreen(
                navigateBack = { navController.popBackStack() },
            )
        }
        composable<SettingsRoutes.GraphicsSettingsScreenRoute> {
            GraphicsSettingsScreen(
                navigateBack = { navController.popBackStack() },
                goToCustomDriversSettings = {
                    navController.navigate(SettingsRoutes.CustomDriversScreenRoute)
                }
            )
        }
        composable<SettingsRoutes.CustomDriversScreenRoute> {
            CustomDriversScreen(
                navigateBack = { navController.popBackStack() },
            )
        }
        composable<SettingsRoutes.OverlaySettingsScreenRoute> {
            OverlaySettingsScreen(
                navigateBack = { navController.popBackStack() },
            )
        }
        composable<SettingsRoutes.EmulatedUSBDevicesSettingsScreenRoute> {
            EmulatedUSBDevicesSettingsScreen(
                navigateBack = { navController.popBackStack() },
            )
        }
        navigation<SettingsRoutes.InputSettingsRoute>(startDestination = SettingsRoutes.InputSettingsScreenRoute) {
            composable<SettingsRoutes.ControllerInputSettingsScreenRoute> { navBackStackEntry ->
                val controllerIndex =
                    navBackStackEntry.toRoute<SettingsRoutes.ControllerInputSettingsScreenRoute>().index
                ControllerInputSettingsScreen(
                    navigateBack = { navController.popBackStack() },
                    controllerIndex = controllerIndex,
                )
            }

            composable<SettingsRoutes.HotkeySettingsScreenRoute> {
                HotkeySettingsScreen(
                    navigateBack = { navController.popBackStack() }
                )
            }

            composable<SettingsRoutes.InputOverlaySettingsScreenRoute> {
                InputOverlaySettingsScreen(
                    navigateBack = { navController.popBackStack() }
                )
            }

            composable<SettingsRoutes.DeviceInputSettingsScreenRoute> {
                DeviceInputSettingsScreen(
                    navigateBack = { navController.popBackStack() }
                )
            }

            composable<SettingsRoutes.InputSettingsScreenRoute> {
                InputSettingsScreen(
                    navigateBack = { navController.popBackStack() },
                    goToHotkeySettings = {
                        navController.navigate(SettingsRoutes.HotkeySettingsScreenRoute)
                    },
                    goToInputOverlaySettings = {
                        navController.navigate(SettingsRoutes.InputOverlaySettingsScreenRoute)
                    },
                    goToControllerSettings = { controllerIndex ->
                        navController.navigate(
                            SettingsRoutes.ControllerInputSettingsScreenRoute(
                                controllerIndex
                            )
                        )
                    },
                    goToHostInputSettings = {
                        navController.navigate(SettingsRoutes.DeviceInputSettingsScreenRoute)
                    },
                )
            }
        }
        navigation<SettingsRoutes.GeneralSettings>(startDestination = SettingsRoutes.GeneralSettingsScreenRoute) {
            composable<SettingsRoutes.GeneralSettingsScreenRoute> {
                GeneralSettingsScreen(
                    navigateBack = { navController.popBackStack() },
                    goToGamePathsSettings = { navController.navigate(SettingsRoutes.GamePathsScreenRoute) }
                )
            }
            composable<SettingsRoutes.GamePathsScreenRoute> {
                GamePathsScreen(
                    navigateBack = { navController.popBackStack() },
                )
            }
        }

        composable<SettingsRoutes.AccountSettingsScreenRoute> {
            AccountSettingsScreen(
                navigateBack = { navController.popBackStack() },
            )
        }
    }
}