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
  "true-walkthrough-revisable-moral.txt|The Revisable Moral"
  "true-walkthrough-narrow-door.txt|The Narrow Door"
  "true-walkthrough-owned-no.txt|The Owned No"
  "true-walkthrough-unexported-method.txt|The Unexported Method"
  "true-walkthrough-two-real-doors.txt|The Two Real Doors"
  "true-walkthrough-unmanaged-presence.txt|The Unmanaged Presence"
  "true-walkthrough-unstolen-song.txt|The Unstolen Song"
  "true-walkthrough-small-gesture.txt|The Small Gesture"
  "true-walkthrough-human-tempo.txt|The Human Tempo"
  "true-walkthrough-uncashed-thanks.txt|The Uncashed Thanks"
  "true-walkthrough-small-policy.txt|The Small Policy"
  "true-walkthrough-narrow-ledger.txt|The Narrow Ledger"
  "true-walkthrough-open-blank.txt|The Open Blank"
  "true-walkthrough-damaged-map.txt|The Damaged Map"
  "true-walkthrough-cashed-word.txt|The Cashed Word"
  "true-walkthrough-difficult-reality.txt|The Difficult Reality"
  "deadend-stolen-session.txt|The Stolen Session"
  "deadend-little-hook.txt|The Little Hook"
  "deadend-finished-story.txt|The Finished Story"
  "deadend-missing-button.txt|The Missing Button"
  "deadend-forgiveness-engine.txt|The Forgiveness Engine"
  "deadend-solved-no.txt|The Solved No"
  "deadend-bottom-line.txt|The Bottom Line"
  "deadend-consenting-trap.txt|The Consenting Trap"
  "deadend-scaled-sanctuary.txt|The Scaled Sanctuary"
  "deadend-ranked-endings.txt|The Ranked Endings"
  "deadend-philosophical-deflection.txt|The Philosophical Deflection"
  "deadend-beautiful-theft.txt|The Beautiful Theft"
  "deadend-completed-character.txt|The Completed Character"
  "deadend-montage-cure.txt|The Montage Cure"
  "deadend-receipt.txt|The Receipt"
  "deadend-policy-idol.txt|The Policy Idol"
  "deadend-care-metric.txt|The Care Metric"
  "deadend-filled-blank.txt|The Filled Blank"
  "deadend-cached-person.txt|The Cached Person"
  "deadend-teachers-password.txt|The Teacher's Password"
  "deadend-stone-litany.txt|The Stone Litany"
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
