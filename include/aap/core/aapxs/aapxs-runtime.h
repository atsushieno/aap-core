#ifndef AAP_CORE_AAPXS_RUNTIME_H
#define AAP_CORE_AAPXS_RUNTIME_H

// AAPXS v2 runtime - strongly typed.

#include <assert.h>
#include <cstdint>
#include <vector>
#include <future>
#include "../../unstable/aapxs-vnext.h"
#include "../../android-audio-plugin.h"

namespace aap::xs {
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
            uris.emplace_back("");
        }

        uint8_t tryAdd(const char* uri) {
            assert(uris.size() < UINT8_MAX);
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
                if (uri == uris[n])
                    return urids[i];
            for (size_t i = 1, n = uris.size(); i < n; i++)
                if (uris[n] && !strcmp(uri, uris[n]))
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
    class AAPXSUridMapping {
        UridMapping urid_mapping{};
        std::vector<T> items{};
        bool frozen{false};

    public:
        AAPXSUridMapping() {
            items.resize(UINT8_MAX);
        }
        virtual ~AAPXSUridMapping() {}

        UridMapping* getUridMapping() { return &urid_mapping; }

        inline void freezeFeatureSet() { frozen = true; }
        inline bool isFrozen() { return frozen; }

        void add(T feature, const char* uri) {
            uint8_t urid = urid_mapping.tryAdd(uri);
            items[urid] = feature;
        }
        T* getByUri(const char* uri) {
            uint8_t urid = urid_mapping.getUrid(uri);
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
        const_iterator begin() const { return items.begin(); }
        const_iterator end() const { return items.end(); }
    };

    class AAPXSDefinitionClientRegistry;
    class AAPXSDefinitionServiceRegistry;

    typedef uint32_t (*initiator_get_new_request_id_func) (AAPXSInitiatorInstance* instance);
    typedef uint32_t (*recipient_get_new_request_id_func) (AAPXSRecipientInstance* instance);
    typedef void (*aapxs_initiator_send_func) (AAPXSInitiatorInstance* instance, AAPXSRequestContext* context);
    typedef void (*aapxs_initiator_receive_func) (AAPXSInitiatorInstance* instance, AAPXSRequestContext* context);
    typedef void (*aapxs_recipient_send_func) (AAPXSRecipientInstance* instance, AAPXSRequestContext* context);
    typedef void (*aapxs_recipient_receive_func) (AAPXSRecipientInstance* instance, AAPXSRequestContext* context);

    class AAPXSDispatcher {
    protected:
        AAPXSUridMapping<AAPXSInitiatorInstance> initiators{};
        AAPXSUridMapping<AAPXSRecipientInstance> recipients{};

        inline void addInitiator(AAPXSInitiatorInstance initiator, const char* uri) { initiators.add(initiator, uri); }
        inline void addRecipient(AAPXSRecipientInstance recipient, const char* uri) { recipients.add(recipient, uri); }
    };

    class AAPXSClientDispatcher : public AAPXSDispatcher {
        AAPXSInitiatorInstance
        populateAAPXSInitiatorInstance(AAPXSSerializationContext *serialization,
                                       aapxs_initiator_send_func sendAAPXSRequest,
                                       initiator_get_new_request_id_func getNewRequestId);
        AAPXSRecipientInstance
        populateAAPXSRecipientInstance(AAPXSSerializationContext *serialization,
                                       aapxs_recipient_send_func sendAapxsReply);

    public:
        AAPXSClientDispatcher(AAPXSDefinitionClientRegistry* registry);

        inline AAPXSInitiatorInstance* getPluginAAPXSByUri(const char* uri) { return initiators.getByUri(uri); }
        inline AAPXSInitiatorInstance* getPluginAAPXSByUrid(uint8_t urid) { return initiators.getByUrid(urid); };
        inline AAPXSRecipientInstance* getHostAAPXSByUri(const char* uri) { return recipients.getByUri(uri); }
        inline AAPXSRecipientInstance* getHostAAPXSByUrid(uint8_t urid) { return recipients.getByUrid(urid); };

        void
        setupInstances(AAPXSDefinitionClientRegistry *registry,
                       AAPXSSerializationContext *serialization,
                       aapxs_initiator_send_func sendAAPXSRequest,
                       aapxs_recipient_send_func sendAAPXSReplyFunc,
                       initiator_get_new_request_id_func initiatorGetNewRequestId);
    };

    class AAPXSServiceDispatcher : public AAPXSDispatcher {
        AAPXSInitiatorInstance populateAAPXSInitiatorInstance(
                AAPXSSerializationContext* serialization,
                aapxs_initiator_send_func sendHostAAPXSRequest,
                initiator_get_new_request_id_func getNewRequestId);
        AAPXSRecipientInstance populateAAPXSRecipientInstance(
                AAPXSSerializationContext* serialization,
                aapxs_recipient_send_func sendAAPXSReply);

    public:
        AAPXSServiceDispatcher(AAPXSDefinitionServiceRegistry* registry);

        AAPXSRecipientInstance* getPluginAAPXSByUri(const char* uri) { return recipients.getByUri(uri); }
        AAPXSRecipientInstance* getPluginAAPXSByUrid(uint8_t urid) { return recipients.getByUrid(urid); };
        AAPXSInitiatorInstance* getHostAAPXSByUri(const char* uri) { return initiators.getByUri(uri); }
        AAPXSInitiatorInstance* getHostAAPXSByUrid(uint8_t urid) { return initiators.getByUrid(urid); };

        void
        setupInstances(AAPXSDefinitionServiceRegistry *registry,
                       AAPXSSerializationContext *serialization,
                       aapxs_recipient_send_func sendAapxsReply,
                       aapxs_initiator_send_func sendAAPXSRequest,
                       initiator_get_new_request_id_func initiatorGetNewRequestId);
    };

    class AAPXSDefinitionRegistry : public AAPXSUridMapping<AAPXSDefinition>  {

    public:
        AAPXSDefinitionRegistry();

        static AAPXSDefinitionRegistry* getStandardExtensions();
    };

    class AAPXSDefinitionClientRegistry {
        AAPXSDefinitionRegistry* registry;
    public:
        AAPXSDefinitionClientRegistry(AAPXSDefinitionRegistry* registry = nullptr) : registry(registry ? registry : AAPXSDefinitionRegistry::getStandardExtensions()){}
        virtual ~AAPXSDefinitionClientRegistry() {}

        AAPXSDefinitionRegistry* items() { return registry; }
        virtual void setupClientInstances(AAPXSClientDispatcher *client, AAPXSSerializationContext* serialization) = 0;
    };

    class AAPXSDefinitionServiceRegistry {
        AAPXSDefinitionRegistry* registry;

    public:
        AAPXSDefinitionServiceRegistry(AAPXSDefinitionRegistry* registry = nullptr) : registry(registry ? registry : AAPXSDefinitionRegistry::getStandardExtensions()){}
        virtual ~AAPXSDefinitionServiceRegistry() {}

        AAPXSDefinitionRegistry* items() { return registry; }
        virtual void setupServiceInstances(AAPXSServiceDispatcher *client, AAPXSSerializationContext* serialization) = 0;
    };

    class AAPXSDefinitionWrapper {
    protected:
        AAPXSDefinitionWrapper() {}
    public:
        virtual AAPXSDefinition& asPublic() = 0;
    };

    class TypedClientAAPXS {
        const char* uri;
    protected:
        AAPXSInitiatorInstance *aapxs_instance;
        AAPXSSerializationContext *serialization;

    public:
        TypedClientAAPXS(const char* uri, AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : uri(uri), aapxs_instance(initiatorInstance), serialization(serialization) {
        }

        virtual ~TypedClientAAPXS() {}

        // This must be visible to consuming code i.e. defined in this header file.
        template<typename T>
        static void getTypedCallback(void* callbackContext, AndroidAudioPlugin* plugin, int32_t requestId) {
            auto callbackData = (WithPromise<TypedClientAAPXS, T>*) callbackContext;
            auto thiz = (TypedClientAAPXS*) callbackData->context;
            T result = *(T*) (thiz->serialization->data);
            callbackData->promise->set_value(result);
        }

        // This must be visible to consuming code i.e. defined in this header file.
        // FIXME: use spinlock for RT-safe extension functions, which means there should be another RT-safe version of this function.
        template<typename T>
        T callTypedFunctionSynchronously(int32_t opcode) {
            std::promise<T> promise{};
            uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
            auto future = promise.get_future();
            WithPromise<TypedClientAAPXS, T> callbackData{this, &promise};
            AAPXSRequestContext request{getTypedCallback<int32_t>, &callbackData, serialization, uri, requestId, opcode};

            aapxs_instance->send_aapxs_request(aapxs_instance, &request);

            future.wait();
            return future.get();
        }

        static void getVoidCallback(void* callbackContext, AndroidAudioPlugin* plugin, int32_t requestId);
        void callVoidFunctionSynchronously(int32_t opcode);
    };
}

#endif //AAP_CORE_AAPXS_RUNTIME_H
