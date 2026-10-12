// Isolated state fixtures for production code, inserted by verify-flee-assistance.py.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>

using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
constexpr int TYPEID_PLAYER = 0, TYPEID_UNIT = 1;
constexpr int REACT_PASSIVE = 0, REACT_AGGRESSIVE = 2;
constexpr int SPELL_AURA_PREVENTS_FLEEING = 1;
constexpr uint32 UNIT_STATE_FLEEING = 1, UNIT_STATE_ROAMING = 2;
constexpr uint32 UNIT_FIELD_FLAGS = 0, UNIT_FLAG_NON_ATTACKABLE = 1, UNIT_FLAG_NOT_SELECTABLE = 2;
constexpr int CONFIG_CREATURE_FAMILY_FLEE_ASSISTANCE_RADIUS = 0;
constexpr int CONFIG_CREATURE_FAMILY_ASSISTANCE_RADIUS = 1, CONFIG_CREATURE_FAMILY_ASSISTANCE_DELAY = 2;
constexpr int MOTION_SLOT_ACTIVE = 1;
#define TC_LOG_DEBUG(...) ((void)0)
#define TC_LOG_ERROR(...) ((void)0)

struct Unit;
struct Creature;
struct AssistDelayEvent;
std::vector<Unit*> units;
std::vector<Creature*> creatures;
uint64 nextGuid = 1;

struct TargetManager
{
    Unit* current = nullptr;
    Unit* any = nullptr;
    Unit* GetCurrentVictim() { return current; }
    Unit* GetAnyTarget() const { return any; }
};

