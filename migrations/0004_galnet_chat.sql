create table if not exists galnet_messages (
  id bigserial primary key,
  channel text not null check (channel in ('LOCAL', 'FACTION', 'GALACTIC')),
  commander text not null,
  text text not null check (char_length(text) between 1 and 240),
  created_at timestamptz not null default now()
);

create index if not exists galnet_messages_created_idx on galnet_messages (created_at desc);
