#ifndef AAP_CORE_AUDIOGRAPH_H
#define AAP_CORE_AUDIOGRAPH_H

#include <cstdint>

class AudioGraphNode;

class AudioGraphNode {
public:
};
namespace aap {

    class AudioGraph {
    public:
        void attachNode(AudioGraphNode* sourceNode, int32_t sourceOutputBusIndex, AudioGraphNode* destinationNode, int32_t destinationInputBusIndex);
        void detachNode(AudioGraphNode* sourceNode, int32_t sourceOutputBusIndex);
    };
}

#endif //AAP_CORE_AUDIOGRAPH_H
