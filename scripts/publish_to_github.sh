#!/usr/bin/env bash
#
# publish_to_github.sh — one-shot: commit current state and push to a new GitHub repo.
#
# Prerequisites (on your machine, not in Cowork sandbox):
#   1. gh CLI installed          ->  brew install gh
#   2. gh authenticated          ->  gh auth login    (choose GitHub.com, HTTPS, browser)
#   3. Run this from the repo root.
#
# Usage:
#   ./scripts/publish_to_github.sh <repo-name> [public|private]
#   ./scripts/publish_to_github.sh claude-buddy-pico private
#
# What it does:
#   - clears any stale .git/index.lock
#   - sets local git user.name/email if unset
#   - stages and commits current working tree (if there are changes)
#   - creates a new GitHub repo under your authenticated account
#   - wires up `origin` and pushes `main`
#   - optionally enables GitHub Pages from the /docs folder on main
#
set -euo pipefail

REPO_NAME="${1:-claude-buddy-pico}"
VISIBILITY="${2:-private}"          # public | private
ENABLE_PAGES="${ENABLE_PAGES:-0}"   # set to 1 to turn on GitHub Pages from /docs

if [[ "$VISIBILITY" != "public" && "$VISIBILITY" != "private" ]]; then
  echo "visibility must be 'public' or 'private', got: $VISIBILITY" >&2
  exit 1
fi

# 0. preflight
command -v gh  >/dev/null || { echo "gh CLI not found. brew install gh" >&2; exit 1; }
command -v git >/dev/null || { echo "git not found" >&2; exit 1; }
gh auth status >/dev/null 2>&1 || { echo "gh not authenticated. run: gh auth login" >&2; exit 1; }

cd "$(git rev-parse --show-toplevel)"

# 1. stale lock
if [[ -f .git/index.lock ]]; then
  echo ">> removing stale .git/index.lock"
  rm -f .git/index.lock
fi

# 2. identity
if ! git config user.name  >/dev/null; then git config user.name  "Amar"; fi
if ! git config user.email >/dev/null; then git config user.email "amar.gandhi@me.com"; fi

# 3. branch
git symbolic-ref HEAD refs/heads/main >/dev/null 2>&1 || git checkout -B main

# 4. stage + commit (only if dirty)
if [[ -n "$(git status --porcelain)" ]]; then
  echo ">> staging and committing current state"
  git add -A
  git commit -m "Initial public snapshot of Claude Pico Buddy

- firmware, 3D case files, docs, examples
- license, notice, contributing
"
else
  echo ">> working tree already clean"
fi

# 5. create repo on GitHub if it doesn't exist yet
GH_USER="$(gh api user -q .login)"
FULL="$GH_USER/$REPO_NAME"
if gh repo view "$FULL" >/dev/null 2>&1; then
  echo ">> repo $FULL already exists on GitHub"
else
  echo ">> creating repo $FULL ($VISIBILITY)"
  gh repo create "$FULL" "--$VISIBILITY" --source=. --remote=origin --push
  echo ">> done"
  if [[ "$ENABLE_PAGES" == "1" ]]; then
    echo ">> enabling GitHub Pages from /docs on main"
    gh api -X POST "repos/$FULL/pages" \
      -f "source[branch]=main" \
      -f "source[path]=/docs" >/dev/null \
      && echo ">> Pages enabled. Site URL will be: https://$GH_USER.github.io/$REPO_NAME/"
  fi
  echo ">> repo: https://github.com/$FULL"
  exit 0
fi

# 6. if repo already existed, make sure origin is wired and push
if ! git remote get-url origin >/dev/null 2>&1; then
  git remote add origin "https://github.com/$FULL.git"
fi
git push -u origin main
echo ">> repo: https://github.com/$FULL"
