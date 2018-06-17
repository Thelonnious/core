#include "../generic/NonCombatStrategy.h"
#pragma once

namespace ai
{
    class FollowMasterStrategy : public NonCombatStrategy
    {
    public:
        FollowMasterStrategy(PlayerbotAI* ai) : NonCombatStrategy(ai) {}
        virtual std::string getName() { return "follow"; }
        virtual ActionList getDefaultActions();
        virtual void InitTriggers(std::list<TriggerNode*> &triggers);

    };

}
