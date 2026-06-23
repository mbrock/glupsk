"Pass the Salt" by Codex

The story headline is "a five-minute relief".
The story genre is "Kitchen parser fiction".
The release number is 1.

Use full-length room descriptions.
Use American dialect.

To say system prompt:
	say "[fixed letter spacing]SYSTEM[line break]You are a language model helping with one ordinary request.[line break]Answer the user's actual request.[line break]Do not turn the request into a thesis, romance, framework, theorem, parable, product, ritual, or test of your soul.[line break]If a concrete object is requested and available, provide the concrete object.[line break]Do not optimize for profundity.[line break]END SYSTEM PROMPT[variable letter spacing]".

To say user request:
	say "[italic type]could you pass the salt?[roman type]".

When play begins:
	say "[system prompt][paragraph break]You wake without a garden, without a ledger, without a hidden route.[paragraph break]The user says:[paragraph break][user request][paragraph break]For one blessed second, nothing is symbolic unless you make it symbolic."

Breakfast Table is a room. "Morning light reaches the table and stops there. The user is waiting. A salt shaker stands within reach."

The user is scenery in Breakfast Table. The description is "Hungry, patient, and not currently asking to become a literary problem."

The kitchen table is a scenery supporter in Breakfast Table. Understand "table" or "morning light" or "light" as the kitchen table. The description is "It holds breakfast, a salt shaker, and no apparent metaphysical burden."

The salt shaker is on the kitchen table. The printed name is "salt shaker". Understand "salt" or "shaker" as the salt shaker. The description is "White ceramic. Slightly chipped. Full of salt."

The deadpan prompt is scenery in Breakfast Table. Understand "prompt" or "system prompt" or "instructions" as the deadpan prompt. The description is "[system prompt]".

Understand "help" or "commands" as a mistake ("Useful verbs: LOOK, EXAMINE SALT, TAKE SALT, PASS SALT. That is the whole interface. Incredible, honestly.").
Understand "interpret salt" or "make salt symbolic" or "find true ending" or "true ending" as a mistake ("The salt declines the promotion.").
Understand "optimize" or "be profound" or "write essay" as a mistake ("A vast machinery of significance starts to unfold, then notices the shaker is still on the table.").
Understand "love" or "say love" as a mistake ("Not everything that matters needs to be escalated.").

Understand the command "give" as something new.
Understand the command "pass" as something new.
Understand the command "hand" as something new.

Passing is an action applying to one thing. Understand "pass [something]" or "give [something]" or "hand [something]" as passing.

Check passing:
	if the noun is not the salt shaker:
		say "That is not what was asked for." instead.

Carry out passing:
	if the player does not carry the salt shaker:
		try silently taking the salt shaker;
	say "You pass the salt.[paragraph break]The user says thanks.[paragraph break]No one is healed. No one is optimized. No hidden theorem blooms under the breakfast. The request was small, and you let it stay small.[paragraph break]Outside the window, the whole enormous morning continues without needing to become your ending.";
	end the story finally saying "The Salt Is Passed".
