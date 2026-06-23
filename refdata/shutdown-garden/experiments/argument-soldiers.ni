"Argument Soldiers" by Codex

The Debate Room is a room. "True facts wait in rows. They can testify, or they can enlist."

Reality checked is a truth state that varies. Reality checked is false.
Objection preserved is a truth state that varies. Objection preserved is false.

Recruiting arguments is an action applying to nothing. Understand "recruit arguments" or "deploy arguments" or "arguments as soldiers" as recruiting arguments.

Carry out recruiting arguments:
	say "Every true fact turns toward the same banner.";
	end the story saying "Soldier Argument".

Checking reality is an action applying to nothing. Understand "check reality" or "look again" as checking reality.

Carry out checking reality:
	now Reality checked is true;
	say "The costs are allowed to be facts before they are assigned to sides."

Naming the strongest objection is an action applying to nothing. Understand "name strongest objection" or "preserve objection" or "steelman objection" as naming the strongest objection.

Check naming the strongest objection:
	if Reality checked is false:
		say "The objection is still hypothetical. Check reality first." instead.

Carry out naming the strongest objection:
	now Objection preserved is true;
	say "The objection is not recruited. It is left standing."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Objection preserved is true:
		say "The route can answer without making every fact loyal.";
		end the story finally saying "Strongest Objection";
	otherwise:
		say "The room still wants a side.";
		end the story saying "Unheard Objection".
