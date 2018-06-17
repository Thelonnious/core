#pragma once

#include "GenericWarriorStrategy.h"

namespace ai
{
    class TankWarriorStrategy : public GenericWarriorStrategy
    {
    public:
        TankWarriorStrategy(PlayerbotAI* ai);

    public:
        virtual void InitTriggers(std::list<TriggerNode*> &triggers);
        virtual std::string getName() { return "tank"; }
        virtual ActionList getDefaultActions();
        virtual int GetType() { return STRATEGY_TYPE_TANK | STRATEGY_TYPE_MELEE; }
    };
}
