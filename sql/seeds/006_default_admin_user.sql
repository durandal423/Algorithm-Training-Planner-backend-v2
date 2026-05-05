-- Default admin seed. Keep this migration last so auth tables already exist and
-- all schema-level role/status constraints are in place.
WITH existing AS (
    UPDATE users
    SET username = 'admin',
        display_name = 'admin',
        password_hash = '$argon2id$v=19$m=65536,t=2,p=1$8WFzYQb5XXRuSTNdsh09qw$oXcwO0Ylj4xoLxnjk+SHu4YNwT8SrI5xKKOKfTLpPEc',
        role = 'admin',
        status = 'active'
    WHERE lower(username) = 'admin'
    RETURNING id
)
INSERT INTO users(username, email, display_name, password_hash, role, status)
SELECT
    'admin',
    NULL,
    'admin',
    '$argon2id$v=19$m=65536,t=2,p=1$8WFzYQb5XXRuSTNdsh09qw$oXcwO0Ylj4xoLxnjk+SHu4YNwT8SrI5xKKOKfTLpPEc',
    'admin',
    'active'
WHERE NOT EXISTS (SELECT 1 FROM existing);
