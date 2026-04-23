DELETE FROM notification_deliveries
WHERE event_type::text = 'system';

DELETE FROM notification_preferences
WHERE event_type::text = 'system';
