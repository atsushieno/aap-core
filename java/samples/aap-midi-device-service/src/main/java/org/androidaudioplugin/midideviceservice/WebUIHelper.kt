package org.androidaudioplugin.midideviceservice

import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation

class WebUIHelper {
    companion object {
        fun getHtml(plugin: PluginInformation): String {
            var html = """<html>
<head>
<script type='text/javascript'><!--

  function initialize() {
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
      <input type='range' class='slider' id='port_${port.index}' min='${port.minimum}' max='${port.maximum}' value='${port.default}' step='${(port.maximum - port.minimum) / 20.0}' oninput='sendInput(${port.index}, this.value)' />
    </td>
  </tr>
  """
            }
            html += "</table></body></html>"

            return html
        }
    }
}

