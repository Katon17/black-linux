#!/usr/bin/env bash
# Create github.com/<user>/black-linux, push the source and upload the .deb as a release.
# Usage:  bash publish-github.sh [user] [repo]
#   Easiest: GitHub CLI.  sudo apt install gh && gh auth login   (then run this script)
#   Or a token:  GITHUB_TOKEN=ghp_xxx bash publish-github.sh
#   (fine-grained: "Administration: write" + "Contents: write", or classic "repo" scope)
set -euo pipefail
USER_NAME="${1:-Katon17}"
REPO="${2:-black-linux}"
cd "$(dirname "$0")"
VER="$(cat VERSION)"
DEB="release/black-linux_${VER}_amd64.deb"
[ -f "$DEB" ] || { echo "missing $DEB (run build-deb.sh first)"; exit 1; }
[ -d .git ] || { git init -q -b main; git add -A; git -c user.name="$USER_NAME" -c user.email="$USER_NAME@users.noreply.github.com" commit -q -m "black-linux $VER"; }

if [ -z "${GITHUB_TOKEN:-}" ] && command -v gh >/dev/null && gh auth status >/dev/null 2>&1; then
  echo "== GitHub CLI"
  if gh repo view "$USER_NAME/$REPO" >/dev/null 2>&1; then
    git remote get-url origin >/dev/null 2>&1 || git remote add origin "https://github.com/$USER_NAME/$REPO.git"
    git push -u origin HEAD:main
  else
    gh repo create "$USER_NAME/$REPO" --public \
      --description "Native Linux port of Black (Xbox, 2006) - static recompilation, Vulkan + SDL3, .deb for Ubuntu/Mint. Bring your own game." \
      --source=. --remote=origin --push
  fi
  gh repo edit "$USER_NAME/$REPO" --add-topic linux,recompilation,xbox,vulkan,sdl3,deb,black >/dev/null 2>&1 || true
  if gh release view "v$VER" -R "$USER_NAME/$REPO" >/dev/null 2>&1; then
    gh release upload "v$VER" "$DEB" --clobber -R "$USER_NAME/$REPO"
  else
    gh release create "v$VER" "$DEB" -R "$USER_NAME/$REPO" --title "black-linux $VER" -F RELEASE_NOTES.md --target main
  fi
  echo "== done: https://github.com/$USER_NAME/$REPO"
  exit 0
fi
: "${GITHUB_TOKEN:?no GitHub CLI login: run  gh auth login  or set GITHUB_TOKEN=<token>}"
API=https://api.github.com
H=(-H "Authorization: Bearer $GITHUB_TOKEN" -H "Accept: application/vnd.github+json" -H "X-GitHub-Api-Version: 2022-11-28")

echo "== repo $USER_NAME/$REPO"
code=$(curl -s -o /dev/null -w '%{http_code}' "${H[@]}" "$API/repos/$USER_NAME/$REPO")
if [ "$code" = 404 ]; then
  curl -sf "${H[@]}" -X POST "$API/user/repos" -d "{\"name\":\"$REPO\",\"description\":\"Native Linux port of Black (Xbox, 2006) - static recompilation, Vulkan + SDL3, .deb for Ubuntu/Mint. Bring your own game.\",\"has_wiki\":false}" >/dev/null
  echo "   created"
else
  echo "   exists (HTTP $code)"
fi

echo "== push"
git push -q "https://x-access-token:$GITHUB_TOKEN@github.com/$USER_NAME/$REPO.git" HEAD:main
git tag -f "v$VER" >/dev/null
git push -q -f "https://x-access-token:$GITHUB_TOKEN@github.com/$USER_NAME/$REPO.git" "v$VER"

echo "== release v$VER"
rel=$(curl -s "${H[@]}" "$API/repos/$USER_NAME/$REPO/releases/tags/v$VER")
id=$(printf '%s' "$rel" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("id",""))')
if [ -z "$id" ]; then
  body=$(python3 - "$VER" <<'PY'
import json,sys
v=sys.argv[1]
print(json.dumps({"tag_name":"v"+v,"name":"black-linux "+v,"body":open("RELEASE_NOTES.md").read(),"draft":False,"prerelease":False}))
PY
)
  id=$(curl -sf "${H[@]}" -X POST "$API/repos/$USER_NAME/$REPO/releases" -d "$body" | python3 -c 'import json,sys; print(json.load(sys.stdin)["id"])')
fi
name=$(basename "$DEB")
old=$(curl -s "${H[@]}" "$API/repos/$USER_NAME/$REPO/releases/$id/assets" | python3 -c "import json,sys; print(next((str(a['id']) for a in json.load(sys.stdin) if a['name']=='$name'),''))")
[ -n "$old" ] && curl -sf "${H[@]}" -X DELETE "$API/repos/$USER_NAME/$REPO/releases/assets/$old"
curl -sf "${H[@]}" -H "Content-Type: application/vnd.debian.binary-package" --data-binary @"$DEB" \
  "https://uploads.github.com/repos/$USER_NAME/$REPO/releases/$id/assets?name=$name" >/dev/null
echo "== done: https://github.com/$USER_NAME/$REPO/releases/tag/v$VER"
