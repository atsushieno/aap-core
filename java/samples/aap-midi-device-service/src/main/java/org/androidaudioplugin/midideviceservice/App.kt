package org.androidaudioplugin.midideviceservice

import android.annotation.SuppressLint
import android.content.Context
import android.media.midi.MidiDeviceInfo
import android.webkit.*
import androidx.annotation.RequiresApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.webkit.WebViewAssetLoader
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.midideviceservice.ui.theme.AAPMidiDeviceServiceTheme
import java.nio.ByteBuffer

interface Updater {
    fun setInstrumentPlugin(plugin: PluginInformation)
    fun setMidi2Enabled(value: Boolean)
    fun initializeMidi()
    fun terminateMidi()
    fun playNote()
}

private object updater: Updater {
    override fun setInstrumentPlugin(plugin: PluginInformation) {
        model.specifiedInstrument = plugin
    }

    override fun setMidi2Enabled(value: Boolean) {
        model.useMidi2Protocol = value
    }

    override fun initializeMidi() {
        model.initializeMidi()
    }

    override fun terminateMidi() {
        model.terminateMidi()
    }

    override fun playNote() {
        model.playNote()
    }
}

@Composable
fun App() {
    val plugins: List<PluginInformation> = model.pluginServices.flatMap { s -> s.plugins }.toList()
        .filter { p -> p.category?.contains("Instrument") ?: false || p.category?.contains("Synth") ?: false }

    var pluginViewShown by remember { mutableStateOf(false) }

    AAPMidiDeviceServiceTheme {
        Surface(color = MaterialTheme.colors.background) {
            Column {
                AvailablePlugins(onItemClick = { plugin -> updater.setInstrumentPlugin(plugin) }, plugins)
                PluginInstanceControllerUI()
                Button(onClick = { pluginViewShown = !pluginViewShown }) {
                    Text(if (pluginViewShown) "hide WebUI" else "show WebUI")
                }
            }
            if (pluginViewShown)
                PluginWebUI()
        }
    }
}

@Composable
fun AvailablePlugins(onItemClick: (PluginInformation) -> Unit = {}, instrumentPlugnis: List<PluginInformation>) {
    val small = TextStyle(fontSize = 12.sp)

    val state by remember { mutableStateOf(LazyListState()) }
    var selectedIndex by remember { mutableStateOf(if (model.instrument != null) instrumentPlugnis.indexOf(model.instrument!!) else -1) }

    LazyColumn(state = state) {
        itemsIndexed(instrumentPlugnis, itemContent = { index, plugin ->
            Row(modifier = Modifier
                .padding(start = 16.dp, end = 16.dp)
            ) {
                Column(modifier = Modifier
                    .clickable {
                        onItemClick(plugin)
                        selectedIndex = index
                    }
                    .border(
                        if (index == selectedIndex) 2.dp else 0.dp,
                        MaterialTheme.colors.primary
                    )
                    .weight(1f)) {
                    Text(plugin.displayName)
                    Text(plugin.packageName, style = small)
                }
            }
        })
    }
}

@Composable
fun PluginInstanceControllerUI() {
    // FIXME: maybe we should remove this hacky state variable
    var midiManagerInitializedState by remember { mutableStateOf(model.midiManagerInitialized) }
    var useMidi2ProtocolState by remember { mutableStateOf(model.useMidi2Protocol) }

    Row {
        if (midiManagerInitializedState)
            Button(modifier = Modifier.padding(2.dp),
                onClick = {
                    updater.terminateMidi()
                    midiManagerInitializedState = false
                }) {
                Text("Stop MIDI Service")
            }
        else
            Button(modifier = Modifier.padding(2.dp),
                onClick = {
                    updater.initializeMidi()
                    midiManagerInitializedState = true
                }) {
                Text("Start MIDI Service")
            }
    }
    Row {
        Checkbox(checked = useMidi2ProtocolState, onCheckedChange = { value ->
            updater.setMidi2Enabled(value)
            useMidi2ProtocolState = value
        })
        Text("Use MIDI 2.0 Protocol")
        Button(modifier = Modifier.padding(2.dp),
            onClick = { updater.playNote() }) {
            Text("Play")
        }

    }
}

@SuppressLint("SetJavaScriptEnabled")
@Composable
fun PluginWebUI() {
    Column {
        AndroidView(
            modifier = Modifier.padding(40.dp).border(2.dp, Color.Black),
            factory = { ctx: Context ->
                WebView(ctx).also { webView -> with(webView) {
                    val assetLoader = WebViewAssetLoader.Builder().addPathHandler(
                        "/assets/",
                        WebViewAssetLoader.AssetsPathHandler(ctx)
                    ).build()
                    webViewClient = object : WebViewClient() {
                        @RequiresApi(21)
                        override fun shouldInterceptRequest(
                            view: WebView,
                            request: WebResourceRequest
                        ): WebResourceResponse? {
                            return assetLoader.shouldInterceptRequest(request.url)
                        }
                    }
                    settings.javaScriptEnabled = true
                    addJavascriptInterface(AAPScriptInterface(), "AAPInterop")

                    var html = """<html>
<head>
<script type='text/javascript'>
</script>
</head>
<body
  onLoad='AAPInterop.onInitialize(); AAPInterop.onShow()'
  onUnload='AAPInterop.onHide();AAPInterop.onCleanup()'>
  <p>(parameters are read only so far)
  <table>
"""
                    for (port in model.instrument!!.ports) {
                        when (port.content) {
                            PortInformation.PORT_CONTENT_TYPE_AUDIO,
                            PortInformation.PORT_CONTENT_TYPE_MIDI,
                            PortInformation.PORT_CONTENT_TYPE_MIDI2 -> continue
                        }
                        html += """
  <tr>
    <th>${port.name}</th>
    <td>
      <input type='range' class='slider' id='port_${port.index}' min='${port.minimum}' max='${port.maximum}' value='${port.default}' step='${(port.maximum - port.minimum) / 20.0}' oninput='AAPInterop.write(${port.index}, this.value)' />
    </td>
  </tr>
  """
                    }
                    html += "</table></body></html>"

                    loadData(html, "text/html", null)
                }
            }
        })
    }
}

class AAPScriptInterface {
    @JavascriptInterface
    fun log(s: String) {
        println(s)
    }

    @JavascriptInterface
    fun onInitialize() {
        println("!!!!!!!!!!!!!!! onInitialize")
    }

    @JavascriptInterface
    fun onCleanup() {
        println("!!!!!!!!!!!!!!! onCleanup")
    }

    @JavascriptInterface
    fun onShow() {
        println("!!!!!!!!!!!!!!! onShow")
    }

    @JavascriptInterface
    fun onHide() {
        println("!!!!!!!!!!!!!!! onHide")
    }

    @JavascriptInterface
    fun onNotify(port: Int, data: ByteArray, offset: Int, length: Int) {
        println("!!!!!!!!!!!!!!! onNotify")
    }

    @JavascriptInterface
    fun listen(port: Int) {
    }

    @JavascriptInterface
    fun write(port: Int, value: String) {
        tmpBuffer.asFloatBuffer().put(0, value.toFloat())
        write(port, tmpBuffer.array(), 0, 4)
    }
    private val tmpBuffer: ByteBuffer = ByteBuffer.allocate(4)

    @JavascriptInterface
    fun write(port: Int, data: ByteArray, offset: Int, length: Int) {
        println("!!!!!!!!!!!!!!! write($port, ${ByteBuffer.wrap(data).asFloatBuffer().get(offset)})")
    }
}
