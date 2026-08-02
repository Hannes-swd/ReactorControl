#include "element_registry.h"

const std::string ElementRegistry::kEmptyState;

void ElementRegistry::RegisterType(const std::string& typeId, std::vector<std::string> states) {
    if (states.empty()) {
        return;
    }
    stateNamesByType[typeId] = std::move(states);
}

void ElementRegistry::RegisterInstance(const std::string& id, const std::string& typeId, int initialStateIndex) {
    if (stateNamesByType.find(typeId) == stateNamesByType.end()) {
        return;
    }
    instances[id] = Instance{ typeId, initialStateIndex };
}

int ElementRegistry::GetStateIndex(const std::string& id) const {
    auto it = instances.find(id);
    return it != instances.end() ? it->second.stateIndex : -1;
}

const std::string& ElementRegistry::GetStateName(const std::string& id) const {
    auto it = instances.find(id);
    if (it == instances.end()) {
        return kEmptyState;
    }
    const std::vector<std::string>& states = stateNamesByType.at(it->second.typeId);
    return states[it->second.stateIndex];
}

void ElementRegistry::SetState(const std::string& id, int stateIndex) {
    auto it = instances.find(id);
    if (it == instances.end()) {
        return;
    }
    const std::vector<std::string>& states = stateNamesByType.at(it->second.typeId);
    if (stateIndex < 0 || static_cast<size_t>(stateIndex) >= states.size()) {
        return;
    }
    it->second.stateIndex = stateIndex;
}

void ElementRegistry::SetState(const std::string& id, const std::string& stateName) {
    auto it = instances.find(id);
    if (it == instances.end()) {
        return;
    }
    const std::vector<std::string>& states = stateNamesByType.at(it->second.typeId);
    for (size_t i = 0; i < states.size(); i++) {
        if (states[i] == stateName) {
            it->second.stateIndex = static_cast<int>(i);
            return;
        }
    }
}

void ElementRegistry::RegisterTrigger(const std::string& onId, const std::string& targetId, const std::string& targetState) {
    triggersByOnId[onId].emplace_back(targetId, targetState);
}

void ElementRegistry::SetFrameClick(const std::string& id) {
    frameClickedId = id;
    if (id.empty()) {
        return;
    }

    auto it = instances.find(id);
    if (it != instances.end()) {
        const std::vector<std::string>& states = stateNamesByType.at(it->second.typeId);
        it->second.stateIndex = (it->second.stateIndex + 1) % static_cast<int>(states.size());
    }

    auto triggerIt = triggersByOnId.find(id);
    if (triggerIt != triggersByOnId.end()) {
        for (const auto& [targetId, targetState] : triggerIt->second) {
            SetState(targetId, targetState);
        }
    }
}

bool ElementRegistry::WasClicked(const std::string& id) const {
    return !id.empty() && id == frameClickedId;
}

size_t ElementRegistry::StateCount(const std::string& id) const {
    auto it = instances.find(id);
    if (it == instances.end()) {
        return 0;
    }
    return stateNamesByType.at(it->second.typeId).size();
}
