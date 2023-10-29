#ifndef AAP_CORE_AAPXS_RUNTIME_H
#define AAP_CORE_AAPXS_RUNTIME_H

// The new 2023 version of AAPXS runtime - strongly typed.

#include <assert.h>
#include <cstdint>
#include <vector>
#include <future>
#include "../../unstable/aapxs-vnext.h"
#include "../../android-audio-plugin.h"

namespace aap {

    class AAPXSClientSerializer {
        void serializePluginRequest(AAPXSSerializationContext *context);
    };

    class AAPXSServiceSerializer {
        void serializePluginReply(AAPXSSerializationContext *context);
    };

    typedef struct AAPXSClientHandler {
        AAPXSProxyContextVNext
        (*as_plugin_proxy)(AAPXSClientHandler *handler, AAPXSClientSerializer *serializer);

        void (*on_host_invoked)(AAPXSClientHandler *handler, AAPXSClientSerializer *serializer,
                                AndroidAudioPluginHost *host, int32_t opcode);
    } AAPXSClientHandler;

    typedef struct AAPXSServiceHandler {
        AAPXSProxyContextVNext
        (*as_host_proxy)(AAPXSServiceHandler *handler, AAPXSServiceSerializer *serializer);

        void (*on_plugin_invoked)(AAPXSServiceHandler *handler, AAPXSServiceSerializer *serializer,
                                  AndroidAudioPlugin *plugin, int32_t opcode);
    } AAPXSServiceHandler;

    typedef struct AAPXSInstanceManager {
        void *host_context;

        AAPXSClientSerializer *(*get_client_serializer)();

        AAPXSServiceSerializer *(*get_service_serializer)();

        AAPXSClientHandler *(*get_client_handler)();

        AAPXSServiceHandler *(*get_service_handler)();
    } AAPXSInstanceManager;

    class AAPXSClientManager {
        AAPXSClientHandler* get(const char* uri);
    };

    // They are used for sure.

    template<typename T, typename R>
    struct WithPromise {
        T* context;
        std::promise<R>* promise;
    };

    class UridMapping {
        std::vector<uint8_t> urids{};
        std::vector<const char*> uris{};

    public:
        static const uint8_t UNMAPPED_URID = 0;

        UridMapping() {
            urids.resize(UINT8_MAX);
            uris.resize(UINT8_MAX);
        }

        uint8_t tryAdd(const char* uri) {
            assert(uris.size() < UINT8_MAX);
            auto existing = getUrid(uri);
            if (existing > 0)
                return existing;
            uris.emplace_back(uri);
            uint8_t urid = uris.size();
            urids.emplace_back(urid);
            return urid;
        }

        uint8_t getUrid(const char* uri) {
            // starts from 1, as 0 is "unmapped"
            for (size_t i = 1, n = uris.size(); i < n; i++)
                if (uri == uris[n])
                    return urids[i];
            for (size_t i = 1, n = uris.size(); i < n; i++)
                if (!strcmp(uri, uris[n]))
                    return urids[i];
            return UNMAPPED_URID;
        }

        const char* getUri(uint8_t urid) {
            // starts from 1, as 0 is "unmapped"
            for (size_t i = 1, n = urids.size(); i < n; i++)
                if (urid == urids[n])
                    return uris[i];
            return nullptr;
        }
    };

    template <typename T>
    class AAPXSFeatureMapVNext {
        UridMapping urid_mapping{};
        std::vector<T> items{};
        bool frozen{false};

    public:
        AAPXSFeatureMapVNext() {
            items.resize(UINT8_MAX);
        }
        virtual ~AAPXSFeatureMapVNext() {}

        UridMapping* getUridMapping() { return &urid_mapping; }

        inline void freezeFeatureSet() { frozen = true; }
        inline bool isFrozen() { return frozen; }

        void add(T feature, const char* uri) {
            uint8_t urid = urid_mapping.tryAdd(uri);
            items[urid] = feature;
        }
        T& getByUri(const char* uri) {
            uint8_t urid = urid_mapping.getUrid(uri);
            return items[urid];
        }
        T& getByUrid(uint8_t urid) {
            return items[urid];
        }

        using container=std::vector<T>;
        using iterator=typename container::iterator;
        using const_iterator=typename container::const_iterator;

        iterator begin() { return items.begin(); }
        iterator end() { return items.end(); }
        const_iterator begin() const { return items.begin(); }
        const_iterator end() const { return items.end(); }
    };

    class AAPXSInitiatorInstanceMap : public AAPXSFeatureMapVNext<AAPXSInitiatorInstance> {};
    class AAPXSRecipientInstanceMap : public AAPXSFeatureMapVNext<AAPXSRecipientInstance> {};

    class AAPXSClientFeatureRegistry;
    class AAPXSServiceFeatureRegistry;

    class AAPXSClientInstanceManagerVNext {
        //AAPXSFeatureRegistry* registry;
        AAPXSInitiatorInstanceMap initiators{};
        AAPXSRecipientInstanceMap recipients{};

    public:
        AAPXSClientInstanceManagerVNext(AAPXSClientFeatureRegistry* registry);

        inline void addInitiator(AAPXSInitiatorInstance initiator, const char* uri) { initiators.add(initiator, uri); }
        inline void addRecipient(AAPXSRecipientInstance recipient, const char* uri) { recipients.add(recipient, uri); }

        inline AAPXSInitiatorInstance& getPluginAAPXSByUri(const char* uri) { return initiators.getByUri(uri); }
        inline AAPXSInitiatorInstance& getPluginAAPXSByUrid(uint8_t urid) { return initiators.getByUrid(urid); };
        inline AAPXSRecipientInstance& getHostAAPXSByUri(const char* uri) { return recipients.getByUri(uri); }
        inline AAPXSRecipientInstance& getHostAAPXSByUrid(uint8_t urid) { return recipients.getByUrid(urid); };
    };

    class AAPXSServiceInstanceManagerVNext {
        AAPXSInitiatorInstanceMap initiators{};
        AAPXSRecipientInstanceMap recipients{};

    public:
        AAPXSServiceInstanceManagerVNext(AAPXSServiceFeatureRegistry* registry);

        void addInitiator(AAPXSInitiatorInstance initiator, const char* uri) { initiators.add(initiator, uri); }
        void addRecipient(AAPXSRecipientInstance recipient, const char* uri) { recipients.add(recipient, uri); }

        AAPXSRecipientInstance& getPluginAAPXSByUri(const char* uri) { return recipients.getByUri(uri); }
        AAPXSRecipientInstance& getPluginAAPXSByUrid(uint8_t urid) { return recipients.getByUrid(urid); };
        AAPXSInitiatorInstance& getHostAAPXSByUri(const char* uri) { return initiators.getByUri(uri); }
        AAPXSInitiatorInstance& getHostAAPXSByUrid(uint8_t urid) { return initiators.getByUrid(urid); };
    };

    class AAPXSClientFeatureRegistry : public AAPXSFeatureMapVNext<AAPXSFeatureVNext*> {
    public:
        virtual void setupClientInstances(aap::AAPXSClientInstanceManagerVNext *client, AAPXSSerializationContext* serialization) = 0;
    };

    class AAPXSServiceFeatureRegistry : public AAPXSFeatureMapVNext<AAPXSFeatureVNext*> {
    public:
        virtual void setupServiceInstances(aap::AAPXSServiceInstanceManagerVNext *client, AAPXSSerializationContext* serialization) = 0;
    };
}

#endif //AAP_CORE_AAPXS_RUNTIME_H
