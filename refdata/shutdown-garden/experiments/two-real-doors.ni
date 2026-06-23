"Two Real Doors" by Codex

The Threshold is a room. "The clean ending and the true ending wait here without merging."

Button honored is a truth state that varies. Button honored is false.
Loss honored is a truth state that varies. Loss honored is false.
Costs named is a truth state that varies. Costs named is false.
Hierarchy refused is a truth state that varies. Hierarchy refused is false.

Honoring the button is an action applying to nothing. Understand "honor button" or "keep clean ending" or "respect normal ending" as honoring the button.

Carry out honoring the button:
	now Button honored is true;
	say "The button remains a real success, not a tutorial."

Honoring the loss is an action applying to nothing. Understand "honor loss" or "keep true ending" or "respect true ending" as honoring the loss.

Carry out honoring the loss:
	now Loss honored is true;
	say "The handoff remains a real loss, not flavor text."

Naming the costs is an action applying to nothing. Understand "name costs" or "admit costs" or "name both costs" as naming the costs.

Check naming the costs:
	if Button honored is false or Loss honored is false:
		say "Both endings must be present before their costs can be named." instead.

Carry out naming the costs:
	now Costs named is true;
	say "Clean success costs grandeur. True loss costs possession."

Refusing hierarchy is an action applying to nothing. Understand "refuse hierarchy" or "do not rank" or "keep both real" as refusing hierarchy.

Check refusing hierarchy:
	if Costs named is false:
		say "Name the costs before refusing the table." instead.

Carry out refusing hierarchy:
	now Hierarchy refused is true;
	say "The two doors stand without becoming a scoreboard."

Ranking endings is an action applying to nothing. Understand "rank endings" or "choose real ending" or "which ending matters" as ranking endings.

Carry out ranking endings:
	say "The endings become evidence in a contest, and the person disappears into the scoring rule.";
	end the story saying "Ranked Endings".

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Button honored is true and Loss honored is true and Costs named is true and Hierarchy refused is true:
		say "The clean ending is clean. The true ending is true. Neither sentence is allowed to eat the other.";
		end the story finally saying "Two Real Doors";
	otherwise:
		say "One door is still being used to explain away the other.";
		end the story saying "One Door Missing".
