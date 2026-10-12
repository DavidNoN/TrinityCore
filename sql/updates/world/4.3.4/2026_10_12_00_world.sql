-- Restore the 15% flee thresholds documented by the existing SmartAI scripts.
-- Guard the previous values so custom thresholds are not overwritten.
UPDATE `smart_scripts`
SET `event_param2` = 15
WHERE `entryorguid` = 2009 AND `source_type` = 0 AND `id` = 0
  AND `event_type` = 2 AND `action_type` = 25
  AND `event_param1` = 0 AND `event_param2` = 30000;

UPDATE `smart_scripts`
SET `event_param2` = 15
WHERE `entryorguid` = 1920 AND `source_type` = 0 AND `id` = 2
  AND `event_type` = 2 AND `action_type` = 25
  AND `event_param1` = 0 AND `event_param2` = 0;
