#include "../generic/NonCombatStrategy.h"
#pragma once

namespace ai
{
    class AttackRtiStrategy : public NonCombatStrategy
    {
    public:
        AttackRtiStrategy(PlayerbotAI* ai) : NonCombatStrategy(ai) {}
        virtual std::string getName() { return "attack rti"; }

    public:
        virtual void InitTriggers(std::list<std::shared_ptr<TriggerNode>> &triggers);
    };

}
