
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
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
	if (argc < 1) {
		fprintf (stderr, "Usage: %s [input-lv2dir] ([output-xmldir] [manifest-fragment.xml])\n", argv[0]);
		return 1;
	}
	
	const char* lv2dirName = argc < 2 ?  "lv2" : argv[1];

	char* xmldir = argc < 3 ? (char*) "res/xml" : argv[2];
	char* manifestfragfile = argc < 4 ? (char*) "manifest-fragment.xml" : argv[3];
	fprintf(stderr, "manifest fragment: %s\n", manifestfragfile);
	
	// FIXME: should we support Windows... ? They have WSL now.
	char *cmd = (char*) malloc(snprintf(NULL, 0, "mkdir -p %s", xmldir) + 1);
	sprintf(cmd, "mkdir -p %s", xmldir);
	fprintf(stderr, "run command %s\n", cmd);
	system(cmd);
	free(cmd);
	
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

	FILE *androidManifestFP = fopen(manifestfragfile, "w+");
	if (!androidManifestFP) {
		fprintf(stderr, "Failed to create %s\n", manifestfragfile);
		return 2;
	}

	char* lv2realpath = realpath(lv2dirName, NULL);
	fprintf(stderr, "LV2 directory: %s\n", lv2realpath);
#if 1
	DIR *lv2dir = opendir(lv2dirName);
	if (!lv2dir) {
		fprintf(stderr, "Directory %s cannot be opened.\n", lv2dirName);
		return 3;
	}
	char* filenames[4096];
	
	dirent *ent;
	int n_files;
	while ((ent = readdir(lv2dir)) != NULL) {
		if (!strcmp(ent->d_name, "."))
			continue;
		if (!strcmp(ent->d_name, ".."))
			continue;
		
		fprintf(stderr, "DIRECTORY: %s\n", ent->d_name);
		int ttllen = snprintf(NULL, 0, "%s/%s/manifest.ttl", lv2realpath, ent->d_name) + 1;
		char* ttlfile = (char*) malloc(ttllen);
		filenames[n_files++] = ttlfile;
		sprintf(ttlfile, "%s/%s/manifest.ttl", lv2realpath, ent->d_name);
		struct stat st;
		if(stat(ttlfile, &st)) {
			fprintf(stderr, "%s is not found.\n", ttlfile);
			continue;
		}
		fprintf(stderr, "Loading from %s %d\n", ttlfile, strlen(ttlfile));
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
	
	fprintf(androidManifestFP, "        <!-- generated manifest fragment by \"aap-import-lv2-metadata\" tool -->\n");

	for (auto i = lilv_plugins_begin(plugins); !lilv_plugins_is_end(plugins, i); i = lilv_plugins_next(plugins, i)) {
		
		const LilvPlugin *plugin = lilv_plugins_get(plugins, i);
		const char *name = lilv_node_as_string(lilv_plugin_get_name(plugin));
		const LilvNode *author = lilv_plugin_get_author_name(plugin);
		const LilvNode *manufacturer = lilv_plugin_get_project(plugin);

		fprintf(androidManifestFP, "        <service android:name=\".AudioPluginService\" android:label=\"%s\">\n", name);
		fprintf(androidManifestFP, "            <intent-filter><action android:name=\"org.androidaudiopluginframework.AudioPluginService\" /></intent-filter>\n");
		fprintf(androidManifestFP, "            <meta-data android:name=\"org.androidaudiopluginframework.AudioPluginService\" android:resource=\"@xml/metadata%d\" />\n", numbered_files);
		fprintf(androidManifestFP, "        </service>\n");

		char *xmlFilename = (char*) malloc(snprintf(NULL, 0, "%s/%s", xmldir, name) + 1);
		sprintf(xmlFilename, "%s/metadata%d.xml", xmldir, numbered_files++);
		fprintf(stderr, "Writing metadata file %s\n", xmlFilename);
		FILE *xmlFP = fopen(xmlFilename, "w+");
		
		fprintf(xmlFP, "<plugin backend=\"LV2\" name=\"%s\" category=\"%s\" author=\"%s\" manufacturer=\"%s\" unique-id=\"lv2:%s\">\n  <ports>\n",
			name,
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
		
		fclose(xmlFP);
		free(xmlFilename);
	}
	
	fclose(androidManifestFP);
	
	lilv_world_free(world);
	
	for(int i = 0; i < n_files; i++)
		free(filenames[i]);
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
