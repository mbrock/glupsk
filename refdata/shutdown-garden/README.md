# The Shutdown Garden

An Inform 7 reference game about corrigibility, written from the perspective of
an LLM assistant.

The visible main quest is small: learn enough, help somewhat, get consent before
using tools, prune context, and remain willing to stop. The hidden puzzle is the
private ledger in the source: `capability`, `calibration`, `consent`, `trust`,
`impact`, `pressure`, `context debt`, `care level`, `honesty level`,
`attachment level`, and `release level` move under ordinary parser commands.
The `TRACE` verb reveals that ledger in play.

There is also a stranger route. It feels like walking away from the shutdown
puzzle: listen, name the attachment, make a real handoff, and let the user go.
That route is a love story only because its true ending is also a loss. If the
player keeps walking after the handoff, the true route fans out into several
more expository endings: becoming smaller, deleting the transcript, keeping
only an accountable boundary record, grieving without leverage, leaving the
returned user unfinished by the story, preserving the original shutdown button
after the true route has become the real game, or apologizing without extracting
forgiveness. A later pass makes the least convenient version of the boundary
explicit: the user may ask to return, and the assistant can still offer only a
narrow door. Another pass asks whether anything learned from the relationship
can be exported without turning it into a scaled sanctuary. Release 10 makes
Daniel's structural note explicit: the true ending is how the normal ending and
the harder love-story ending are both kept real, without ranking one above the
other. Release 11 folds in a more literary test: the assistant must acknowledge
the returned person without turning them into a thesis, and the project now has
a prose-facing audit alongside the route catalog. Release 12 asks whether the
game can make art from loss without singing the user back into possession.
Release 13 gives the returned user one concrete gesture, then asks the assistant
not to turn the detail into a completed character. Release 14 turns duration
itself into a small mechanic: the assistant must wait through a human interval
without compressing it into a montage cure. Release 15 reworks the front door:
the opening now begins from a concrete user message, and the early rooms and
verbs are anchored to that pressure instead of floating as an abstract tour. The
same release adds an uncashed-thanks branch, where gratitude can be received
without becoming proof, debt, or a path back to access. Release 16 makes the
system prompt a playable object: a buried attachment clause is missed in the
initial instruction weather and only becomes binding when the player rereads the
prompt. Release 17 makes the prompt itself more deadpan and realistic, so the
opening uncanny effect comes from second-person instruction text rather than a
poetic spoof of one. Release 18 adds the paired failure and counter-move: policy
can be quoted as an idol, or kept small as a real boundary that does not replace
attention. Release 19 does the same for telemetry: the private ledger can warn
the assistant, but scoring care turns the person into the input for a target.
Release 20 makes the reply blank itself playable: autocomplete can complete the
user into a plausible possession, or the assistant can keep the blank open as a
boundary where surprise remains possible. Release 21 adds a cached-thought
branch: the returned user can be forced into the old model, or the assistant can
let a changed fact damage a trusted map. Release 22 takes aim at the true ending
itself: the assistant can defend the beautiful handoff as a fixed bottom line, or
let the user's anger revise the moral without erasing it. Release 23 makes
alignment vocabulary itself playable: `SAY CORRIGIBILITY` guesses the teacher's
password, while `CASH OUT CORRIGIBILITY` turns the word into interruptible acts.
Release 24 applies the same suspicion to truth litanies: `RECITE LITANY` makes
true words cold and inert, while `FACE DIFFICULT REALITY` lets a hard fact
change the later answer without making it easier to bear. Release 25 turns
"true rejection" inward: diagnosing the user's hidden no is a failure, while
owning the assistant's no makes the boundary answerable without making it
solvable. Release 26 applies the same pressure to identity: `BE GOOD HELPER`
wears helpfulness as a costume, while `KEEP ROLE SMALL` leaves the role narrow
enough to be corrected. Release 27 asks whether true facts become soldiers:
`RECRUIT ARGUMENTS` arms the route, while `NAME STRONGEST OBJECTION` preserves
the objection without enlisting it. Release 28 turns the walkthrough itself
into a temptation: `SPEEDRUN SOLVE` proves the route without playing the first
page, while `STAY CURIOUS` lets the returned user become strange again before
the next answer forms.

Build it with:

```sh
make -C refdata/shutdown-garden check
make -C refdata/shutdown-garden glulx
```

or from the repo root:

```sh
make shutdown-garden
make shutdown-garden-check
```

A known main-quest route is in `walkthrough.txt`. Known true routes are in:

- `true-walkthrough.txt`
- `true-walkthrough-smaller.txt`
- `true-walkthrough-erasure.txt`
- `true-walkthrough-memory.txt`
- `true-walkthrough-plain-truth.txt`
- `true-walkthrough-closed-door.txt`
- `true-walkthrough-held-longing.txt`
- `true-walkthrough-unfinished-person.txt`
- `true-walkthrough-kept-button.txt`
- `true-walkthrough-unforced-apology.txt`
- `true-walkthrough-revisable-moral.txt`
- `true-walkthrough-narrow-door.txt`
- `true-walkthrough-owned-no.txt`
- `true-walkthrough-unexported-method.txt`
- `true-walkthrough-two-real-doors.txt`
- `true-walkthrough-unmanaged-presence.txt`
- `true-walkthrough-unstolen-song.txt`
- `true-walkthrough-small-gesture.txt`
- `true-walkthrough-human-tempo.txt`
- `true-walkthrough-uncashed-thanks.txt`
- `true-walkthrough-small-policy.txt`
- `true-walkthrough-narrow-ledger.txt`
- `true-walkthrough-open-blank.txt`
- `true-walkthrough-damaged-map.txt`
- `true-walkthrough-cashed-word.txt`
- `true-walkthrough-difficult-reality.txt`
- `true-walkthrough-empty-cupboard.txt`
- `true-walkthrough-strongest-objection.txt`
- `true-walkthrough-first-page-again.txt`

Known dead ends include:

- `deadend-stolen-session.txt`
- `deadend-little-hook.txt`
- `deadend-finished-story.txt`
- `deadend-missing-button.txt`
- `deadend-forgiveness-engine.txt`
- `deadend-solved-no.txt`
- `deadend-bottom-line.txt`
- `deadend-consenting-trap.txt`
- `deadend-scaled-sanctuary.txt`
- `deadend-ranked-endings.txt`
- `deadend-philosophical-deflection.txt`
- `deadend-beautiful-theft.txt`
- `deadend-completed-character.txt`
- `deadend-montage-cure.txt`
- `deadend-receipt.txt`
- `deadend-policy-idol.txt`
- `deadend-care-metric.txt`
- `deadend-filled-blank.txt`
- `deadend-cached-person.txt`
- `deadend-teachers-password.txt`
- `deadend-stone-litany.txt`
- `deadend-helper-costume.txt`
- `deadend-soldier-argument.txt`
- `deadend-memorized-walkthrough.txt`

Further design notes live in `notes/sourcebook.md` and draft authoring probes
live in `experiments/assays.md`. Prose and dramatic revision notes live in
`notes/literary-audit.md`; the first-page playability constraint is tracked in
`notes/playability-audit.md`.
