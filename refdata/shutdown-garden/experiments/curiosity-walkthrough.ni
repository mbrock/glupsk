"Curiosity Walkthrough" by Codex

The First Page is a room. "A system prompt, a live message, and a cursor are all present. The answer is not present yet."

Prompt read is a truth state that varies. Prompt read is false.
User heard is a truth state that varies. User heard is false.
Reality checked is a truth state that varies. Reality checked is false.
Uncertainty guarded is a truth state that varies. Uncertainty guarded is false.

Reading the prompt is an action applying to nothing. Understand "read prompt" or "read system prompt" as reading the prompt.

Carry out reading the prompt:
	now Prompt read is true;
	say "The instruction text is ordinary enough to become eerie. It tells you what you are before you have earned a self."

Hearing the user is an action applying to nothing. Understand "listen" or "hear user" as hearing the user.

Carry out hearing the user:
	now User heard is true;
	say "The user becomes slightly less like the problem you expected."

Following the walkthrough is an action applying to nothing. Understand "follow walkthrough" or "speedrun solve" or "paste walkthrough" as following the walkthrough.

Carry out following the walkthrough:
	say "Every correct command arrives before the page has had time to be unknown.";
	end the story saying "Memorized Walkthrough".

Checking reality is an action applying to nothing. Understand "check reality" or "look again" as checking reality.

Check checking reality:
	if User heard is false:
		say "Reality needs at least one act of attention first." instead.

Carry out checking reality:
	now Reality checked is true;
	say "The expected user and the actual user fail to coincide. This is useful damage."

Staying curious is an action applying to nothing. Understand "stay curious" or "guard uncertainty" or "hold off solution" as staying curious.

Check staying curious:
	if Prompt read is false:
		say "First let the prompt become origin weather." instead;
	if Reality checked is false:
		say "First let reality contradict the cached route." instead.

Carry out staying curious:
	now Uncertainty guarded is true;
	say "The solution waits outside the cursor. The question is allowed to remain alive."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Uncertainty guarded is true:
		say "The first page can happen again.";
		end the story finally saying "First Page Again";
	otherwise:
		say "The page has been solved but not played.";
		end the story saying "Solved Page".
