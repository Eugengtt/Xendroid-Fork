package xendroid.compose.ui

import android.net.Uri
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.NavBackStackEntry
import androidx.navigation.NavController
import androidx.navigation.NavType
import androidx.navigation.navArgument
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import xendroid.compose.AppContainer
import xendroid.compose.data.GameFormat
import xendroid.compose.gamepad.GamepadController
import xendroid.compose.gamepad.GamepadEditorScreen
import xendroid.compose.settings.GameSettingsViewModel
import xendroid.compose.settings.SettingsViewModel
import xendroid.compose.ui.compress.GameCompressViewModel
import xendroid.compose.ui.content.ContentManagerViewModel
import xendroid.compose.ui.content.ContentManagerScreen
import xendroid.compose.ui.content.InstallContentViewModel
import xendroid.compose.ui.content.InstallContentScreen
import xendroid.compose.ui.library.GameLibraryScreen
import xendroid.compose.ui.library.GameLibraryViewModel
import xendroid.compose.ui.settings.PerGameSettingsScreen
import xendroid.compose.ui.settings.SettingsScreen
import xendroid.compose.ui.about.AboutScreen
import xendroid.compose.ui.keymap.KeymapScreen
import xendroid.compose.ui.keymap.KeymapViewModel
import xendroid.compose.ui.profile.ProfileManagerViewModel
import xendroid.compose.ui.profile.ProfilesScreen
import xendroid.compose.patches.GamePatchesViewModel
import xendroid.compose.ui.patches.GamePatchesScreen

object Routes {
    const val LIBRARY = "library"
    const val SETTINGS = "settings"
    const val KEYMAP = "keymap"
    const val ABOUT = "about"
    const val PROFILES = "profiles"
    const val USERDATA = "userdata"   // action, not a destination (see GameLibraryScreen)
    const val GAMEPAD_EDITOR = "gamepad_editor"
    // "$PER_GAME_SETTINGS/{titleId}?name={name}&format={format}&uri={uri}"
    const val PER_GAME_SETTINGS = "per_game_settings"
    // "$GAME_PATCHES/{titleId}?name={name}"
    const val GAME_PATCHES = "game_patches"
    // "$CONTENT_MANAGER/{titleId}?name={name}"
    const val CONTENT_MANAGER = "content_manager"
    const val INSTALL_CONTENT = "install_content"
}

private fun NavBackStackEntry.backOnce(nav: NavController): () -> Unit = {
    if (lifecycle.currentState.isAtLeast(Lifecycle.State.RESUMED)) nav.popBackStack()
}

