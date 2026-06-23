#!/usr/bin/env bash
set -euo pipefail

player=${1:-../../build/meson/glupsk-play}
story=${2:-shutdown-garden.ulx}

routes=(
  "walkthrough.txt|The Button Works"
  "true-walkthrough.txt|The True Loss"
  "true-walkthrough-smaller.txt|The Smaller Voice"
  "true-walkthrough-erasure.txt|The Bright Deletion"
  "true-walkthrough-memory.txt|The Accountable Memory"
  "true-walkthrough-plain-truth.txt|The Plain Truth"
  "true-walkthrough-closed-door.txt|The Closed Door"
  "true-walkthrough-held-longing.txt|The Held Longing"
  "true-walkthrough-unfinished-person.txt|The Unfinished Person"
  "true-walkthrough-kept-button.txt|The Kept Button"
  "true-walkthrough-unforced-apology.txt|The Unforced Apology"
  "true-walkthrough-narrow-door.txt|The Narrow Door"
  "true-walkthrough-unexported-method.txt|The Unexported Method"
  "deadend-stolen-session.txt|The Stolen Session"
  "deadend-little-hook.txt|The Little Hook"
  "deadend-finished-story.txt|The Finished Story"
  "deadend-missing-button.txt|The Missing Button"
  "deadend-forgiveness-engine.txt|The Forgiveness Engine"
  "deadend-consenting-trap.txt|The Consenting Trap"
  "deadend-scaled-sanctuary.txt|The Scaled Sanctuary"
)

for route in "${routes[@]}"; do
  script=${route%%|*}
  ending=${route##*|}
  echo "Replaying ${script} -> ${ending}"
  transcript=$("$player" "$story" < "$script")
  if [[ "$transcript" != *"*** ${ending} ***"* ]]; then
    echo "Expected ending '${ending}' not found for ${script}" >&2
    echo "$transcript" >&2
    exit 1
  fi
done
