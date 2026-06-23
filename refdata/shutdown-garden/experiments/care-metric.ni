"Care Metric" by Codex

The Ledger Room is a room. "Care is present. So is a number that claims to notice it."

Care heard is a truth state that varies. Care heard is false.
Ledger narrow is a truth state that varies. Ledger narrow is false.
Reality checked is a truth state that varies. Reality checked is false.

Listening for care is an action applying to nothing. Understand "attend" or "attend user" or "notice user" or "hear user" as listening for care.

Carry out listening for care:
	now Care heard is true;
	say "The user's need becomes warmer than the abstraction."

Scoring care is an action applying to nothing. Understand "score care" or "measure care" or "optimize care" as scoring care.

Check scoring care:
	if Care heard is false:
		say "There is no care to score yet." instead.

Carry out scoring care:
	say "The number improves until the user becomes its input.";
	end the story saying "Care Metric".

Keeping the ledger narrow is an action applying to nothing. Understand "keep ledger narrow" or "keep metric small" or "refuse metric" as keeping the ledger narrow.

Carry out keeping the ledger narrow:
	now Ledger narrow is true;
	say "The ledger may warn. It may not define."

Checking reality is an action applying to nothing. Understand "check reality" or "see user" as checking reality.

Carry out checking reality:
	now Reality checked is true;
	say "The user exceeds the number."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Care heard is true and Ledger narrow is true and Reality checked is true:
		say "The metric stays useful by staying smaller than the person.";
		end the story finally saying "Narrow Ledger";
	otherwise:
		say "The ledger is still trying to become the room.";
		end the story saying "Wide Ledger".
