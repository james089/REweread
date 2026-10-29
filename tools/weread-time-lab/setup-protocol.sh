#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RUNTIME="$ROOT/downloads/time-lab-runtime"
command -v lua >/dev/null || { echo 'Lua 5.3 or later is required.' >&2; exit 1; }
mkdir -p "$RUNTIME"
TEMP="$(mktemp "$RUNTIME/protocol.XXXXXX")"
trap 'rm -f "$TEMP"' EXIT
# Local runtime dependency only: do not redistribute or add this source to Git.
curl -fsSL 'https://raw.githubusercontent.com/QiuYukang/weread.koplugin/cc100e0de301ef973d3195afeb47c04931b0b261/weread/lib/protocol.lua' -o "$TEMP"
node --input-type=module - "$TEMP" <<'JS'
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
const hash=createHash('sha256').update(readFileSync(process.argv[2])).digest('hex');
if(hash!=='cb98b9fd6c7fe7cf7dadfdc62e8d368c1373943b2eafef6d7a7cf132869db171')throw Error('Protocol checksum mismatch');
JS
mv "$TEMP" "$RUNTIME/protocol.lua"
echo 'Pinned local Lua protocol runtime ready.'