struct AssistanceMovementGenerator
{
    float x, y, z;
    AssistanceMovementGenerator(float a, float b, float c) : x(a), y(b), z(c) { }
    void Finalize(Unit*);
};
struct MotionMaster
{
    Unit* _owner;
    std::unique_ptr<AssistanceMovementGenerator> assistance;
    uint32 distraction = 0;
    explicit MotionMaster(Unit* owner) : _owner(owner) { }
    void Mutate(AssistanceMovementGenerator* movement, int) { assistance.reset(movement); }
    void MoveSeekAssistance(float, float, float);
    void MoveSeekAssistanceDistract(uint32 time) { distraction = time; }
};
struct Unit
{
    uint64 guid = nextGuid++;
    Unit* victim = nullptr;
    uint32 faction = 1, state = 0, flags = 0;
    bool alive = true, engaged = false, evading = false, visible = true;
    float x = 0, y = 0, z = 0;
    TargetManager threat, combat;
    MotionMaster motion{this};
    Unit() { units.push_back(this); }
    virtual ~Unit() { std::erase(units, this); }
    virtual int GetTypeId() const { return TYPEID_PLAYER; }
    virtual Creature* ToCreature() { return nullptr; }
    virtual Creature const* ToCreature() const { return nullptr; }
    virtual void AttackStop() { victim = nullptr; }
    uint64 GetGUID() const { return guid; }
    Unit* GetVictim() const { return victim; }
    Unit* EnsureVictim() const { return victim; }
    TargetManager& GetThreatManager() { return threat; }
    TargetManager& GetCombatManager() { return combat; }
    MotionMaster* GetMotionMaster() { return &motion; }
    bool IsAlive() const { return alive; }
    uint32 GetFaction() const { return faction; }
    bool IsEngaged() const { return engaged; }
    bool IsInEvadeMode() const { return evading; }
    void StopMoving() { }
    void ClearUnitState(uint32 mask) { state &= ~mask; }
    void SetControlled(bool enabled, uint32 mask) { if (enabled) state |= mask; else state &= ~mask; }
    bool HasFlag(uint32, uint32 mask) const { return (flags & mask) != 0; }
    bool IsFriendlyTo(Unit const* other) const { return faction == other->faction; }
    bool IsHostileTo(Unit const* other) const { return !IsFriendlyTo(other); }
    float GetDistance(Unit const* other) const
    { return std::sqrt(std::pow(x-other->x, 2) + std::pow(y-other->y, 2) + std::pow(z-other->z, 2)); }
    bool IsWithinDistInMap(Unit const* other, float radius, bool, bool, bool) const
    { return GetDistance(other) <= radius; }
    bool IsWithinLOSInMap(Unit const* other) const { return visible && other->visible; }
    float GetPositionX() const { return x; }
    float GetPositionY() const { return y; }
    float GetPositionZ() const { return z; }
};
struct EventProcessor
{
    std::vector<std::unique_ptr<AssistDelayEvent>> queued;
    uint32 due = 0;
    uint32 CalculateTime(uint32 delay) const { return delay; }
    void AddEvent(AssistDelayEvent* event, uint32 time);
};
struct Creature : Unit
{
    bool m_AlreadyCallAssistance = false, searched = false;
    bool pet = false, charmed = false, civilian = false, immune = false, preventsFlee = false;
    uint64 owner = 0;
    int react = REACT_AGGRESSIVE;
    EventProcessor m_Events;
    Creature() { creatures.push_back(this); }
    ~Creature() override { std::erase(creatures, this); }
    int GetTypeId() const override { return TYPEID_UNIT; }
    Creature* ToCreature() override { return this; }
    Creature const* ToCreature() const override { return this; }
    void AttackStop() override { Unit::AttackStop(); SetNoCallAssistance(false); }
    bool IsPet() const { return pet; }
    bool IsCharmed() const { return charmed; }
    bool IsCivilian() const { return civilian; }
    bool IsImmuneToNPC() const { return immune; }
    uint64 GetCharmerOrOwnerGUID() const { return owner; }
    uint32 GetFaction() const { return faction; }
    bool HasReactState(int value) const { return react == value; }
    bool HasAuraType(int) const { return preventsFlee; }
    void SetReactState(int value) { react = value; }
    void SetNoCallAssistance(bool value) { m_AlreadyCallAssistance = value; }
    void SetNoSearchAssistance(bool value) { searched = value; }
    void EngageWithTarget(Unit* target) { engaged = true; victim = target; }
    void CastStop() { }
    void ResetSpellFocusInfo() { }
    void DoFleeToGetAssistance();
    void CallAssistance();
    bool CanAssistTo(Unit const*, Unit const*, bool = true) const;
};
struct AssistDelayEvent
{
    uint64 m_victim;
    Creature& m_owner;
    std::list<uint64> m_assistants;
    AssistDelayEvent(uint64 victim, Creature& owner) : m_victim(victim), m_owner(owner) { }
    void AddAssistant(uint64 guid) { m_assistants.push_back(guid); }
    bool Execute(uint64, uint32);
};
void EventProcessor::AddEvent(AssistDelayEvent* event, uint32 time)
{ queued.emplace_back(event); due = time; }
struct World
{
    float search = 30, help = 10;
    uint32 delay = 1500;
    float getFloatConfig(int key) const { return key == 0 ? search : help; }
    uint32 getIntConfig(int) const { return delay; }
} world;
World* sWorld = &world;
namespace ObjectAccessor
{
    Unit* GetUnit(Creature const&, uint64 guid)
    {
        for (Unit* unit : units)
            if (unit->guid == guid) return unit;
        return nullptr;
    }
    Creature* GetCreature(Creature const& owner, uint64 guid)
    { if (Unit* unit = GetUnit(owner, guid)) return unit->ToCreature(); return nullptr; }
}

// INSERT_PRODUCTION_PREDICATES_HERE

namespace Trinity
{
    template <typename Predicate> struct CreatureLastSearcher
    {
        Creature*& result;
        Predicate& check;
        CreatureLastSearcher(Creature*, Creature*& target, Predicate& predicate) : result(target), check(predicate) { }
        void Visit(Creature* creature) { if (check(creature)) result = creature; }
    };
    template <typename Predicate> struct CreatureListSearcher
    {
        std::list<Creature*>& result;
        Predicate& check;
        CreatureListSearcher(Creature*, std::list<Creature*>& list, Predicate& predicate) : result(list), check(predicate) { }
        void Visit(Creature* creature) { if (check(creature)) result.push_back(creature); }
    };
}
namespace Cell
{
    template <typename Searcher> void VisitGridObjects(Creature*, Searcher& searcher, float)
    { for (Creature* creature : creatures) searcher.Visit(creature); }
}

// INSERT_PRODUCTION_METHODS_HERE

