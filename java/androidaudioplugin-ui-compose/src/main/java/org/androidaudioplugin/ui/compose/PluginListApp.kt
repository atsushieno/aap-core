package org.androidaudioplugin.ui.compose

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.contentColor
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Paint
import androidx.compose.ui.graphics.PaintingStyle
import androidx.compose.ui.unit.dp
import com.arkivanov.decompose.extensions.compose.jetpack.asState
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceConnection

@Composable
fun PluginListApp() {
    MaterialTheme { PluginListAppContent() }
}

@Composable
fun PluginListAppContent() {
    Surface {
        val state = pluginListViewModel.state.asState()
        ModalPanelLayout(
            bodyContent = { HomeScreen(state.value.availablePluginServices) },
            modalState = state.value.modalState,
            onModalStateChanged = { modalState -> pluginListViewModel.onModalStateChanged(modalState) },
            plugin = state.value.selectedPluginDetails
        )
    }
}

@Composable
fun ModalPanelLayout(
    bodyContent: @Composable() () -> Unit,
    modalState: ModalPanelState = ModalPanelState.None,
    onModalStateChanged: (ModalPanelState) -> Unit,
    plugin: PluginInformation? = null
) {
    Box(Modifier.fillMaxSize()) {
        bodyContent()

        val panelContent = @Composable() {
            when (modalState) {
                ModalPanelState.ShowPluginDetails -> {
                    if (plugin != null)
                        PluginDetails(plugin)
                }
            }
        }

        if (modalState != ModalPanelState.None) {
            // Taken from Drawer.kt in androidx.ui/ui-material and modified.
            // begin
            val ScrimDefaultOpacity = 0.32f
            val PluginListPanelPadding = 24.dp
            Surface(Modifier.clickable(onClick = { onModalStateChanged(ModalPanelState.None) })) {
                val paint = remember { Paint().apply { style = PaintingStyle.Fill } }
                val color = MaterialTheme.colors.onSurface
                Canvas(Modifier.fillMaxSize()) {
                    drawRect(color.copy(alpha = ScrimDefaultOpacity))
                }
            }
            // end

            Box(Modifier.padding(PluginListPanelPadding)) {
                // remove Container when we will support multiply children
                Surface {
                    Box(Modifier.fillMaxSize(), Alignment.CenterStart) { panelContent() }
                }
            }
        }
    }
}

enum class ModalPanelState {
    None,
    ShowPluginDetails
}

@Composable
fun HomeScreen(
    pluginServices: List<AudioPluginServiceInformation>
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Text(
                        text = "Plugins in this Plugin Service",
                        style = MaterialTheme.typography.subtitle2.copy(color = contentColor())
                    )
                }
            )
        },
        bodyContent = {
            AvailablePlugins(onItemClick = { p ->
                pluginListViewModel.onSelectedPluginDetailsChanged(p)
                pluginListViewModel.onModalStateChanged(ModalPanelState.ShowPluginDetails)
                },
                pluginServices = pluginServices)
        }
    )
}
