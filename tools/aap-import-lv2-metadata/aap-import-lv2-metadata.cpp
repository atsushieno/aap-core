
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <serd/serd.h>
#include <sord/sord.h>
#include <lilv/lilv.h>
/* FIXME: this is super hacky */
#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"

#define RDF__A LILV_NS_RDF "type"

LilvNode
	*rdf_a_uri_node,
	*atom_port_uri_node,
	*atom_supports_uri_node,
	*midi_event_uri_node,
	*instrument_plugin_uri_node,
	*audio_port_uri_node,
	/* *control_port_uri_node, */
	*cv_port_uri_node,
	*input_port_uri_node,
	*output_port_uri_node;


#define PORTCHECKER_SINGLE(_name_,_type_) inline bool _name_ (const LilvPlugin* plugin, const LilvPort* port) { return lilv_port_is_a (plugin, port, _type_); }
#define PORTCHECKER_AND(_name_,_cond1_,_cond2_) inline bool _name_ (const LilvPlugin* plugin, const LilvPort* port) { return _cond1_ (plugin, port) && _cond2_ (plugin, port); }

PORTCHECKER_SINGLE (IS_AUDIO_PORT, audio_port_uri_node)
PORTCHECKER_SINGLE (IS_INPUT_PORT, input_port_uri_node)
PORTCHECKER_SINGLE (IS_OUTPUT_PORT, output_port_uri_node)
/*PORTCHECKER_SINGLE (IS_CONTROL_PORT, control_port_uri_node)*/
PORTCHECKER_SINGLE (IS_CV_PORT, cv_port_uri_node)
PORTCHECKER_SINGLE (IS_ATOM_PORT, atom_port_uri_node)
PORTCHECKER_AND (IS_AUDIO_IN, IS_AUDIO_PORT, IS_INPUT_PORT)
PORTCHECKER_AND (IS_AUDIO_OUT, IS_AUDIO_PORT, IS_OUTPUT_PORT)
/*PORTCHECKER_AND (IS_CONTROL_IN, IS_CONTROL_PORT, IS_INPUT_PORT)
PORTCHECKER_AND (IS_CONTROL_OUT, IS_CONTROL_PORT, IS_OUTPUT_PORT)*/

bool is_plugin_instrument(const LilvPlugin* plugin);

LilvWorld *world;

int main(int argc, char **argv)
{
	if (argc <= 2) {
		fprintf (stderr, "Usage: %s [input-ttlfile] [output-xmlfile]\n", argv[0]);
		return 1;
	}
	
	char* ttlfile = strcat(realpath(argv[1], NULL), "/manifest.ttl");
	fprintf(stderr, "Loading from %s\n", ttlfile);
	char* xmlfile = argv[2];
	fprintf(stderr, "Saving to %s\n", xmlfile);
	auto xmlFP = fopen(xmlfile, "w");
	
	world = lilv_world_new();

	rdf_a_uri_node = lilv_new_uri(world, RDF__A);
    atom_port_uri_node = lilv_new_uri (world, LV2_CORE__AudioPort);
    atom_supports_uri_node = lilv_new_uri (world, LV2_ATOM__supports);
    midi_event_uri_node = lilv_new_uri (world, LV2_MIDI__MidiEvent);
    instrument_plugin_uri_node = lilv_new_uri(world, LV2_CORE__InstrumentPlugin);
    audio_port_uri_node = lilv_new_uri (world, LV2_CORE__AudioPort);
    /*control_port_uri_node = lilv_new_uri (world, LV2_CORE__ControlPort);*/
    input_port_uri_node = lilv_new_uri (world, LV2_CORE__InputPort);
    output_port_uri_node = lilv_new_uri (world, LV2_CORE__OutputPort);

	auto filePathNode = lilv_new_file_uri(world, NULL, ttlfile);
	
	lilv_world_load_bundle(world, filePathNode);
	
	fprintf (stderr, "Loaded bundle. Dumping all plugins from there.\n");
	
	auto plugins = lilv_world_get_all_plugins(world);
	
	for (auto i = lilv_plugins_begin(plugins); !lilv_plugins_is_end(plugins, i); i = lilv_plugins_next(plugins, i)) {
		const auto plugin = lilv_plugins_get(plugins, i);
		auto author = lilv_plugin_get_author_name(plugin);
		auto manufacturer = lilv_plugin_get_project(plugin);
		
		fprintf(xmlFP, "<plugin backend=\"LV2\" name=\"%s\" category=\"%s\" author=\"%s\" manufacturer=\"%s\" unique-id=\"lv2:%s\">\n  <ports>\n",
			lilv_node_as_string(lilv_plugin_get_name(plugin)),
			/* FIXME: this categorization is super hacky */
			is_plugin_instrument(plugin) ? "Instrument" : "Effect",
			author != NULL ? lilv_node_as_string(author) : "",
			manufacturer != NULL ? lilv_node_as_string(manufacturer) : "",
			lilv_node_as_uri(lilv_plugin_get_uri(plugin))
			);
		
		for (int p = 0; p < lilv_plugin_get_num_ports(plugin); p++) {
			auto port = lilv_plugin_get_port_by_index(plugin, p);
			
			fprintf(xmlFP, "    <port direction=\"%s\" content=\"%s\" name=\"%s\" />\n",
				IS_INPUT_PORT(plugin, port) ? "input" : IS_OUTPUT_PORT(plugin, port) ? "output" : "",
				lilv_port_supports_event(plugin, port, midi_event_uri_node) ? "midi" : IS_AUDIO_PORT(plugin, port) ? "audio" : "other",
				lilv_node_as_string(lilv_port_get_name(plugin, port)));
		}
		fprintf(xmlFP, "  </ports>\n</plugin>\n");
	}
	
	lilv_world_free(world);
	

	fprintf (stderr, "done.\n");
	free(ttlfile);
	return 0;
}

bool is_plugin_instrument(const LilvPlugin* plugin)
{
	/* If the plugin is `a lv2:InstrumentPlugin` then true. */
	auto nodes = lilv_world_find_nodes(world, lilv_plugin_get_uri(plugin), rdf_a_uri_node, NULL);
	LILV_FOREACH(nodes, n, nodes) {
		auto node = lilv_nodes_get(nodes, n);
		if(lilv_node_equals(node, instrument_plugin_uri_node))
			return true;
	}
	return false;
}
