package org.androidaudioplugin.midideviceservice

import android.content.Context
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import java.io.ByteArrayOutputStream
import java.io.InputStreamReader
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class WebUIHelper {
    companion object {

        fun getUIZipArchive(ctx: Context, pluginId: String) =
            getUIZipArchive(ctx, AudioPluginHostHelper.queryAudioPlugins(ctx).first { p -> p.pluginId == pluginId })

        fun getUIZipArchive(ctx: Context, plugin: PluginInformation) : ByteArray {
            val ms = ByteArrayOutputStream()
            val os = ZipOutputStream(ms)
            os.writer().use {w ->
                for (s in arrayOf("webcomponents-lite.js", "webaudio-controls.js")) {
                    os.putNextEntry(ZipEntry(s))
                    ctx.assets.open("web/$s").use {
                        InputStreamReader(it).use {
                            w.write(it.readText())
                        }
                    }
                    w.flush()
                }
                for (s in arrayOf("bright_life.png")) {
                    os.putNextEntry(ZipEntry(s))
                    ctx.assets.open("web/$s").use {
                        it.copyTo(os)
                    }
                    w.flush()
                }
                os.putNextEntry(ZipEntry("index.html"))
                w.write(getHtml(plugin))
            }
            return ms.toByteArray()
        }

        fun getHtml(plugin: PluginInformation): String {
            var html = """<html>
<head>
<script type='text/javascript'><!--

  function initialize() {
    for (el of document.getElementsByTagName('webaudio-knob')) {
      el.addEventListener('input', function(e) {
        sendInput(e.target.getAttribute('index_'), e.target.value);
      });
    }
    AAPInterop.onInitialize();
    AAPInterop.onShow();
  }
  
  function terminate() {
    AAPInterop.onHide();
    AAPInterop.onCleanup();
  }

  function sendInput(i, v) {
    AAPInterop.write(i, v);
  }
//--></script>
<script type='text/javascript' src='webcoomponents-lite.js'></script>
<script type='text/javascript' src='webaudio-controls.js'></script>
</head>
<body onLoad='initialize()' onUnload='terminate()'>
  <p>(parameters are read only so far)
  <table>
"""
            for (port in plugin.ports) {
                when (port.content) {
                    PortInformation.PORT_CONTENT_TYPE_AUDIO,
                    PortInformation.PORT_CONTENT_TYPE_MIDI,
                    PortInformation.PORT_CONTENT_TYPE_MIDI2 -> continue
                }
                html += """
  <tr>
    <th>${port.name}</th>
    <td>
      <!-- input type='range' class='slider' id='port_${port.index}' min='${port.minimum}' max='${port.maximum}' value='${port.default}' step='${(port.maximum - port.minimum) / 20.0}' oninput='sendInput(${port.index}, this.value)' / -->
      <webaudio-knob min='${port.minimum}' max='${port.maximum}' value='${port.default}' step='${(port.maximum - port.minimum) / 20.0}' index_='${port.index}' src='bright_life.png' />
    </td>
  </tr>
  """
            }
            html += "</table></body></html>"

            println(html)
            return html
        }
    }
}

