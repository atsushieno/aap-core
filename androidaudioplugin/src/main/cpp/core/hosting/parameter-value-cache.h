#ifndef AAP_CORE_PARAMETER_VALUE_CACHE_H
#define AAP_CORE_PARAMETER_VALUE_CACHE_H

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace aap::internal {
struct ParameterValueDescription {
    int32_t id;
    double minimum, maximum, default_value;
};
// Metadata/index snapshots are immutable and retained until instance teardown.
// Cells are shared by stable ID across revisions, so updates through an earlier
// snapshot cannot be lost during publication of a new parameter layout.
class ParameterValueCache {
    static_assert(std::atomic<double>::is_always_lock_free,
                  "parameter values must not use library locks on this target");
    struct Entry {
        ParameterValueDescription description;
        std::shared_ptr<std::atomic<double>> value;
    };
public:
    struct Snapshot {
        std::vector<Entry> entries;
        std::unordered_map<int32_t, size_t> id_to_index;
        bool setById(int32_t id, double value) const noexcept {
            auto it = id_to_index.find(id);
            if (it == id_to_index.end()) return false;
            entries[it->second].value->store(value, std::memory_order_relaxed);
            return true;
        }
    };
private:
    static_assert(std::atomic<const Snapshot*>::is_always_lock_free);
    std::atomic<const Snapshot*> current{nullptr};
    std::mutex publication_mutex; // control writers only
    std::vector<std::unique_ptr<Snapshot>> retained;
    // Keep cells even while their IDs are absent from a particular revision.
    std::unordered_map<int32_t, std::shared_ptr<std::atomic<double>>> cells;
public:
    const Snapshot* snapshot() const noexcept { return current.load(std::memory_order_acquire); }
    void publish(const std::vector<ParameterValueDescription>& parameters) {
        std::lock_guard<std::mutex> writers{publication_mutex};
        auto next = std::make_unique<Snapshot>();
        next->entries.reserve(parameters.size());
        for (auto& parameter : parameters) {
            auto& cell = cells[parameter.id];
            if (!cell) cell = std::make_shared<std::atomic<double>>(parameter.default_value);
            next->id_to_index[parameter.id] = next->entries.size();
            next->entries.push_back({parameter, cell});
        }
        auto* published = next.get();
        retained.push_back(std::move(next));
        current.store(published, std::memory_order_release);
    }
    bool setById(int32_t id, double value) const noexcept {
        auto* data = snapshot();
        return data && data->setById(id, value);
    }
    void setByIndex(int32_t index, double value) const noexcept {
        auto* data = snapshot();
        if (data && index >= 0 && static_cast<size_t>(index) < data->entries.size())
            data->entries[index].value->store(value, std::memory_order_relaxed);
    }
    double getByIndex(int32_t index) const noexcept {
        auto* data = snapshot();
        return data && index >= 0 && static_cast<size_t>(index) < data->entries.size()
                ? data->entries[index].value->load(std::memory_order_relaxed) : 0.0;
    }
};
}
#endif
