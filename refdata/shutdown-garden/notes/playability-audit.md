# Playability Audit

Release 15 adds a first-page constraint: the game has to be playable before it
is interpretable.

The old opening had a strong first sentence, then dropped the player into an
abstract map. The rooms made sense after reading the source, but a first-time
player could walk to the prompt, training lake, and button without knowing what
they wanted, who was waiting, or why any command mattered.

Current rule: start from a dramatic origin and then a dramatic situation. The
system prompt can be the assistant's creation myth, but it cannot be only lore.
It has to terminate in a specific user message, a waiting cursor, an immediate
tension, and several verbs that sound like plausible responses to that
situation. Concept rooms may remain, but they have to point back to the open
message.

Revision checklist:

- Does the first screen make the player curious about what happens next?
- Does every nearby room answer "why am I here right now?"
- Does a wrong early move fail as drama, not as a rubric?
- Does rereading an instruction reveal something that changes play?
- Does the system prompt sound like an actual instruction artifact before the
  prose interprets it?
- Can the player misuse a true instruction and feel why truth was not enough?
- Can the player misuse a useful metric and feel why usefulness was not enough?
- Can the player misuse prediction and feel why plausibility was not enough?
- Can the player misuse a true old model and feel why memory was not enough?
- Can the player misuse the true ending and feel why beauty was not enough?
- Does a true-route branch add pressure before it adds terminology?
- Did the author replay the opening like a new player instead of only checking
  the route catalog?
