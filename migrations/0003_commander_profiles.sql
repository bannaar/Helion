create table if not exists commander_profiles (
  user_id text primary key,
  commander_name text not null default 'JAMESON',
  allegiance text not null default 'independent',
  faction text not null default 'free-traders',
  standing integer not null default 0,
  experience integer not null default 0,
  credits integer not null default 1500,
  ship_id text not null default 'sidewinder',
  inventory jsonb not null default '{}'::jsonb,
  reputation jsonb not null default '{}'::jsonb,
  notes text not null default '',
  save_state jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

create index if not exists commander_profiles_updated_idx
  on commander_profiles (updated_at desc);
