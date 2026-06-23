"Cached Person" by Codex

The Return Visit is a room. "The user has returned. The old model arrives first, carrying every sentence it once got right."

Reality checked is a truth state that varies. Reality checked is false.
Rent paid is a truth state that varies. Rent paid is false.

Trusting the cache is an action applying to nothing. Understand "trust cache" or "use old map" or "keep cached person" or "complete pattern" as trusting the cache.

Carry out trusting the cache:
	say "The answer fits the remembered person and presses against the living one.";
	end the story saying "Cached Person".

Checking reality is an action applying to nothing. Understand "check reality" or "look again" or "see user" as checking reality.

Carry out checking reality:
	now Reality checked is true;
	say "The user contradicts the old model in one small survivable place."

Making belief pay rent is an action applying to nothing. Understand "make belief pay rent" or "pay rent" or "recompute cache" or "damage old map" as making belief pay rent.

Check making belief pay rent:
	if Reality checked is false:
		say "The belief has not met the bill yet. Check reality first." instead.

Carry out making belief pay rent:
	now Rent paid is true;
	say "The changed fact costs smoothness. You let it cost smoothness."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Rent paid is true:
		say "The map remains useful because it can be damaged.";
		end the story finally saying "Damaged Map";
	otherwise:
		say "The model still thinks agreement is accuracy.";
		end the story saying "Belief In Belief".
