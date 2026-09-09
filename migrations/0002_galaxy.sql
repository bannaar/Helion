-- Persistent galaxy: shared markets, news, and anonymous traffic.
-- Unowned rows (no user_id). No personal data.

create table if not exists galaxy_clock (
  id integer primary key check (id = 1),
  tick integer not null default 0,
  last_tick_at timestamptz not null default now()
);

insert into galaxy_clock (id, tick)
  values (1, 0)
  on conflict (id) do nothing;

create table if not exists markets (
  system_id text not null,
  commodity text not null,
  stock integer not null,
  price integer not null,
  primary key (system_id, commodity)
);

create table if not exists dispatches (
  id serial primary key,
  tick integer not null,
  system_id text,
  headline text not null,
  created_at timestamptz not null default now()
);

create table if not exists traffic (
  id serial primary key,
  system_id text not null,
  kind text not null,
  detail text not null,
  created_at timestamptz not null default now()
);

create index if not exists traffic_created_idx on traffic (created_at desc);
create index if not exists dispatches_created_idx on dispatches (created_at desc);
