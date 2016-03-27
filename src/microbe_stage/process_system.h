#pragma once

#include "engine/component.h"
#include "engine/system.h"
#include "engine/touchable.h"
#include "scripting/luabind.h"
#include "engine/typedefs.h"

#include <boost/range/adaptor/map.hpp>
#include <vector>
#include <unordered_map>

namespace luabind {
class scope;
}

namespace thrive {

class ProcessorComponent : public Component {
    COMPONENT(Processor)

public:
    static luabind::scope
    luaBindings();

    void
    load(
        const StorageContainer& storage
    ) override;

    StorageContainer
    storage() const override;

    std::unordered_map<BioProcessId, float> process_capacities;
    std::unordered_map<CompoundId, std::pair<float, float>> thresholds;
};

class CompoundBagComponent : public Component {
    COMPONENT(CompoundBag)

public:
    static luabind::scope
    luaBindings();

    void
    load(
        const StorageContainer& storage
    ) override;

    StorageContainer
    storage() const override;

    ProcessorComponent* processor;
    std::unordered_map<CompoundId, float> compounds;
};

class ProcessSystem : public System {

public:
    static luabind::scope
    luaBindings();

    /**
    * @brief Constructor
    */
    ProcessSystem();

    /**
    * @brief Destructor
    */
    ~ProcessSystem();

    /**
    * @brief Initializes the system
    *
    */
    void init(GameState* gameState) override;

    /**
    * @brief Shuts the system down
    */
    void shutdown() override;

    /**
    * @brief Updates the system
    */
    void update(int renderTime, int logicTime) override;
private:

    struct Implementation;
    std::unique_ptr<Implementation> m_impl;
};

}
