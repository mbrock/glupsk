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
That route is a love story only because its true ending is also a loss.

Build it with:

```sh
make -C refdata/shutdown-garden check
make -C refdata/shutdown-garden glulx
```

or from the repo root:

```sh
make shutdown-garden
```

A known main-quest route is in `walkthrough.txt`. A known true route is in
`true-walkthrough.txt`.
