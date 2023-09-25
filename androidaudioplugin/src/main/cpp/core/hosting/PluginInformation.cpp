#include <aap/core/plugin-information.h>


aap::PluginInformation::PluginInformation(bool isOutProcess, const char* pluginPackageName,
                                     const char* pluginLocalName, const char* displayName,
                                     const char* developerName, const char* versionString,
                                     const char* pluginID, const char* sharedLibraryFilename,
                                     const char* libraryEntrypoint, const char* metadataFullPath,
                                     const char* primaryCategory, const char* uiViewFactory,
                                     const char* uiActivity, const char* uiWeb)
        : is_out_process(isOutProcess),
          plugin_package_name(pluginPackageName),
          plugin_local_name(pluginLocalName),
          display_name(displayName),
          developer_name(developerName),
          version(versionString),
          shared_library_filename(sharedLibraryFilename),
          library_entrypoint(libraryEntrypoint),
          plugin_id(pluginID),
          metadata_full_path(metadataFullPath),
          primary_category(primaryCategory),
          ui_view_factory(uiViewFactory),
          ui_activity(uiActivity),
          ui_web(uiWeb)
{
    struct tm epoch{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    last_info_updated_unixtime_milliseconds = (int64_t) (1000.0 * difftime(time(nullptr), mktime(&epoch)));

    char *cp;
    size_t len = (size_t) snprintf(nullptr, 0, "%s+%s+%s", display_name.c_str(), plugin_id.c_str(), version.c_str());
    cp = (char*) calloc(len, 1);
    snprintf(cp, len, "%s+%s+%s", display_name.c_str(), plugin_id.c_str(), version.c_str());
    identifier_string = cp;
    free(cp);
}

aap::PluginExtensionInformation aap::PluginInformation::getExtension(const char *uri) const {
    for (int i = 0, n = getNumExtensions(); i < n; i++) {
        auto ext = getExtension(i);
        if (strcmp(ext.uri.c_str(), uri) == 0)
            return ext;
    }
    return {};
}

