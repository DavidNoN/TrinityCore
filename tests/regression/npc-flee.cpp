// World-state fixtures for the production scripts extracted by verify-npc-flee.py.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <list>
#include <string>
#include <vector>

using uint32 = std::uint32_t;
constexpr int REACT_PASSIVE = 0, REACT_AGGRESSIVE = 2;
constexpr int UNIT_STATE_CASTING = 1, UNIT_STATE_NOT_MOVE = 2;
constexpr int MOTION_SLOT_ACTIVE = 1, POINT_MOTION_TYPE = 8;
constexpr int UNIT_FIELD_FLAGS = 0, UNIT_FLAG_NON_ATTACKABLE = 1;
constexpr int UNIT_DYNAMIC_FLAGS = 1, UNIT_DYNFLAG_TAPPED = 4;
constexpr int QUEST_STATUS_INCOMPLETE = 1;
constexpr float INVALID_HEIGHT = -100000.0f;
uint32 urand(uint32 min, uint32) { return min; }

struct ObjectGuid
{
    uint32 value = 0;
    void Clear() { value = 0; }
    bool IsEmpty() const { return value == 0; }
    bool operator==(ObjectGuid const&) const = default;
};
struct Player;
struct Unit
{
    Player* player = nullptr;
    Player* GetCharmerOrOwnerPlayerOrPlayerItself() { return player; }
};
struct Player : Unit
{
    int quest = QUEST_STATUS_INCOMPLETE;
    std::vector<uint32> credits;
    Player() { player = this; }
    int GetQuestStatus(uint32) const { return quest; }
    void KilledMonsterCredit(uint32 id) { credits.push_back(id); }
};
struct Creature;
struct MotionMaster
{
    Creature* followed = nullptr;
    float targetX = 0, targetY = 0, targetZ = 0;
    uint32 point = 0;
    void Clear(int) { followed = nullptr; point = 0; }
    void MoveFollow(Creature* target, float, float, bool, bool, bool, int) { followed = target; }
    void MovePoint(uint32 id, float x, float y, float z, bool)
    { point = id; targetX = x; targetY = y; targetZ = z; }
};
struct Map
{
    float height = 25;
    float GetHeight(int, float, float, float, bool, float) const { return height; }
};
std::vector<Creature*> creatures;
uint32 nextGuid = 1;
struct Creature : Unit
{
    ObjectGuid guid{nextGuid++}, owner, loot;
    uint32 entry = 23286, health = 100, maxHealth = 100, state = 0, flags = 0;
    int react = REACT_AGGRESSIVE, phase = 1;
    bool alive = true, engaged = true, charmed = false, los = true, updateSucceeds = true;
    float x = 0, y = 0, z = 19;
    Unit* victim = nullptr;
    Unit* threat = nullptr;
    MotionMaster motion;
    Map map;
    std::vector<uint32> spells;
    unsigned melee = 0, despawns = 0, damageRequirementResets = 0;
    Creature() { creatures.push_back(this); }
    ~Creature() { std::erase(creatures, this); }
    uint32 GetEntry() const { return entry; }
    ObjectGuid GetGUID() const { return guid; }
    ObjectGuid GetCharmerOrOwnerGUID() const { return owner; }
    ObjectGuid GetLootRecipientGUID() const { return loot; }
    bool IsAlive() const { return alive; }
    bool IsCharmed() const { return charmed; }
    bool IsInPhase(Creature const* other) const { return phase == other->phase; }
    bool IsWithinLOSInMap(Creature const* other) const { return los && other->los; }
    bool HasUnitState(uint32 mask) const { return (state & mask) != 0; }
    bool HealthBelowPct(int pct) const { return health < maxHealth * pct / 100; }
    bool HealthBelowPctDamaged(int pct, uint32 damage) const
    { return std::int64_t(health) - damage < maxHealth * pct / 100; }
    uint32 GetHealth() const { return health; }
    Unit* GetVictim() const { return victim; }
    float GetDistance(Creature const* other) const
    { return std::sqrt(std::pow(x-other->x, 2) + std::pow(y-other->y, 2) + std::pow(z-other->z, 2)); }
    bool IsWithinDistInMap(Creature const* other, float range) const { return GetDistance(other) <= range; }
    void GetCreatureListWithEntryInGrid(std::list<Creature*>& list, uint32 id, float range)
    { for (Creature* c : creatures) if (c->entry == id && GetDistance(c) <= range) list.push_back(c); }
    void AttackStop() { victim = nullptr; }
    void CombatStop(bool) { victim = threat = nullptr; engaged = false; }
    void StopMoving() { }
    void SetReactState(int value) { react = value; }
    void SetWalk(bool) { }
    MotionMaster* GetMotionMaster() { return &motion; }
    bool UpdateEntry(uint32 id) { if (!updateSucceeds) return false; entry = id; maxHealth = 200; return true; }
    void SetFullHealth() { health = maxHealth; }
    void ResetPlayerDamageReq() { ++damageRequirementResets; }
    void DespawnOrUnsummon() { alive = false; ++despawns; }
    void SetFlag(int, uint32 mask) { flags |= mask; }
    void RemoveFlag(int, uint32 mask) { flags &= ~mask; }
    float GetPositionX() const { return x; }
    float GetPositionY() const { return y; }
    float GetPositionZ() const { return z; }
    int GetPhaseShift() const { return phase; }
    Map* GetMap() { return &map; }
};
namespace ObjectAccessor
{
    Creature* GetCreature(Creature const&, ObjectGuid guid)
    { for (Creature* c : creatures) if (c->guid == guid) return c; return nullptr; }
}
struct ScriptedAI
{
    Creature* me;
    explicit ScriptedAI(Creature* c) : me(c) { }
    virtual ~ScriptedAI() = default;
    virtual void Reset() { }
    virtual void JustEngagedWith(Unit*) { }
    virtual void DamageTaken(Unit*, uint32&) { }
    virtual void MovementInform(uint32, uint32) { }
    virtual void UpdateAI(uint32) { }
    bool UpdateVictim()
    {
        if (!me->alive || !me->engaged) return false;
        if (me->react != REACT_PASSIVE && !me->victim) me->victim = me->threat;
        return me->victim != nullptr || me->react == REACT_PASSIVE;
    }
    void DoCast(Unit*, uint32 spell, bool = false) { me->spells.push_back(spell); }
    void DoCastVictim(uint32 spell) { if (me->victim) DoCast(me->victim, spell); }
    void DoMeleeAttackIfReady() { if (me->victim) ++me->melee; }
};

