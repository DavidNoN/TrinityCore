-- Read-only: each row must return passed=1 after applying the world update.
SELECT expected.entry, c.name,
    EXISTS (SELECT 1 FROM smart_scripts s
        WHERE s.entryorguid=expected.entry AND s.source_type=0
          AND s.event_type=2 AND s.event_param1=0 AND s.event_param2=15
          AND s.event_chance=100 AND (s.event_flags & 1)=1
          AND s.action_type=25 AND s.action_param1=1) AS passed
FROM (SELECT 3106 AS entry UNION ALL SELECT 4818 UNION ALL SELECT 4819 UNION ALL SELECT 28608) expected
JOIN creature_template c ON c.entry=expected.entry;

SELECT entry, name,
    (AIName='' AND ScriptName='npc_black_blood_of_draenor' AND (mechanic_immune_mask & 16)=16) AS passed
FROM creature_template WHERE entry=23286;

SELECT entry, name, (AIName='' AND ScriptName='npc_wayward_plainstrider') AS passed
FROM creature_template WHERE entry=39337;

SELECT 'existing NPC spells preserved' AS check_name,
    (EXISTS (SELECT 1 FROM smart_scripts WHERE entryorguid=3106 AND source_type=0 AND action_type=11 AND action_param1=78744)
     AND EXISTS (SELECT 1 FROM smart_scripts WHERE entryorguid=4819 AND source_type=0 AND action_type=11 AND action_param1=8382)
     AND EXISTS (SELECT 1 FROM smart_scripts WHERE entryorguid=4818 AND source_type=0 AND action_type=11 AND action_param1=7164)
     AND EXISTS (SELECT 1 FROM smart_scripts WHERE entryorguid=4818 AND source_type=0 AND action_type=11 AND action_param1=22691)
     AND EXISTS (SELECT 1 FROM smart_scripts WHERE entryorguid=28608 AND source_type=0 AND action_type=11 AND action_param1=15498)) AS passed;
