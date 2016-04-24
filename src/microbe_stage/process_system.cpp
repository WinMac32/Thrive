
#include <iostream>

#include "engine/component_factory.h"
#include "engine/engine.h"
#include "engine/game_state.h"
#include "engine/entity_filter.h"
#include "engine/serialization.h"

#include "microbe_stage/compound.h"
#include "microbe_stage/compound_registry.h"
#include "microbe_stage/bio_process_registry.h"
#include "microbe_stage/process_system.h"

using namespace thrive;

REGISTER_COMPONENT(ProcessorComponent)

luabind::scope
ProcessorComponent::luaBindings() {
    using namespace luabind;
    return class_<ProcessorComponent, Component>("ProcessorComponent")
        .enum_("ID") [
            value("TYPE_ID", ProcessorComponent::TYPE_ID)
        ]
        .scope [
            def("TYPE_NAME", &ProcessorComponent::TYPE_NAME)
        ]
        .def(constructor<>())
        .def("setThreshold", &ProcessorComponent::setThreshold)
        .def("setCapacity", &ProcessorComponent::setCapacity)
    ;
}


void
ProcessorComponent::load(const StorageContainer&)
{

}

StorageContainer
ProcessorComponent::storage() const 
{
    StorageContainer storage = Component::storage();
    //storage.set<float>("potency", m_potency);
    return storage;
}

void
ProcessorComponent::setThreshold(CompoundId id, float low, float high)
{
    this->thresholds[id] = std::pair<float, float>(low, high);
}

void
ProcessorComponent::setCapacity(BioProcessId id, float capacity)
{
    this->process_capacities[id] = capacity;
}

REGISTER_COMPONENT(CompoundBagComponent)

luabind::scope
CompoundBagComponent::luaBindings() {
    using namespace luabind;
    return class_<CompoundBagComponent, Component>("CompoundBagComponent")
        .enum_("ID") [
            value("TYPE_ID", CompoundBagComponent::TYPE_ID)
        ]
        .scope [
            def("TYPE_NAME", &CompoundBagComponent::TYPE_NAME)
        ]
        .def(constructor<>())
        .def("setProcessor", &CompoundBagComponent::setProcessor)
        .def("giveCompound", &CompoundBagComponent::giveCompound)
        .def("takeCompound", &CompoundBagComponent::takeCompound)
        .def("getCompoundAmount", &CompoundBagComponent::getCompoundAmount)
    ;
}

void
CompoundBagComponent::load(const StorageContainer&)
{

}

StorageContainer
CompoundBagComponent::storage() const
{
    StorageContainer storage = Component::storage();
    return storage;
}

void
CompoundBagComponent::setProcessor(ProcessorComponent& processor) {
    this->processor = &processor;
}

// helper methods for integrating compound bags with current, un-refactored, lua microbes
float
CompoundBagComponent::getCompoundAmount(CompoundId id) {
    return compounds[id];
}

void
CompoundBagComponent::giveCompound(CompoundId id, float amt) {
    compounds[id] += amt;
}

float
CompoundBagComponent::takeCompound(CompoundId id, float to_take) {
    float& ref = compounds[id];
    float amt = ref > to_take ? to_take : ref;
    ref -= amt;
    return amt;
}

luabind::scope
ProcessSystem::luaBindings() {
    using namespace luabind;
    return class_<ProcessSystem, System>("ProcessSystem")
        .def(constructor<>())
    ;
}
struct ProcessSystem::Implementation {

    EntityFilter<
        CompoundBagComponent
    > m_entities;

    void update(int);
    void updateAddedEntites(int);
    void updateRemovedEntities(int);
    inline float step_function(float, float, float, float);
    inline float step_2(float, float, float);

    static constexpr float SMOOTHING_FACTOR = 0.1;
    static constexpr float TIME_SCALING_FACTOR = 1;
};

ProcessSystem::ProcessSystem()
    : m_impl(new Implementation())
{

}

ProcessSystem::~ProcessSystem()
{

}

void 
ProcessSystem::init(GameState* gameState)
{
    System::initNamed("ProcessSystem", gameState);
    m_impl->m_entities.setEntityManager(&gameState->entityManager());
}

void 
ProcessSystem::shutdown()
{

}

void
ProcessSystem::Implementation::updateRemovedEntities(int) {
    // std::cerr << logicTime;
    // for (EntityId entityId : this->m_entities.removedEntities()) {
        // std::cerr << &entityId;
    // }
}

void
ProcessSystem::Implementation::updateAddedEntites(int) {
    // std::cerr << logicTime;
    // for (auto& value : this->m_entities.addedEntities()) {
        // std::cerr << &value;
    // }
}

/*
#create a step function
#return positive if below low, negative if above high
def step_function(value, threshold, high_threshold, vent_threshold):
    if value >= high_threshold:
        return -float(value - high_threshold)/(vent_threshold - high_threshold)
    elif value >= threshold:
        return 0
    elif value < threshold and threshold != 0 and value >= 0:
        return 1 - (float(value)/threshold)
    else:
        print "error in step function, I was passed a negative value"
        return 0

*/


// 0 <= value <= threshold < high_threshold < vent_threshold
inline float
ProcessSystem::Implementation::step_function(float value, float threshold, float high_threshold, float vent_threshold) {
    if (value >= high_threshold) {
        return (high_threshold - value) / (vent_threshold - high_threshold);
    }
    if (value >= threshold) {
        return 0;
    }
    return 1 - (value/threshold);
}

inline float
ProcessSystem::Implementation::step_2(float value, float threshold, float high_threshold) {
    if (value > high_threshold)
        return high_threshold - value;
    if (value >= threshold) {
        return 0;
    }
    return 1 - value/threshold;
}

void
ProcessSystem::Implementation::update(int logicTime) {
    std::cerr << logicTime;
    for (auto& value : this->m_entities) {
        CompoundBagComponent* bag = std::get<0>(value.second);
        ProcessorComponent* processor = bag->processor;
        std::unordered_map<CompoundId, float> actions;
        for (const auto& compound : bag->compounds) {
            actions[compound.first] = step_2(compound.second, processor->thresholds[compound.first].first, processor->thresholds[compound.first].second);
        }
        for (const auto& process : processor->process_capacities) {
            float input_rate = -1, output_rate = -1;
            for (const auto& input : BioProcessRegistry::getInputCompounds(process.first)) {
                const float p = actions[input.first];
                input_rate = input_rate >= p ? input_rate : p;
            }
            for (const auto& output : BioProcessRegistry::getOutputCompounds(process.first)) {
                const float p = actions[output.first];
                output_rate = output_rate >= p ? output_rate : p;
            }

            float rate = output_rate - input_rate;
            rate = rate > 0 ? rate * SMOOTHING_FACTOR : 0;
            bool will_run = true;
            for (const auto& input : BioProcessRegistry::getInputCompounds(process.first)) {
                if (rate * input.second >= bag->compounds[input.first]) {
                    will_run = false;
                    break;
                }
            }
            if (will_run) {
                for (const auto& input : BioProcessRegistry::getInputCompounds(process.first)) {
                    bag->compounds[input.first] -= rate * input.second;
                }
                for (const auto& output : BioProcessRegistry::getOutputCompounds(process.second)) {
                    bag->compounds[output.first] += rate * output.second;
                }
            }
        }
    }
}

void
ProcessSystem::update(int, int logicTime)
{
    m_impl->updateRemovedEntities(logicTime);
    m_impl->updateAddedEntites(logicTime);
    m_impl->m_entities.clearChanges();
    m_impl->update(logicTime);
}