// INSERT_PRODUCTION_SCRIPTS_HERE

unsigned checks = 0, failures = 0;
void Check(std::string const& name, bool pass)
{ ++checks; if (!pass) ++failures; std::cout << (pass ? "PASS " : "FAIL ") << name << '\n'; }
void Engage(Creature& c, Unit& target) { c.victim = c.threat = &target; c.engaged = true; }

int main()
{
    Player enemy;
    {
        Creature blood, partner; partner.x = 10; blood.loot = {1}; Engage(blood, enemy);
        npc_black_blood_of_draenor ai(&blood); ai.Reset();
        blood.health = 40; ai.UpdateAI(1000);
        Check("blood does not seek a partner above the threshold", blood.react == REACT_AGGRESSIVE);
        blood.health = 39; ai.UpdateAI(1000); ai.UpdateAI(100);
        Check("blood follows a nearby ooze at low health", blood.motion.followed == &partner && !blood.victim);
        Check("blood slows its attacker before retreating", std::ranges::find(blood.spells, 7279) != blood.spells.end());
        blood.x = 10; ai.UpdateAI(100);
        Check("two oozes become one full-health terror", blood.entry == 23290 && blood.health == 200 && partner.despawns == 1);
        Check("merging keeps combat and renews the reward damage requirement", blood.threat == &enemy && blood.damageRequirementResets == 1);
        Check("merged terror retains its combat tag", blood.loot == ObjectGuid{1} && (blood.flags & UNIT_DYNFLAG_TAPPED));
        ai.UpdateAI(100); Check("terror resumes attacking", blood.victim == &enemy && blood.melee > 0);
        ai.UpdateAI(20000); Check("terror cannot merge again", partner.despawns == 1);
    }
    {
        Creature blood; Engage(blood, enemy); blood.health = 39;
        npc_black_blood_of_draenor ai(&blood); ai.Reset(); ai.UpdateAI(1000);
        Check("isolated blood continues fighting without a partner", blood.victim == &enemy && blood.melee == 1);
    }
    for (int invalid = 0; invalid < 6; ++invalid)
    {
        Creature blood, bad, good; Engage(blood, enemy); blood.health = 39; bad.x = 2; good.x = 10;
        if (invalid == 0) bad.alive = false;
        if (invalid == 1) bad.charmed = true;
        if (invalid == 2) bad.owner = {99};
        if (invalid == 3) bad.phase = 2;
        if (invalid == 4) bad.los = false;
        if (invalid == 5) bad.loot = {99};
        npc_black_blood_of_draenor ai(&blood); ai.Reset(); ai.UpdateAI(1000); ai.UpdateAI(100);
        Check("blood excludes invalid partner " + std::to_string(invalid), blood.motion.followed == &good);
    }
    for (int invalid = 0; invalid < 3; ++invalid)
    {
        Creature blood, partner; partner.x = 10; Engage(blood, enemy); blood.health = 39;
        npc_black_blood_of_draenor ai(&blood); ai.Reset(); ai.UpdateAI(1000); ai.UpdateAI(100);
        if (invalid == 0) partner.alive = false;
        if (invalid == 1) partner.entry = 23290;
        if (invalid == 2) partner.x = 50;
        ai.UpdateAI(100);
        Check("lost partner resumes combat " + std::to_string(invalid), blood.react == REACT_AGGRESSIVE && !blood.motion.followed);
        ai.UpdateAI(100); Check("lost partner does not leave blood passive " + std::to_string(invalid), blood.victim == &enemy);
    }
    {
        Creature blood, partner; partner.x = 10; Engage(blood, enemy); blood.health = 39;
        npc_black_blood_of_draenor ai(&blood); ai.Reset(); ai.UpdateAI(1000); ai.UpdateAI(100);
        blood.x = 10; blood.state = UNIT_STATE_NOT_MOVE; ai.UpdateAI(100);
        Check("stun or root prevents merging", blood.entry == 23286 && partner.alive);
        ai.UpdateAI(20000); ai.UpdateAI(100);
        Check("unreachable merge times out into combat", blood.victim == &enemy && !blood.motion.followed);
    }
    {
        Creature blood, partner; partner.x = 10; Engage(blood, enemy); blood.health = 39;
        npc_black_blood_of_draenor ai(&blood); ai.Reset(); ai.UpdateAI(1000); ai.UpdateAI(100);
        blood.x = 10; blood.updateSucceeds = false; ai.UpdateAI(100); ai.UpdateAI(100);
        Check("failed transformation keeps both oozes alive and resumes fighting", partner.alive && blood.entry == 23286 && blood.victim == &enemy);
    }
    {
        Creature blood, partner; Engage(blood, enemy); blood.health = 39; blood.charmed = true;
        npc_black_blood_of_draenor ai(&blood); ai.Reset(); ai.UpdateAI(1000);
        Check("charmed blood cannot consume an ooze", blood.entry == 23286 && partner.alive);
    }
    {
        Creature first, partner, second; partner.x = 10; second.x = 20;
        first.health = second.health = 39; Engage(first, enemy); Engage(second, enemy);
        npc_black_blood_of_draenor firstAI(&first), secondAI(&second);
        firstAI.Reset(); secondAI.Reset(); firstAI.UpdateAI(1000); secondAI.UpdateAI(1000);
        first.x = 10; firstAI.UpdateAI(100); secondAI.UpdateAI(100); secondAI.UpdateAI(100);
        Check("competing oozes cannot consume the same partner twice", first.entry == 23290 && partner.despawns == 1 && second.entry == 23286);
        Check("losing a contested partner restores the second ooze's combat", second.victim == &enemy && second.react == REACT_AGGRESSIVE);
    }
    {
        Creature blood, partner; partner.x = 10; Engage(blood, enemy); blood.health = 39;
        npc_black_blood_of_draenor ai(&blood); ai.Reset(); ai.UpdateAI(1000);
        blood.state = UNIT_STATE_CASTING; ai.UpdateAI(100);
        Check("blood waits for its slow cast before moving", !blood.motion.followed);
        blood.state = 0; ai.UpdateAI(100);
        Check("blood starts moving when its slow cast finishes", blood.motion.followed == &partner);
    }
    {
        Creature bird; bird.entry = 39337; bird.y = -3800; Engage(bird, enemy);
        npc_wayward_plainstrider ai(&bird); ai.Reset(); ai.JustEngagedWith(&enemy);
        Check("plainstrider keeps water walking and charge", std::ranges::find(bird.spells, 73757) != bird.spells.end() && std::ranges::find(bird.spells, 6268) != bird.spells.end());
        uint32 damage = 74; ai.DamageTaken(&enemy, damage);
        Check("plainstrider fights above 25 percent", bird.engaged && enemy.credits.empty());
        damage = 76; ai.DamageTaken(&enemy, damage);
        Check("plainstrider retreats west across the river", bird.motion.point != 0 && bird.motion.targetY > bird.y && bird.motion.targetZ >= bird.z);
        Check("plainstrider exits combat and gives quest credit", !bird.engaged && enemy.credits == std::vector<uint32>{39336});
        damage = 500; ai.DamageTaken(&enemy, damage);
        Check("escaping plainstrider survives follow-up damage and gives credit once", damage == 0 && enemy.credits.size() == 1);
        ai.MovementInform(POINT_MOTION_TYPE, bird.motion.point);
        Check("plainstrider despawns upon reaching its destination", bird.despawns == 1);
    }
    {
        Creature bird; bird.entry = 39337; npc_wayward_plainstrider ai(&bird); ai.Reset();
        Player player; Unit pet; pet.player = &player; uint32 damage = 76; ai.DamageTaken(&pet, damage);
        Check("pet damage awards its owner quest credit", player.credits == std::vector<uint32>{39336});
        ai.UpdateAI(40000); Check("plainstrider escape has a bounded lifetime", bird.despawns == 1);
    }
    {
        Creature bird; bird.entry = 39337; npc_wayward_plainstrider ai(&bird); ai.Reset();
        Player player; uint32 damage = 100; ai.DamageTaken(&player, damage);
        Check("lethal hit does not incorrectly award escape credit", damage == 100 && player.credits.empty() && bird.motion.point == 0);
    }
    {
        Creature bird; bird.entry = 39337; bird.map.height = INVALID_HEIGHT;
        npc_wayward_plainstrider ai(&bird); ai.Reset(); Player player; player.quest = 0;
        uint32 damage = 76; ai.DamageTaken(&player, damage);
        Check("plainstrider also flees without the quest", bird.motion.point != 0 && player.credits.empty());
        Check("missing terrain height keeps the bird on the water surface", bird.motion.targetZ == bird.z);
        ai.Reset(); Check("reset restores an attackable plainstrider", !(bird.flags & UNIT_FLAG_NON_ATTACKABLE));
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
