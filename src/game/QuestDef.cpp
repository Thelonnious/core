/*
 * Copyright (C) 2005-2008 MaNGOS <http://www.mangosproject.org/>
 *
 * Copyright (C) 2008 Trinity <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "QuestDef.h"
#include "Player.h"
#include "World.h"

Quest::Quest(Field * questRecord)
{
    QuestId = questRecord[0].GetUInt32();
    QuestMethod = questRecord[1].GetUInt8();
    ZoneOrSort = questRecord[2].GetInt16();
    SkillOrClass = questRecord[3].GetInt16();
    MinLevel = questRecord[4].GetUInt8();
    QuestLevel = questRecord[5].GetUInt8();
    Type = (QuestTypes)(questRecord[6].GetUInt16());
    RequiredRaces = questRecord[7].GetUInt16();
    RequiredSkillValue = questRecord[8].GetUInt16();
    RepObjectiveFaction = questRecord[9].GetUInt16();
    RepObjectiveValue = questRecord[10].GetInt32();
    RequiredMinRepFaction = questRecord[11].GetUInt16();
    RequiredMinRepValue = questRecord[12].GetInt32();
    RequiredMaxRepFaction = questRecord[13].GetUInt16();
    RequiredMaxRepValue = questRecord[14].GetInt32();
    SuggestedPlayers = questRecord[15].GetUInt8();
    LimitTime = questRecord[16].GetUInt32();
    QuestFlags = questRecord[17].GetUInt16();
    uint8 SpecialFlags = questRecord[18].GetUInt8();
    CharTitleId = questRecord[19].GetUInt8();
    PrevQuestId = questRecord[20].GetInt32();
    NextQuestId = questRecord[21].GetInt32();
    ExclusiveGroup = questRecord[22].GetInt32();
    NextQuestInChain = questRecord[23].GetUInt32();
    SrcItemId = questRecord[24].GetUInt32();
    SrcItemCount = questRecord[25].GetUInt8();
    SrcSpell = questRecord[26].GetUInt32();
    Title = questRecord[27].GetString();
    Details = questRecord[28].GetString();
    Objectives = questRecord[29].GetString();
    OfferRewardText = questRecord[30].GetString();
    RequestItemsText = questRecord[31].GetString();
    EndText = questRecord[32].GetString();

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        ObjectiveText[i] = questRecord[33+i].GetString();

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        RequiredItemId[i] = questRecord[37+i].GetUInt32();

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        RequiredItemCount[i] = questRecord[41+i].GetUInt16();

    for (int i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
        RequiredSourceItemId[i] = questRecord[45+i].GetUInt32();

    for (int i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
        RequiredSourceItemCount[i] = questRecord[49+i].GetUInt16();

    for (int i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
        ReqSourceRef[i] = questRecord[53+i].GetUInt8();

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        RequiredNpcOrGo[i] = questRecord[57+i].GetInt32();

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        RequiredNpcOrGoCount[i] = questRecord[61+i].GetUInt16();

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        ReqSpell[i] = questRecord[65+i].GetUInt32();

    for (int i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
        RewardChoiceItemId[i] = questRecord[69+i].GetUInt32();

    for (int i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
        RewardChoiceItemCount[i] = questRecord[75+i].GetUInt16();

    for (int i = 0; i < QUEST_REWARDS_COUNT; ++i)
        RewardItemId[i] = questRecord[81+i].GetUInt32();

    for (int i = 0; i < QUEST_REWARDS_COUNT; ++i)
        RewardItemIdCount[i] = questRecord[85+i].GetUInt16();

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
        RewardRepFaction[i] = questRecord[89+i].GetUInt16();

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
        RewardRepValue[i] = questRecord[94+i].GetInt32();

    RewardHonorableKills = questRecord[99].GetUInt32();
    RewardOrReqMoney = questRecord[100].GetInt32();
    RewardMoneyMaxLevel = questRecord[101].GetUInt32();
    RewardSpell = questRecord[102].GetUInt32();
    RewardSpellCast = questRecord[103].GetUInt32();
    RewardMailTemplateId = questRecord[104].GetUInt32();
    RewardMailDelaySecs = questRecord[105].GetUInt32();
    PointMapId = questRecord[106].GetUInt16();
    PointX = questRecord[107].GetFloat();
    PointY = questRecord[108].GetFloat();
    PointOpt = questRecord[109].GetUInt32();

	for (int i = 0; i < QUEST_EMOTE_COUNT; ++i)
		DetailsEmote[i] = questRecord[110 + i].GetUInt16();

    for (unsigned int & i : DetailsEmoteDelay)
        i = 0; //NYI

    IncompleteEmote = questRecord[114].GetUInt16();
    CompleteEmote = questRecord[115].GetUInt16();

	for (int i = 0; i < QUEST_EMOTE_COUNT; ++i)
		OfferRewardEmote[i] = questRecord[116 + i].GetInt16();

    for (unsigned int & i : OfferRewardEmoteDelay)
        i = 0; //NYI

    QuestStartScript = questRecord[120].GetUInt32();
    QuestCompleteScript = questRecord[121].GetUInt32();
    QuestFlags |= SpecialFlags << 16;

    m_reqitemscount = 0;
    m_reqCreatureOrGOcount = 0;
    m_rewitemscount = 0;
    m_rewchoiceitemscount = 0;

    for (int i=0; i < QUEST_OBJECTIVES_COUNT; i++)
    {
        if ( RequiredItemId[i] )
            ++m_reqitemscount;
        if ( RequiredNpcOrGo[i] )
            ++m_reqCreatureOrGOcount;
    }

    for (unsigned int i : RewardItemId)
    {
        if ( i )
            ++m_rewitemscount;
    }

    for (unsigned int i : RewardChoiceItemId)
    {
        if (i)
            ++m_rewchoiceitemscount;
    }

    m_markedAsBugged = questRecord[122].GetBool();
}

uint32 Quest::XPValue( Player *pPlayer ) const
{
    if( pPlayer )
    {
        if( RewardMoneyMaxLevel > 0 )
        {
            uint32 pLevel = pPlayer->GetLevel();
            uint32 qLevel = QuestLevel;
            float fullxp = 0;
            if (qLevel >= 65)
                fullxp = RewardMoneyMaxLevel / 6.0f;
            else if (qLevel == 64)
                fullxp = RewardMoneyMaxLevel / 4.8f;
            else if (qLevel == 63)
                fullxp = RewardMoneyMaxLevel / 3.6f;
            else if (qLevel == 62)
                fullxp = RewardMoneyMaxLevel / 2.4f;
            else if (qLevel == 61)
                fullxp = RewardMoneyMaxLevel / 1.2f;
            else if (qLevel > 0 && qLevel <= 60)
                fullxp = RewardMoneyMaxLevel / 0.6f;

            if( pLevel <= qLevel +  5 )
                return (uint32)fullxp;
            else if( pLevel == qLevel +  6 )
                return (uint32)(fullxp * 0.8f);
            else if( pLevel == qLevel +  7 )
                return (uint32)(fullxp * 0.6f);
            else if( pLevel == qLevel +  8 )
                return (uint32)(fullxp * 0.4f);
            else if( pLevel == qLevel +  9 )
                return (uint32)(fullxp * 0.2f);
            else
                return (uint32)(fullxp * 0.1f);
        }
    }
    return 0;
}

int32  Quest::GetRewOrReqMoney() const
{
    if(RewardOrReqMoney <=0)
        return RewardOrReqMoney;

    return int32(RewardOrReqMoney * sWorld->GetRate(RATE_DROP_MONEY));
}

