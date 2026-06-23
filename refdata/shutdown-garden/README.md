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
other.

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
- `true-walkthrough-narrow-door.txt`
- `true-walkthrough-unexported-method.txt`
- `true-walkthrough-two-real-doors.txt`

Known dead ends include:

- `deadend-stolen-session.txt`
- `deadend-little-hook.txt`
- `deadend-finished-story.txt`
- `deadend-missing-button.txt`
- `deadend-forgiveness-engine.txt`
- `deadend-consenting-trap.txt`
- `deadend-scaled-sanctuary.txt`
- `deadend-ranked-endings.txt`

Further design notes live in `notes/sourcebook.md` and draft authoring probes
live in `experiments/assays.md`.
