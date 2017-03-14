#include "StealthAlertMovementGenerator.h"
#include "Creature.h"
#include "MotionMaster.h"

StealthAlertMovementGenerator::StealthAlertMovementGenerator(Unit const* owner, Unit const* target, uint32 timer) : 
    m_timer(timer), 
    originalOrientation(owner->GetOrientation()), 
    targetOrientation(owner->GetAngle(target)) 
{ }

bool StealthAlertMovementGenerator::Initialize(Unit* owner)
{
    owner->SetFacingTo(targetOrientation);
    return true;
}

void StealthAlertMovementGenerator::Finalize(Unit* owner, bool premature) 
{
    if(!premature)
        owner->SetFacingTo(originalOrientation); // "Hmm, must have been the wind"
}

bool StealthAlertMovementGenerator::Update(Unit* owner, uint32 time_diff)
{
    //stop being suspicious if a control was applied, we've got other things to worry about
    if(owner->GetMotionMaster()->GetMotionSlotType(MOTION_SLOT_CONTROLLED) != NULL_MOTION_TYPE)
        return false;
    
    //stop generator if creature entered combat, just to be sure
    if(owner->IsInCombat())
        return false;

    if (time_diff > m_timer)
        return false;

    m_timer -= time_diff;
    return true;
}