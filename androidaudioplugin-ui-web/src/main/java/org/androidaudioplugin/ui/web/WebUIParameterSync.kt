package org.androidaudioplugin.ui.web

import android.os.Handler
import android.os.Looper
import android.webkit.WebView
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import org.androidaudioplugin.NativeLocalPluginInstance

class WebUIParameterSync(private val webView: WebView) {
    private val mainHandler = Handler(Looper.getMainLooper())
    private val pendingValues = linkedMapOf<Int, Double>()
    private var isInitialized = false

    @Synchronized
    fun onWebUiInitialized() {
        isInitialized = true
        installParameterValueTextSupportLocked()
        flushPendingValuesLocked()
    }

    @Synchronized
    fun onWebUiCleanedUp() {
        isInitialized = false
    }

    @Synchronized
    fun updateParameter(parameterId: Int, value: Double) {
        pendingValues[parameterId] = value
        if (isInitialized)
            flushPendingValuesLocked()
    }

    @Synchronized
    fun updateParameters(values: Map<Int, Double>) {
        values.forEach { (parameterId, value) -> pendingValues[parameterId] = value }
        if (isInitialized)
            flushPendingValuesLocked()
    }

    private fun flushPendingValuesLocked() {
        if (!isInitialized || pendingValues.isEmpty())
            return
        val updates = LinkedHashMap(pendingValues)
        pendingValues.clear()
        val payload = updates.entries.joinToString(
            prefix = "{",
            postfix = "}"
        ) { (parameterId, value) -> "\"$parameterId\":$value" }
        mainHandler.post {
            webView.evaluateJavascript(
                "window.AAPInteropHost && window.AAPInteropHost.updateParameters && window.AAPInteropHost.updateParameters($payload);",
                null
            )
        }
    }

    private fun installParameterValueTextSupportLocked() {
        mainHandler.post {
            webView.evaluateJavascript(
                """
                (function() {
                  if (window.__aapParameterValueSupportInstalled)
                    return;
                  window.__aapParameterValueSupportInstalled = true;

                  function formatValue(value) {
                    var num = Number(value);
                    if (!Number.isFinite(num))
                      return '';
                    if (Math.abs(num) >= 100 || Number.isInteger(num))
                      return num.toString();
                    return num.toFixed(3).replace(/\.?0+$/, '');
                  }

                  function ensureStyles() {
                    if (document.getElementById('aap-parameter-value-style'))
                      return;
                    var style = document.createElement('style');
                    style.id = 'aap-parameter-value-style';
                    style.textContent =
                      '.aap-parameter-value-wrapper{display:inline-flex;flex-direction:column;align-items:center;gap:4px;}' +
                      '.aap-parameter-value-label{min-height:1em;font-size:12px;line-height:1;color:#666;text-align:center;}';
                    document.head.appendChild(style);
                  }

                  function ensureLabel(knob) {
                    if (!knob)
                      return null;
                    if (!knob.__aapValueLabel) {
                      var wrapper = knob.parentElement;
                      if (!wrapper || !wrapper.classList || !wrapper.classList.contains('aap-parameter-value-wrapper')) {
                        wrapper = document.createElement('div');
                        wrapper.className = 'aap-parameter-value-wrapper';
                        knob.parentNode.insertBefore(wrapper, knob);
                        wrapper.appendChild(knob);
                      }
                      var label = document.createElement('div');
                      label.className = 'aap-parameter-value-label';
                      wrapper.appendChild(label);
                      knob.__aapValueLabel = label;
                    }
                    return knob.__aapValueLabel;
                  }

                  function updateKnob(knob, value) {
                    var label = ensureLabel(knob);
                    if (!label)
                      return;
                    if (value !== undefined) {
                      knob.value = value;
                      knob.setAttribute('value', value);
                      label.textContent = formatValue(value);
                      return;
                    }
                    label.textContent = formatValue(knob.value !== undefined ? knob.value : knob.getAttribute('value'));
                  }

                  function bindKnob(knob) {
                    if (!knob || knob.__aapValueLabelBound)
                      return;
                    knob.__aapValueLabelBound = true;
                    ensureLabel(knob);
                    updateKnob(knob);
                    knob.addEventListener('input', function() { updateKnob(knob); });
                    knob.addEventListener('change', function() { updateKnob(knob); });
                  }

                  function bindAllKnobs() {
                    ensureStyles();
                    Array.prototype.forEach.call(document.getElementsByTagName('webaudio-knob'), bindKnob);
                  }

                  var existingHost = window.AAPInteropHost || {};
                  var existingUpdateParameters = existingHost.updateParameters;
                  existingHost.updateParameters = function(values) {
                    if (existingUpdateParameters)
                      existingUpdateParameters(values);
                    Object.keys(values || {}).forEach(function(parameterId) {
                      var knobs = document.getElementsByTagName('webaudio-knob');
                      Array.prototype.forEach.call(knobs, function(knob) {
                        if (String(knob.aapParameterId) === String(parameterId) ||
                            String(knob.aapParameterIndex) === String(parameterId)) {
                          updateKnob(knob, values[parameterId]);
                        }
                      });
                    });
                  };
                  window.AAPInteropHost = existingHost;

                  bindAllKnobs();
                  new MutationObserver(bindAllKnobs).observe(document.body || document.documentElement, { childList: true, subtree: true });
                })();
                """.trimIndent(),
                null
            )
        }
    }
}

