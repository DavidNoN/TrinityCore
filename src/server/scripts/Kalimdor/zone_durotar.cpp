/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "CreatureAIImpl.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellScript.h"

namespace Durotar
{
enum WaywardPlainstrider
{
    QUEST_UNBIDDEN_VISITORS             = 25194,
    NPC_SCARED_WAYWARD_PLAINSTRIDER_KC   = 39336,
    SPELL_WAYWARD_PLAINSTRIDER_CHARGE   = 6268,
    SPELL_WAYWARD_PLAINSTRIDER_WATER    = 73757,
    POINT_WAYWARD_PLAINSTRIDER_ESCAPE   = 1
};

struct npc_wayward_plainstrider : ScriptedAI
{
    explicit npc_wayward_plainstrider(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _escaping = false;
        _despawnTimer = 0;
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        me->SetReactState(REACT_AGGRESSIVE);
        DoCast(me, SPELL_WAYWARD_PLAINSTRIDER_WATER, true);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoCast(me, SPELL_WAYWARD_PLAINSTRIDER_CHARGE);
    }

    void DamageTaken(Unit* attacker, uint32& damage) override
    {
        if (_escaping)
        {
            damage = 0;
            return;
        }

        // A lethal hit still kills the bird, matching the quest's requirement
        // to injure it without one-shotting it. Pets credit their player owner.
        if (!attacker || me->IsCharmed() || damage >= me->GetHealth()
            || !me->HealthBelowPctDamaged(25, damage))
            return;

        Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player)
            return;

        _escaping = true;
        _despawnTimer = 30000;
        me->CombatStop(true);
        me->SetReactState(REACT_PASSIVE);
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        me->SetWalk(false);
        DoCast(me, SPELL_WAYWARD_PLAINSTRIDER_WATER, true);

        if (player->GetQuestStatus(QUEST_UNBIDDEN_VISITORS) == QUEST_STATUS_INCOMPLETE)
            player->KilledMonsterCredit(NPC_SCARED_WAYWARD_PLAINSTRIDER_KC);

        // Increasing Y crosses the Southfury River west into the Barrens.
        // Keep the route at least as high as the starting water surface.
        float targetY = me->GetPositionY() + 200.0f;
        float targetZ = me->GetMap()->GetHeight(me->GetPhaseShift(),
            me->GetPositionX(), targetY, me->GetPositionZ() + 50.0f, true, 100.0f);
        targetZ = std::max(me->GetPositionZ(), targetZ);
        me->GetMotionMaster()->MovePoint(POINT_WAYWARD_PLAINSTRIDER_ESCAPE,
            me->GetPositionX(), targetY, targetZ, false);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (_escaping && type == POINT_MOTION_TYPE && id == POINT_WAYWARD_PLAINSTRIDER_ESCAPE)
            me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        if (_escaping)
        {
            if (_despawnTimer <= diff)
                me->DespawnOrUnsummon();
            else
                _despawnTimer -= diff;
            return;
        }

        if (UpdateVictim())
            DoMeleeAttackIfReady();
    }

private:
    bool _escaping = false;
    uint32 _despawnTimer = 0;
};

enum VoodooSpells
{
    SPELL_BREW      = 16712, // Special Brew
    SPELL_GHOSTLY   = 16713, // Ghostly
    SPELL_HEX1      = 16707, // Hex
    SPELL_HEX2      = 16708, // Hex
    SPELL_HEX3      = 16709, // Hex
    SPELL_GROW      = 16711, // Grow
    SPELL_LAUNCH    = 16716, // Launch (Whee!)
};

// 17009
class spell_voodoo : public SpellScriptLoader
{
    public:
        spell_voodoo() : SpellScriptLoader("spell_voodoo") { }

        class spell_voodoo_SpellScript : public SpellScript
        {
            bool Validate(SpellInfo const* /*spellInfo*/) override
            {
                return ValidateSpellInfo({ SPELL_BREW, SPELL_GHOSTLY, SPELL_HEX1, SPELL_HEX2, SPELL_HEX3, SPELL_GROW, SPELL_LAUNCH });
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                uint32 spellid = RAND(SPELL_BREW, SPELL_GHOSTLY, RAND(SPELL_HEX1, SPELL_HEX2, SPELL_HEX3), SPELL_GROW, SPELL_LAUNCH);
                if (Unit* target = GetHitUnit())
                    GetCaster()->CastSpell(target, spellid, false);
            }

            void Register() override
            {
                OnEffectHitTarget.Register(&spell_voodoo_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const override
        {
            return new spell_voodoo_SpellScript();
        }
};

class spell_zuni_lvl_1_trigger_aura : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetCaster(), GetSpellInfo()->Effects[EFFECT_0].BasePoints, true);
    }

    void Register() override
    {
        OnEffectHitTarget.Register(&spell_zuni_lvl_1_trigger_aura::HandleDummy, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};
}

void AddSC_durotar()
{
    using namespace Durotar;
    RegisterCreatureAI(npc_wayward_plainstrider);
    new spell_voodoo();
    RegisterSpellScript(spell_zuni_lvl_1_trigger_aura);
}
