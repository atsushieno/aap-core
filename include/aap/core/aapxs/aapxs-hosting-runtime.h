#ifndef AAP_CORE_AAPXS_HOSTING_RUNTIME_H
#define AAP_CORE_AAPXS_HOSTING_RUNTIME_H

// AAPXS v2 runtime - strongly typed.

#include <cstdint>
#include <vector>
#include <map>
#include <future>
#include "aap/aapxs.h"
#include "../../android-audio-plugin.h"
#include "aap/unstable/utility.h"

namespace aap::xs {
    /**
     * Implements URI-to-int mappings for RT-safe URI indication, similar to LV2 URID.
     *
     * In this version, the value range is 1..255.
     *
     * The integer `0` is reserved as UNMAPPED (used when the mapped URID is not found).
     */
    class UridMapping {
        std::vector<uint8_t> urids{};
        std::vector<std::string> string_pool{};
        std::vector<const char*> uris{};

    public:
        static const uint8_t UNMAPPED_URID = 0;

        UridMapping() {
            uris.emplace_back("");
            urids.emplace_back(0);
        }

        uint8_t tryAdd(const char* uri) {
            if (uris.size() >= UINT8_MAX) {
                AAP_ASSERT_FALSE;
                return -1;
            }
            auto existing = getUrid(uri);
            if (existing > 0)
                return existing;
            uint8_t urid = uris.size();
            uris.emplace_back(uri);
            urids.emplace_back(urid);
            return urid;
        }

        uint8_t getUrid(const char* uri) {
            // starts from 1, as 0 is "unmapped"
            for (size_t i = 1, n = uris.size(); i < n; i++)
                if (uri == uris[i])
                    return urids[i];
            for (size_t i = 1, n = uris.size(); i < n; i++)
                if (uris[i] && !strcmp(uri, uris[i]))
                    return urids[i];
            return UNMAPPED_URID;
        }

        const char* getUri(uint8_t urid) {
            // starts from 1, as 0 is "unmapped"
            for (size_t i = 1, n = urids.size(); i < n; i++)
                if (urid == urids[i])
                    return uris[i];
            return nullptr;
        }

        using container=std::vector<uint8_t>;
        using iterator=typename container::iterator;
        using const_iterator=typename container::const_iterator;

        iterator begin() { return urids.begin(); }
        iterator end() { return urids.end(); }
        const_iterator begin() const { return urids.cbegin(); }
        const_iterator end() const { return urids.cend(); }

        void forceAdd(uint8_t urid, char* uri) {
            if (!uri) {
                AAP_ASSERT_FALSE;
                return;
            }
            if (strlen(uri) <= 0) {
                AAP_ASSERT_FALSE;
                return;
            }

            string_pool.emplace_back(uri);
            while (urids.size() < urid)
                urids.emplace_back(0);
            while (uris.size() < urid)
                uris.emplace_back("");
            urids.emplace_back(urid);
            // add the underlying buffer as the const char*.
            uris.emplace_back(string_pool[string_pool.size() - 1].data());
        }
    };

    template <typename T>
    class AAPXSUridMapping {
        UridMapping* urid_mapping;
        std::vector<T> items{};
        bool frozen{false};

    public:
        AAPXSUridMapping(UridMapping *mapping) : urid_mapping(mapping) {
            items.resize(UINT8_MAX);
        }
        virtual ~AAPXSUridMapping() {}

        UridMapping* getUridMapping() { return urid_mapping; }

        inline void freezeFeatureSet() { frozen = true; }
        inline bool isFrozen() { return frozen; }

        void add(T feature, const char* uri) {
            uint8_t urid = urid_mapping->tryAdd(uri);
            items[urid] = feature;
        }
        T* getByUri(const char* uri) {
            uint8_t urid = urid_mapping->getUrid(uri);
            return &items[urid];
        }
        T* getByUrid(uint8_t urid) {
            return &items[urid];
        }

        using container=std::vector<T>;
        using iterator=typename container::iterator;
        using const_iterator=typename container::const_iterator;

        iterator begin() { return items.begin(); }
        iterator end() { return items.end(); }
        const_iterator begin() const { return items.cbegin(); }
        const_iterator end() const { return items.cend(); }
    };

    typedef uint32_t (*initiator_get_new_request_id_func) (AAPXSInitiatorInstance* instance);
    typedef bool (*aapxs_initiator_send_func) (AAPXSInitiatorInstance* instance, AAPXSRequestContext* context);
    typedef void (*aapxs_recipient_send_func) (AAPXSRecipientInstance* instance, AAPXSRequestContext* context);

    class AAPXSDispatcher {
    protected:
        AAPXSUridMapping<AAPXSInitiatorInstance> initiators;
        AAPXSUridMapping<AAPXSRecipientInstance> recipients;
        std::map<uint8_t, std::unique_ptr<AAPXSSerializationContext>> serialization_store{};

        AAPXSDispatcher(UridMapping* mapping)
                : initiators(mapping), recipients(mapping) {
        }

        inline void addInitiator(AAPXSInitiatorInstance initiator, const char* uri) { initiators.add(initiator, uri); }
        inline void addRecipient(AAPXSRecipientInstance recipient, const char* uri) { recipients.add(recipient, uri); }
    };

    class AAPXSDefinitionRegistry : public AAPXSUridMapping<AAPXSDefinition>  {
        std::unique_ptr<UridMapping> mapping;

