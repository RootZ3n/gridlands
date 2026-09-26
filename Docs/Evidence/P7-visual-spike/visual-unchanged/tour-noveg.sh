#!/usr/bin/env bash
# The review tour with instanced meshes (vegetation) hidden: $1 = project root, $2 = output dir.
ROOT="$1"; OUT="$2"; UE=/pehverse/engines/UE_5.8.3/Engine/Binaries/Linux/UnrealEditor
SHOTS="$ROOT/Saved/Screenshots/LinuxEditor"; rm -rf "$SHOTS" "$OUT"; mkdir -p "$OUT"; rm -f "$ROOT/Saved/SaveGames/Gridlands/world.json"
timeout --signal=INT --kill-after=30 240 "$UE" "$ROOT/Gridlands.uproject" -game -GLNewWorld -RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -nosound -unattended -NoP4 \
	-ExecCmds="ShowFlag.InstancedStaticMeshes 0, ShowFlag.InstancedGrass 0, gl.Style.Tour" >/dev/null 2>&1
mapfile -t NAMES < <(grep -oE "gl\.Style\.Shot [0-9]+ [^ ]+" "$ROOT/Saved/Logs/Gridlands.log" | awk '{print $3}' | tr -d '\r')
mapfile -t FILES < <(ls -1tr "$SHOTS"/*.png 2>/dev/null)
for i in "${!FILES[@]}"; do python3 -c "import sys; from PIL import Image; Image.open(sys.argv[1]).convert('RGB').resize((1280,720), Image.LANCZOS).save(sys.argv[2], quality=88)" "${FILES[$i]}" "$OUT/${NAMES[$i]}.jpg"; done
echo "$OUT: ${#FILES[@]} shots"
