#include <stdio.h>
#include <dlfcn.h>

#if DYNAMIC_LOADER
typedef int(*generate_aap_metadata_func)(char* aapMetadataPath);
#else
extern "C" int generate_aap_metadata(const char* aapMetadataFullPath);
#endif

int main(int argc, char** argv)
{
#if DYNAMIC_LOADER
	if (argc < 3) {
		printf("USAGE: %s [executable-or-sharedlib] [aap_metadata.xml]\n", argv[0]);
		return 1;
	}
	auto dl = dlopen(argv[1], RTLD_LAZY | RTLD_LOCAL);
	if (dl == nullptr) {
		printf("Could not open binary file %s\n", argv[1]);
		return 3;
	}
	auto func = (generate_aap_metadata_func) dlsym(dl, "generate_aap_metadata");
	if (func == nullptr) {
		printf("Binary file %s does not contain `generate_aap_metadata`\n", argv[1]);
		return 3;
	}
	func(argv[2]);
	dlclose(dl);
#else
	if (argc < 2) {
		printf("USAGE: %s [aap_metadata.xml]\n", argv[0]);
		return 1;
	}
	generate_aap_metadata(argv[1]);
#endif
}