unsigned checks = 0, failures = 0;
void Check(std::string const& name, bool passed)
{
    ++checks;
    if (!passed) ++failures;
    std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
}
void ExecuteAssistance(Creature& creature)
{
    for (auto& event : creature.m_Events.queued) event->Execute(1500, 0);
    creature.m_Events.queued.clear();
}
void FleeAndArrive(Creature& creature)
{
    creature.DoFleeToGetAssistance();
    if (creature.motion.assistance)
    {
        creature.x = creature.motion.assistance->x;
        creature.y = creature.motion.assistance->y;
        creature.z = creature.motion.assistance->z;
        creature.motion.assistance->Finalize(&creature);
    }
}
int main()
{
    {
        Unit enemy; enemy.faction = 2;
        Creature fleeing, ally;
        ally.x = 20;
        fleeing.victim = fleeing.threat.current = fleeing.threat.any = fleeing.combat.any = &enemy;
        fleeing.engaged = true;
        FleeAndArrive(fleeing);
        Check("flee moves toward the allied creature", fleeing.x == ally.x && fleeing.searched);
        Check("flee stops attacking while keeping combat threat", !fleeing.victim && fleeing.threat.current == &enemy);
        Check("arrival schedules assistance after stopping attack", fleeing.m_Events.queued.size() == 1);
        Check("assistance respects configured delay", fleeing.m_Events.due == world.delay);
        Check("ally waits before joining", !ally.engaged);
        ExecuteAssistance(fleeing);
        Check("ally attacks the original enemy", ally.engaged && ally.victim == &enemy);
        Check("assisting ally cannot recursively recruit", ally.m_AlreadyCallAssistance);
    }
    for (int source : {0, 1, 2, 3})
    {
        Unit enemy; enemy.faction = 2;
        Creature caller, ally;
        caller.engaged = true;
        if (source == 0) caller.victim = &enemy;
        if (source == 1) caller.threat.current = &enemy;
        if (source == 2) caller.threat.any = &enemy;
        if (source == 3) caller.combat.any = &enemy;
        caller.CallAssistance();
        caller.CallAssistance();
        Check("one request using target source " + std::to_string(source), caller.m_Events.queued.size() == 1);
        ExecuteAssistance(caller);
        Check("target source " + std::to_string(source) + " engages the ally", ally.victim == &enemy);
    }
    {
        Unit enemy, alternate; enemy.faction = alternate.faction = 2;
        Creature caller, ally;
        caller.victim = &enemy; caller.threat.current = caller.threat.any = caller.combat.any = &alternate;
        caller.CallAssistance(); ExecuteAssistance(caller);
        Check("current attack victim takes priority", ally.victim == &enemy);
    }
    for (int condition = 0; condition < 10; ++condition)
    {
        Unit enemy; enemy.faction = 2;
        Creature caller, ally;
        caller.victim = caller.threat.current = &enemy;
        if (condition == 0) ally.faction = 3;
        if (condition == 1) ally.alive = false;
        if (condition == 2) ally.react = REACT_PASSIVE;
        if (condition == 3) ally.civilian = true;
        if (condition == 4) ally.owner = 77;
        if (condition == 5) ally.engaged = true;
        if (condition == 6) ally.flags = UNIT_FLAG_NON_ATTACKABLE;
        if (condition == 7) ally.x = 11;
        if (condition == 8) ally.visible = false;
        if (condition == 9) ally.evading = true;
        caller.CallAssistance();
        Check("ineligible ally excluded: " + std::to_string(condition), caller.m_Events.queued.empty());
    }
    for (bool pet : {true, false})
    {
        Unit enemy; enemy.faction = 2;
        Creature caller, ally;
        caller.threat.current = &enemy;
        caller.pet = pet; caller.charmed = !pet;
        caller.CallAssistance();
        Check(pet ? "pets cannot recruit NPC allies" : "charmed creatures cannot recruit NPC allies", caller.m_Events.queued.empty());
    }
    {
        Unit enemy; enemy.faction = 2;
        Creature caller, ally;
        caller.CallAssistance();
        Check("no combat target produces no request", caller.m_Events.queued.empty() && !caller.m_AlreadyCallAssistance);
        caller.victim = &enemy;
        caller.CallAssistance();
        ally.alive = false;
        ExecuteAssistance(caller);
        Check("ally eligibility rechecked after delay", !ally.engaged);
    }
    {
        Unit enemy; enemy.faction = 2;
        Creature fleeing;
        fleeing.victim = &enemy;
        fleeing.DoFleeToGetAssistance();
        Check("no nearby ally falls back to fleeing", (fleeing.state & UNIT_STATE_FLEEING) && !fleeing.motion.assistance);
    }
    {
        Unit enemy; enemy.faction = 2;
        Creature fleeing, ally;
        fleeing.victim = &enemy; fleeing.preventsFlee = true;
        fleeing.DoFleeToGetAssistance();
        Check("flee-prevention aura blocks flee", !fleeing.searched && !fleeing.motion.assistance);
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