internal class LocalWebUIParameterPoller(
    private val instance: NativeLocalPluginInstance,
    private val parameterSync: WebUIParameterSync
) {
    private val outputBuffer = ByteBuffer.allocateDirect(8192).order(ByteOrder.nativeOrder())
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private var job: Job? = null

    fun start() {
        if (job != null)
            return
        job = scope.launch {
            while (isActive) {
                var sawUpdate = false
                while (isActive) {
                    outputBuffer.clear()
                    val bytesRead = instance.readGuiListenerMidi2Output(outputBuffer, outputBuffer.capacity())
                    if (bytesRead <= 0)
                        break
                    val updates = parseParameterUpdates(outputBuffer, bytesRead)
                    if (updates.isNotEmpty()) {
                        sawUpdate = true
                        parameterSync.updateParameters(updates)
                    }
                }
                delay(if (sawUpdate) 1 else 16)
            }
        }
    }

    fun close() {
        job?.cancel()
        job = null
        scope.cancel()
    }

    private fun parseParameterUpdates(buffer: ByteBuffer, bytesRead: Int): Map<Int, Double> {
        val updates = linkedMapOf<Int, Double>()
        buffer.position(0)
        buffer.limit(bytesRead)
        while (buffer.remaining() >= Int.SIZE_BYTES) {
            val offset = buffer.position()
            val word0 = buffer.int
            when ((word0 ushr 28) and 0xF) {
                4 -> {
                    if (buffer.remaining() < Int.SIZE_BYTES) {
                        buffer.position(bytesRead)
                        continue
                    }
                    val word1 = buffer.int
                    val status = (word0 ushr 16) and 0xF0
                    val channel = (word0 ushr 16) and 0xF
                    if (channel != 0)
                        continue
                    when (status) {
                        0x30 -> {
                            val parameterId = ((word0 ushr 8) and 0x7F) shl 7 or (word0 and 0x7F)
                            updates[parameterId] = Float.fromBits(word1).toDouble()
                        }
                        0xB0 -> {
                            val parameterId = (word0 ushr 8) and 0x7F
                            updates[parameterId] = Float.fromBits(word1).toDouble()
                        }
                    }
                }
                5 -> {
                    if (buffer.remaining() < Int.SIZE_BYTES * 3) {
                        buffer.position(bytesRead)
                        continue
                    }
                    val word1 = buffer.int
                    val word2 = buffer.int
                    val word3 = buffer.int
                    val channel = word1 and 0xF
                    if (channel == 0 && (word0 and 0xFF) == 0x7E && word1 ushr 8 == 0x7F0000)
                        updates[word2 and 0xFFFF] = Float.fromBits(word3).toDouble()
                }
                else -> {
                    val messageSize = when ((word0 ushr 28) and 0xF) {
                        0, 1, 2, 6, 7 -> Int.SIZE_BYTES
                        3, 4, 8, 9, 0xA -> Int.SIZE_BYTES * 2
                        0xB, 0xC -> Int.SIZE_BYTES * 3
                        else -> Int.SIZE_BYTES * 4
                    }
                    buffer.position((offset + messageSize).coerceAtMost(bytesRead))
                }
            }
        }
        return updates
    }
}
