#ifndef AAP_CORE_AAPXS_RUNTIME_H
#define AAP_CORE_AAPXS_RUNTIME_H

// AAPXS v2 runtime - strongly typed.

#include <assert.h>
#include <cstdint>
#include <vector>
#include <future>
#include "../../unstable/aapxs-vnext.h"
#include "../../android-audio-plugin.h"

namespace aap {
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

    class AAPClientDispatcher {
        //AAPXSFeatureRegistry* registry;
        AAPXSInitiatorInstanceMap initiators{};
        AAPXSRecipientInstanceMap recipients{};

    public:
        AAPClientDispatcher(AAPXSClientFeatureRegistry* registry);

        inline void addInitiator(AAPXSInitiatorInstance initiator, const char* uri) { initiators.add(initiator, uri); }
        inline void addRecipient(AAPXSRecipientInstance recipient, const char* uri) { recipients.add(recipient, uri); }

        inline AAPXSInitiatorInstance& getPluginAAPXSByUri(const char* uri) { return initiators.getByUri(uri); }
        inline AAPXSInitiatorInstance& getPluginAAPXSByUrid(uint8_t urid) { return initiators.getByUrid(urid); };
        inline AAPXSRecipientInstance& getHostAAPXSByUri(const char* uri) { return recipients.getByUri(uri); }
        inline AAPXSRecipientInstance& getHostAAPXSByUrid(uint8_t urid) { return recipients.getByUrid(urid); };

        void sendExtensionRequest(const char* uri, int32_t opcode, void *data, int32_t dataSize, uint32_t newRequestId);
        void processExtensionReply(const char *uri, int32_t opcode, void* data, int32_t dataSize, uint32_t requestId);
        void processHostExtensionRequest(const char *uri, int32_t opcode, void* data, int32_t dataSize, uint32_t requestId);
        void sendHostExtensionReply(const char *uri, int32_t opcode, void *data, int32_t dataSize, uint32_t requestId);
    };

    class AAPServiceDispatcher {
        AAPXSInitiatorInstanceMap initiators{};
        AAPXSRecipientInstanceMap recipients{};

    public:
        AAPServiceDispatcher(AAPXSServiceFeatureRegistry* registry);

        void addInitiator(AAPXSInitiatorInstance initiator, const char* uri) { initiators.add(initiator, uri); }
        void addRecipient(AAPXSRecipientInstance recipient, const char* uri) { recipients.add(recipient, uri); }

        AAPXSRecipientInstance& getPluginAAPXSByUri(const char* uri) { return recipients.getByUri(uri); }
        AAPXSRecipientInstance& getPluginAAPXSByUrid(uint8_t urid) { return recipients.getByUrid(urid); };
        AAPXSInitiatorInstance& getHostAAPXSByUri(const char* uri) { return initiators.getByUri(uri); }
        AAPXSInitiatorInstance& getHostAAPXSByUrid(uint8_t urid) { return initiators.getByUrid(urid); };

        void processExtensionRequest(const char *uri, int32_t opcode, uint32_t requestId);
        void sendHostExtensionRequest(const char* uri, int32_t opcode, void *data, int32_t dataSize, uint32_t newRequestId);
        void processHostExtensionReply(const char *uri, int32_t opcode, uint32_t requestId);
        void sendExtensionReply(const char* uri, int32_t opcode, void* data, int32_t dataSize, uint32_t requestId);
    };

    class AAPXSClientFeatureRegistry : public AAPXSFeatureMapVNext<AAPXSFeatureVNext*> {
    public:
        virtual void setupClientInstances(aap::AAPClientDispatcher *client, AAPXSSerializationContext* serialization) = 0;
    };

    class AAPXSServiceFeatureRegistry : public AAPXSFeatureMapVNext<AAPXSFeatureVNext*> {
    public:
        virtual void setupServiceInstances(aap::AAPServiceDispatcher *client, AAPXSSerializationContext* serialization) = 0;
    };
}

#endif //AAP_CORE_AAPXS_RUNTIME_H
