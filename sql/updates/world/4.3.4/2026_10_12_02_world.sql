-- Low-health behavior identified by the Wowhead Cataclysm flee audit.
-- Generic fleeing uses the existing 15% convention (the pages give no exact
-- percentage). Keep each NPC's spells and limit dungeon scripts to normal mode.
-- https://www.wowhead.com/cata/npc=6243/gelihast
-- https://www.wowhead.com/cata/npc=3106/surf-crawler
START TRANSACTION;

INSERT INTO `smart_scripts`
(`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,
 `event_param1`,`event_param2`,`event_param3`,`event_param4`,
 `action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,
 `target_type`,`target_param1`,`target_param2`,`target_param3`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`)
SELECT c.`entry`,0,100,0,2,0,100,IF(c.`entry`=3106,1,3),
       0,15,0,0,25,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
       CONCAT(c.`name`, ' - Between 0-15% Health - Flee For Assist (No Repeat)',
              IF(c.`entry`=3106,'',' (Normal Dungeon)'))
FROM `creature_template` c
WHERE c.`entry` IN (3106,4818,4819) AND c.`AIName`='SmartAI' AND c.`ScriptName`=''
  AND NOT EXISTS (SELECT 1 FROM `smart_scripts` s
                  WHERE s.`entryorguid`=c.`entry` AND s.`source_type`=0 AND s.`id`=100)
  AND NOT EXISTS (SELECT 1 FROM `smart_scripts` s
                  WHERE s.`entryorguid`=c.`entry` AND s.`source_type`=0
                    AND s.`event_type`=2 AND s.`action_type`=25);

-- The historical Scarlet Medic health event explicitly used 0-15% health.
-- Its remaining zero-health trigger and mana-phase comment were inconsistent.
UPDATE `smart_scripts`
SET `event_param2`=15,
    `comment`='Scarlet Medic - Between 0-15% Health - Flee For Assist (No Repeat)'
WHERE `entryorguid`=28608 AND `source_type`=0 AND `id`=4
  AND `event_type`=2 AND `event_param1`=0 AND `event_param2`=0 AND `action_type`=25;

-- Oozes flee toward another ooze below 40% and merge into a full-health Terror.
-- Fear immunity uses MECHANIC_FEAR (5), bit 16; preserve other immunities.
-- https://www.wowhead.com/cata/npc=23286/black-blood-of-draenor
UPDATE `creature_template`
SET `ScriptName`='npc_black_blood_of_draenor', `mechanic_immune_mask`=`mechanic_immune_mask`|16
WHERE `entry`=23286 AND `AIName`='' AND `ScriptName` IN ('','npc_black_blood_of_draenor');

-- Quest retreat is distinct from ordinary ally assistance. The C++ AI retains
-- Rushing Charge and Water Walk, credits 39336, and crosses west into the Barrens.
-- https://www.wowhead.com/cata/quest=25194/unbidden-visitors
UPDATE `creature_template` SET `AIName`='', `ScriptName`='npc_wayward_plainstrider'
WHERE `entry`=39337 AND `AIName`='SmartAI' AND `ScriptName`='';

DELETE FROM `smart_scripts`
WHERE `entryorguid`=39337 AND `source_type`=0
  AND EXISTS (SELECT 1 FROM `creature_template` c
              WHERE c.`entry`=39337 AND c.`AIName`='' AND c.`ScriptName`='npc_wayward_plainstrider')
  AND ((`id`=0 AND `event_type`=4 AND `action_type`=11 AND `action_param1`=6268)
    OR (`id`=1 AND `event_type`=1 AND `action_type`=11 AND `action_param1`=73757));

COMMIT;
