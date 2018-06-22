#include "../generic/NonCombatStrategy.h"
#pragma once

namespace ai
{
    class TankAssistStrategy : public NonCombatStrategy
    {
    public:
        TankAssistStrategy(PlayerbotAI* ai) : NonCombatStrategy(ai) {}
        virtual std::string getName() { return "tank assist"; }
        virtual int GetType() { return STRATEGY_TYPE_TANK; }

    public:
        virtual void InitTriggers(std::list<std::shared_ptr<TriggerNode>> &triggers);
    };

}
