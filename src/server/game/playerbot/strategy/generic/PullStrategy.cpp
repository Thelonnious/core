
#include "../../playerbot.h"
#include "../PassiveMultiplier.h"
#include "PullStrategy.h"

using namespace ai;

class MagePullMultiplier : public PassiveMultiplier
{
public:
    MagePullMultiplier(PlayerbotAI* ai, std::string action) : PassiveMultiplier(ai)
    {
        this->action = action;
    }

public:
    virtual float GetValue(Action* action);

private:
    std::string action;
};

float MagePullMultiplier::GetValue(Action* _action) 
{
    if (!_action)
        return 1.0f;

    std::string name = _action->getName();
    if (this->action == name ||
        name == "reach spell" ||
        name == "change strategy")
        return 1.0f;

    return PassiveMultiplier::GetValue(_action);
}

NextAction** PullStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction(action, 105.0f), new NextAction("follow", 104.0f), new NextAction("end pull", 103.0f), NULL);
}

void PullStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    RangedCombatStrategy::InitTriggers(triggers);
}

void PullStrategy::InitMultipliers(std::list<Multiplier*> &multipliers)
{
    multipliers.push_back(new MagePullMultiplier(ai, action));
    RangedCombatStrategy::InitMultipliers(multipliers);
}