    public:
        AAPXSDefinitionRegistry(std::unique_ptr<UridMapping> mapping = std::make_unique<UridMapping>(), std::vector<AAPXSDefinition> items = {});

        static AAPXSDefinitionRegistry* getStandardExtensions();
    };

    // Created per plugin instance.
    class AAPXSClientDispatcher : public AAPXSDispatcher {
        AAPXSDefinitionRegistry* registry;
        bool already_setup{false};

        AAPXSInitiatorInstance
        populateAAPXSInitiatorInstance(void* hostContext,
                                       AAPXSSerializationContext *serialization,
                                       uint8_t urid,
                                       aapxs_initiator_send_func sendAAPXSRequest,
                                       initiator_get_new_request_id_func getNewRequestId);
        AAPXSRecipientInstance
        populateAAPXSRecipientInstance(void* hostContext,
                                       AAPXSSerializationContext *serialization,
                                       aapxs_recipient_send_func sendAapxsReply);

    public:
        AAPXSClientDispatcher(AAPXSDefinitionRegistry* registry);

        inline AAPXSInitiatorInstance* getPluginAAPXSByUri(const char* uri) { if (already_setup) return initiators.getByUri(uri); AAP_ASSERT_FALSE; return nullptr; }
        inline AAPXSInitiatorInstance* getPluginAAPXSByUrid(uint8_t urid) { if (already_setup) return initiators.getByUrid(urid); AAP_ASSERT_FALSE; return nullptr; }
        inline AAPXSRecipientInstance* getHostAAPXSByUri(const char* uri) { if (already_setup) return recipients.getByUri(uri); AAP_ASSERT_FALSE; return nullptr; }
        inline AAPXSRecipientInstance* getHostAAPXSByUrid(uint8_t urid) { if (already_setup) return recipients.getByUrid(urid); AAP_ASSERT_FALSE; return nullptr; }

        bool
        setupInstances(void* hostContext,
                       std::function<bool(const char*, AAPXSSerializationContext*)> sharedMemoryAllocatingRequester,
                       aapxs_initiator_send_func sendAAPXSRequest,
                       aapxs_recipient_send_func sendAAPXSReplyFunc,
                       initiator_get_new_request_id_func initiatorGetNewRequestId);

        AAPXSSerializationContext *getSerialization(const char *uri);
    };

    // Created per plugin instance.
    class AAPXSServiceDispatcher : public AAPXSDispatcher {
        AAPXSDefinitionRegistry *registry;
        std::map<uint8_t, std::unique_ptr<AAPXSSerializationContext>> serialization_store{};
        bool already_setup{false};

        AAPXSInitiatorInstance populateAAPXSInitiatorInstance(
                void* hostContext,
                AAPXSSerializationContext* serialization,
                uint8_t urid,
                aapxs_initiator_send_func sendHostAAPXSRequest,
                initiator_get_new_request_id_func getNewRequestId);
        AAPXSRecipientInstance populateAAPXSRecipientInstance(
                void* hostContext,
                AAPXSSerializationContext* serialization,
                aapxs_recipient_send_func sendAAPXSReply);

    public:
        AAPXSServiceDispatcher(AAPXSDefinitionRegistry* registry);

        AAPXSRecipientInstance* getPluginAAPXSByUri(const char* uri) { if (already_setup) return recipients.getByUri(uri);  AAP_ASSERT_FALSE; return nullptr; }
        AAPXSRecipientInstance* getPluginAAPXSByUrid(uint8_t urid) { if (already_setup) return recipients.getByUrid(urid);  AAP_ASSERT_FALSE; return nullptr; }
        AAPXSInitiatorInstance* getHostAAPXSByUri(const char* uri) { if (already_setup) return initiators.getByUri(uri);  AAP_ASSERT_FALSE; return nullptr; }
        AAPXSInitiatorInstance* getHostAAPXSByUrid(uint8_t urid) { if (already_setup) return initiators.getByUrid(urid);  AAP_ASSERT_FALSE; return nullptr; }

        void
        setupInstances(void* hostContext,
                       std::function<void(const char*,AAPXSSerializationContext*)> extensionBufferAssigner,
                       aapxs_recipient_send_func sendAapxsReply,
                       aapxs_initiator_send_func sendAAPXSRequest,
                       initiator_get_new_request_id_func initiatorGetNewRequestId);
    };

    class AAPXSDefinitionClientRegistry {
        AAPXSDefinitionRegistry* registry;

    public:
        AAPXSDefinitionClientRegistry(AAPXSDefinitionRegistry* registry = nullptr) : registry(registry ? registry : AAPXSDefinitionRegistry::getStandardExtensions()){}
        virtual ~AAPXSDefinitionClientRegistry() {}

        AAPXSDefinitionRegistry* items() { return registry; }
    };

    class AAPXSDefinitionServiceRegistry {
        AAPXSDefinitionRegistry* registry;

    public:
        AAPXSDefinitionServiceRegistry(AAPXSDefinitionRegistry* registry = nullptr) : registry(registry ? registry : AAPXSDefinitionRegistry::getStandardExtensions()){}
        virtual ~AAPXSDefinitionServiceRegistry() {}

        AAPXSDefinitionRegistry* items() { return registry; }
    };
}

#endif //AAP_CORE_AAPXS_HOSTING_RUNTIME_H