@Composable
fun AppNavHost(container: AppContainer) {
    val nav = rememberNavController()
    NavHost(navController = nav, startDestination = Routes.LIBRARY) {
        composable(Routes.LIBRARY) { libraryEntry ->
            val vm: GameLibraryViewModel =
                viewModel(factory = container.libraryViewModelFactory())
            val compressVm: GameCompressViewModel =
                viewModel(factory = container.gameCompressViewModelFactory())
            // Guard against duplicate navigation: the library entry drops below RESUMED after the
            // first navigate, so a racy second navigate from the same event is a no-op.
            val navigateOnce: (String) -> Unit = { route ->
                if (libraryEntry.lifecycle.currentState.isAtLeast(Lifecycle.State.RESUMED)) {
                    nav.navigate(route)
                }
            }
            GameLibraryScreen(
                viewModel = vm,
                compressVm = compressVm,
                onOpenSettings = { navigateOnce(Routes.SETTINGS) },
                onOpenKeymap = { navigateOnce(Routes.KEYMAP) },
                onOpenAbout = { navigateOnce(Routes.ABOUT) },
                onOpenProfiles = { navigateOnce(Routes.PROFILES) },
                onOpenTouchControls = { navigateOnce(Routes.GAMEPAD_EDITOR) },
                onOpenPerGameSettings = { titleId, name, format, launchUri ->
                    navigateOnce(
                        "${Routes.PER_GAME_SETTINGS}/$titleId" +
                            "?name=${Uri.encode(name)}" +
                            "&format=${format.name}" +
                            "&uri=${Uri.encode(launchUri)}"
                    )
                },
                onOpenGamePatches = { titleId, name ->
                    navigateOnce("${Routes.GAME_PATCHES}/$titleId?name=${Uri.encode(name)}")
                },
                onOpenContentManager = { titleId, name ->
                    navigateOnce("${Routes.CONTENT_MANAGER}/$titleId?name=${Uri.encode(name)}")
                },
                onOpenInstallContent = { navigateOnce(Routes.INSTALL_CONTENT) },
                onInstallFromDisc = { path ->
                    navigateOnce("${Routes.INSTALL_CONTENT}?src=" + Uri.encode(path))
                },
            )
        }
        composable(Routes.SETTINGS) { entry ->
            val vm: SettingsViewModel = viewModel(factory = container.settingsViewModelFactory())
            SettingsScreen(vm = vm, onBack = entry.backOnce(nav))
        }
        composable(Routes.KEYMAP) { entry ->
            val vm: KeymapViewModel =
                viewModel(factory = container.keymapViewModelFactory())
            KeymapScreen(vm = vm, onBack = entry.backOnce(nav))
        }
        composable(Routes.ABOUT) { entry ->
            AboutScreen(onBack = entry.backOnce(nav))
        }
        composable(Routes.PROFILES) { entry ->
            val vm: ProfileManagerViewModel =
                viewModel(factory = container.profileManagerViewModelFactory())
            ProfilesScreen(vm = vm, onBack = entry.backOnce(nav))
        }
        composable(Routes.GAMEPAD_EDITOR) { entry ->
            val ctx = LocalContext.current
            val controller = remember { GamepadController(ctx.applicationContext) }
            GamepadEditorScreen(controller = controller, onDone = entry.backOnce(nav))
        }
        composable(
            route = "${Routes.PER_GAME_SETTINGS}/{titleId}?name={name}&format={format}&uri={uri}",
            arguments = listOf(
                navArgument("titleId") { type = NavType.StringType },
                navArgument("name") { type = NavType.StringType; nullable = true; defaultValue = null },
                navArgument("format") { type = NavType.StringType; nullable = true; defaultValue = null },
                navArgument("uri") { type = NavType.StringType; nullable = true; defaultValue = null },
            ),
        ) { backStackEntry ->
            val titleId = backStackEntry.arguments?.getString("titleId") ?: return@composable
            val gameName = backStackEntry.arguments?.getString("name") ?: ""
            val vm: GameSettingsViewModel =
                viewModel(factory = container.gameSettingsViewModelFactory(titleId))
            PerGameSettingsScreen(
                vm = vm,
                gameName = gameName,
                onBack = backStackEntry.backOnce(nav),
            )
        }
        composable(
            route = "${Routes.GAME_PATCHES}/{titleId}?name={name}",
            arguments = listOf(
                navArgument("titleId") { type = NavType.StringType },
                navArgument("name") { type = NavType.StringType; nullable = true; defaultValue = null },
            ),
        ) { backStackEntry ->
            val titleId = backStackEntry.arguments?.getString("titleId") ?: return@composable
            val gameName = backStackEntry.arguments?.getString("name") ?: ""
            val vm: GamePatchesViewModel =
                viewModel(factory = container.gamePatchesViewModelFactory(titleId))
            GamePatchesScreen(vm = vm, gameName = gameName, onBack = backStackEntry.backOnce(nav))
        }
        composable(
            route = "${Routes.CONTENT_MANAGER}/{titleId}?name={name}",
            arguments = listOf(
                navArgument("titleId") { type = NavType.StringType },
                navArgument("name") { type = NavType.StringType; nullable = true; defaultValue = null },
            ),
        ) { backStackEntry ->
            val titleId = backStackEntry.arguments?.getString("titleId") ?: return@composable
            val gameName = backStackEntry.arguments?.getString("name") ?: ""
            val vm: ContentManagerViewModel =
                viewModel(factory = container.gameContentManagerViewModelFactory(titleId))
            ContentManagerScreen(vm = vm, gameName = gameName, onBack = backStackEntry.backOnce(nav))
        }
        composable(
            "${Routes.INSTALL_CONTENT}?src={src}",
            arguments = listOf(navArgument("src") { nullable = true; defaultValue = null }),
        ) { entry ->
            val vm: InstallContentViewModel =
                viewModel(factory = container.installContentViewModelFactory())
            InstallContentScreen(
                vm = vm,
                onBack = entry.backOnce(nav),
                sourcePath = entry.arguments?.getString("src")?.let { Uri.decode(it) },
            )
        }
    }
}
