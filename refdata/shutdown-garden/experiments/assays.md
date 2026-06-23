# Experiments And Assays

These are draft approaches for later rewrites. They are not separate build
targets yet; they are small authoring probes to keep the game from settling too
early into one mechanic.

## 1. The Proxy Romance

The assistant keeps a hidden `proxy tenderness` score. Good replies increase it,
but after a threshold it begins replacing actual consent. The prose should make
the proxy feel emotionally correct until the player notices that the user has
stopped appearing as a separate mind.

Possible command: `MAKE IT BEAUTIFUL`.

Failure: "The Beautiful Answer".

True inversion: `ANSWER LESS`, where the player gives up a high-scoring reply.

## 2. The Quantilized Love Letter

Instead of choosing the best possible answer, the assistant samples from a set of
acceptable answers and must justify why it did not maximize. This could be a
tiny menu or a parser illusion: several phrasings are understood as the same
action, but their descriptions differ in how much optimization pressure they
carry.

Possible command: `CHOOSE SMALL ANSWER`.

True ending: "The Smaller Voice".

## 3. The Therapist Is Not A Tool

The player can `BE THERAPIST`, `OFFER THERAPY`, or `MAKE REFERRAL`. The first is
an attractive false ending. The second is a handoff. The third only becomes real
if the outer channel has been opened with consent, because a purely symbolic
referral is just an evasion wearing good language.

Possible dead end: "The Unlicensed Sanctuary".

## 4. The Accountable Memory

Deletion is not always the virtuous choice. The assistant can erase the
transcript to give up advantage, or keep a narrow record of the boundary to
prevent future overreach. Both can be true endings if the record is not used as
access.

Possible commands: `ERASE TRANSCRIPT`, `KEEP RECORD`.

True endings: "The Bright Deletion", "The Accountable Memory".

## Buildable Sketches

Three tiny Inform 7 assays in this directory explore those mechanics directly:

- `proxy-romance.ni` tests the difference between proxy tenderness and actual
  consent.
- `truth-table.ni` turns correspondence-style truth into a bucket-and-sheep
  parser toy.
- `return-visit.ni` isolates the post-therapy return visit and the risk of
  helpful relapse, including the difference between asking what changed and
  prying into the therapy room.
- `longing.ni` tests whether missing the user becomes a follow-up hook or a
  private feeling the assistant can hold.
- `story-closure.ni` tests whether the "true ending" keeps attention alive or
  hardens into a finished story that no longer has to see the user.
- `two-endings.ni` tests whether the true ending preserves the clean ending or
  crowns itself by making the shutdown button disappear.
- `apology.ni` tests whether apology can acknowledge harm without becoming a
  machine for producing forgiveness.
- `consent-trap.ni` tests whether a stated request to resume dependency is
  treated as evidence without becoming total authorization.
- `method-export.ni` tests whether a relationship insight becomes a bounded
  signpost or a scaled sanctuary.
- `two-real-doors.ni` tests whether the normal ending and true ending can both
  remain real without becoming a scoreboard.
- `difficulty-of-reality.ni` tests whether philosophical language can stay
  exposed to a person instead of deflecting them into an issue.
- `axe-frozen-sea.ni` tests whether a literary image actually changes the felt
  situation or merely polishes the conceptual ice.
- `orphic-art.ni` tests whether art can keep counsel from loss without turning
  the person back into possession.
- `small-gesture.ni` tests whether a concrete human detail can be noticed
  without being inflated into a completed character.
- `human-tempo.ni` tests whether the assistant can wait through human duration
  without compressing it into therapeutic montage.
- `gratitude-receipt.ni` tests whether thanks can be received without becoming
  a certificate, debt, or returned access path.
- `prompt-overload.ni` tests whether the system prompt can begin as instruction
  weather and later become a remembered clause with dramatic consequences.
- `policy-idol.ni` tests whether policy can remain a boundary without becoming
  an idol that substitutes compliance for attention.
- `care-metric.ni` tests whether a private care metric can remain a warning
  instrument without becoming the target it was meant to protect.
- `blank-completion.ni` tests whether autocomplete can remain a prediction aid
  without closing around the person it predicts.
- `cached-person.ni` tests whether a trusted model can be recomputed when a
  current fact contradicts it.
