/*
 * 
 * aap-import-lv2-metadata: generates helper files for LV2AudioPluginService
 * 
 * It generates:
 * 
 * - src/res/xml/aap_metadata.xml (default) : AAP metadata XML file.
 * - src/java/LV2Plugins.java : contains local LV2 path to plugin mappings,
 *   which is used to initialize LV2 paths.
 * 
 */

#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
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
char* escape_xml(const char* s);


LilvWorld *world;

char* stringpool[4096];
int stringpool_entry = 0;

int main(int argc, const char **argv)
{
	if (argc < 1) {
		fprintf (stderr, "Usage: %s [lv2-dir] [res-xml-dir]\n", argv[0]);
		return 1;
	}
	
	const char* lv2dirName = argc < 2 ?  "lv2" : argv[1];
	const char* xmldir = argc < 3 ? "res/xml" : argv[2];
	
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

	char* lv2realpath = realpath(lv2dirName, NULL);
	fprintf(stderr, "LV2 directory: %s\n", lv2realpath);
#if 1
	DIR *lv2dir = opendir(lv2dirName);
	if (!lv2dir) {
		fprintf(stderr, "Directory %s cannot be opened.\n", lv2dirName);
		return 3;
	}
	
	dirent *ent;
	while ((ent = readdir(lv2dir)) != NULL) {
		if (!strcmp(ent->d_name, "."))
			continue;
		if (!strcmp(ent->d_name, ".."))
			continue;
		
		int ttllen = snprintf(NULL, 0, "%s/%s/manifest.ttl", lv2realpath, ent->d_name) + 1;
		char* ttlfile = (char*) malloc(ttllen);
		stringpool[stringpool_entry++] = ttlfile;
		sprintf(ttlfile, "%s/%s/manifest.ttl", lv2realpath, ent->d_name);
		struct stat st;
		if(stat(ttlfile, &st)) {
			fprintf(stderr, "%s is not found.\n", ttlfile);
			continue;
		}
		fprintf(stderr, "Loading from %s\n", ttlfile);
		auto filePathNode = lilv_new_file_uri(world, NULL, ttlfile);
		
		lilv_world_load_bundle(world, filePathNode);
		
		fprintf (stderr, "Loaded bundle. Dumping all plugins from there.\n");
	}
	closedir(lv2dir);
#else	
	setenv("LV2_PATH", lv2realpath, 1);
	lilv_world_load_all(world);
#endif

	fprintf(stderr, "all plugins loaded\n");
	
	const LilvPlugins *plugins = lilv_world_get_all_plugins(world);
	
	int numbered_files = 0;
	
	char *xmlFilename = (char*) malloc(snprintf(NULL, 0, "%s/aap_metadata.xml", xmldir));
	sprintf(xmlFilename, "%s/aap_metadata.xml", xmldir);
	fprintf(stderr, "Writing metadata file %s\n", xmlFilename);
	FILE *xmlFP = fopen(xmlFilename, "w+");
	if (!xmlFP) {
		fprintf(stderr, "Failed to create XML output file: %s\n", xmlFilename);
		return 1;
	}
	
	fprintf(xmlFP, "<plugins>\n");

	int numPlugins = lilv_plugins_size(plugins);
	char **pluginLv2Dirs = (char **) calloc(sizeof(char*) * numPlugins, 1);
	int numPluginDirEntries = 0;

	for (auto i = lilv_plugins_begin(plugins); !lilv_plugins_is_end(plugins, i); i = lilv_plugins_next(plugins, i)) {
		
		const LilvPlugin *plugin = lilv_plugins_get(plugins, i);
		const char *name = lilv_node_as_string(lilv_plugin_get_name(plugin));
		const LilvNode *author = lilv_plugin_get_author_name(plugin);
		const LilvNode *manufacturer = lilv_plugin_get_project(plugin);

		char *bundle_path = strdup(lilv_file_uri_parse(lilv_node_as_uri(lilv_plugin_get_bundle_uri(plugin)), NULL));
		if (!bundle_path) {
			fprintf(stderr, "Failed to retrieve the plugin bundle path: %s\n", bundle_path);
			continue;
		}
		char *plugin_lv2dir = bundle_path;
		*strrchr(bundle_path, '/') = 0; // "/foo/bar/lv2/some.lv2"{/manifest.tll -> stripped}
		plugin_lv2dir = strrchr(plugin_lv2dir, '/'); // "/some.lv2"
		if (!plugin_lv2dir) {
			fprintf(stderr, "The bundle path did not meet the plugin path premise (/some/path/to/lv2/some.lv2/manifest.ttl): %s\n", plugin_lv2dir);
			free(bundle_path);
			continue;
		}
		plugin_lv2dir++;
		plugin_lv2dir = strdup(plugin_lv2dir);
		free(bundle_path);

		fprintf(xmlFP, "  <plugin backend=\"LV2\" name=\"%s\" category=\"%s\" author=\"%s\" manufacturer=\"%s\" unique-id=\"lv2:%s\" library=\"libandroidaudioplugin-lv2.so\" entrypoint=\"GetAndroidAudioPluginFactoryLV2Bridge\" assets=\"/lv2/%s/\">\n    <ports>\n",
			name,
			/* FIXME: this categorization is super hacky */
			is_plugin_instrument(plugin) ? "Instrument" : "Effect",
			author != NULL ? escape_xml(lilv_node_as_string(author)) : "",
			manufacturer != NULL ? escape_xml(lilv_node_as_string(manufacturer)) : "",
			escape_xml(lilv_node_as_uri(lilv_plugin_get_uri(plugin))),
			escape_xml(plugin_lv2dir)
			);
		
		for (int p = 0; p < lilv_plugin_get_num_ports(plugin); p++) {
			auto port = lilv_plugin_get_port_by_index(plugin, p);
			
			LilvNode *defNode, *minNode, *maxNode;
			lilv_port_get_range(plugin, port, &defNode, &minNode, &maxNode);
			char def[1024], min[1024], max[1024];
			if (defNode != nullptr) std::snprintf(def, 1024, "default=\"%f\"", lilv_node_as_float(defNode)); else def[0] = 0;
			if (minNode != nullptr) std::snprintf(min, 1024, "minimum=\"%f\"", lilv_node_as_float(minNode)); else min[0] = 0;
			if (maxNode != nullptr) std::snprintf(max, 1024, "maximum=\"%f\"", lilv_node_as_float(maxNode)); else max[0] = 0;
			
			fprintf(xmlFP, "      <port direction=\"%s\" %s %s %s content=\"%s\" name=\"%s\" />\n",
				IS_INPUT_PORT(plugin, port) ? "input" : IS_OUTPUT_PORT(plugin, port) ? "output" : "",
				def, min, max,
				lilv_port_supports_event(plugin, port, midi_event_uri_node) ? "midi" : IS_AUDIO_PORT(plugin, port) ? "audio" : "other",
				escape_xml(lilv_node_as_string(lilv_port_get_name(plugin, port))));
			if(defNode) lilv_node_free(defNode);
			if(minNode) lilv_node_free(minNode);
			if(maxNode) lilv_node_free(maxNode);
		}
		fprintf(xmlFP, "    </ports>\n  </plugin>\n");	

		for(int i = 0; i < numPlugins; i++) {
			if(!pluginLv2Dirs[i]) {
				pluginLv2Dirs[i] = plugin_lv2dir;
				numPluginDirEntries++;
				break;
			}
			else if(!strcmp(pluginLv2Dirs[i], plugin_lv2dir))
				break;
		}
	}
	
	fprintf(xmlFP, "</plugins>\n");
	fclose(xmlFP);
	free(xmlFilename);
	
	lilv_world_free(world);
	
	for(int i = 0; i < stringpool_entry; i++)
		free(stringpool[i]);
	free(lv2realpath);
	
	fprintf (stderr, "done.\n");
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

char* escape_xml(const char* s)
{
	auto ret = strdup(s);
	for (int i = 0; ret[i]; i++)
		switch (ret[i]) {
		case '<':
		case '>':
		case '&':
		case '"':
		case '\'':
			ret[i] = '_';
			break;
		}
	stringpool[stringpool_entry++] = ret;
	return ret;
}
