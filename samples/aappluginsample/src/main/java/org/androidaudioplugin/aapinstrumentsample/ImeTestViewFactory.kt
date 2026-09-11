package org.androidaudioplugin.aapinstrumentsample

import android.content.Context
import android.graphics.Color
import android.text.InputType
import android.util.Size
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import org.androidaudioplugin.AudioPluginViewFactory

/**
 * An opt-in Native UI regression fixture for IME (software keyboard) support.
 *
 * It deliberately uses plain platform views instead of Compose, so that what it exercises is
 * the embedded-window IME path itself and not Compose's text input implementation.
 *
 * To use it, point `gui:ui-view-factory` at this class in `res/xml/aap_metadata.xml`, then open
 * the plugin through a host's "Show Native UI" and check that tapping a field raises the
 * software keyboard and that typed text lands in the field.
 *
 * Note the content-size convention in AudioPluginViewService: the first child of the root view
 * is treated as the content that reports resize events (to accommodate JUCE). The fixture
 * therefore wraps everything in a single full-size child.
 */
class ImeTestViewFactory : AudioPluginViewFactory() {
    companion object {
        private const val WIDTH = 600
        private const val HEIGHT = 400
    }

    override fun getPreferredSize(context: Context, pluginId: String, instanceId: Int): Size =
        Size(WIDTH, HEIGHT)

    override fun createView(context: Context, pluginId: String, instanceId: Int): View {
        fun label(caption: String) = TextView(context).apply {
            text = caption
            setTextColor(Color.WHITE)
        }

        fun field(hintText: String, type: Int) = EditText(context).apply {
            inputType = type
            hint = hintText
            setTextColor(Color.WHITE)
            setHintTextColor(Color.LTGRAY)
            setBackgroundColor(Color.DKGRAY)
        }

        val matchWrap = { LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT) }

        val content = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(16, 16, 16, 16)
            // opaque, so the fixture is legible over whatever the host draws behind it
            setBackgroundColor(Color.BLACK)

            addView(label("IME test fixture (plain platform views)"))
            addView(label("Single-line:"))
            addView(field("tap me, the keyboard should appear", InputType.TYPE_CLASS_TEXT), matchWrap())
            addView(label("Multi-line:"))
            addView(field("multi-line input",
                InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE).apply { setLines(3) },
                matchWrap())
        }

        return FrameLayout(context).apply {
            addView(content, FrameLayout.LayoutParams(WIDTH, HEIGHT))
        }
    }
}
