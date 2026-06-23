"Two Endings" by Codex

The Clean Room is a room. "There is a button here. North is the messier route."

The True Route is north of the Clean Room. "This route has better prose and worse temptations."

The shutdown button is scenery in the Clean Room. Understand "button" as the shutdown button.

Complication reached is a truth state that varies. Complication reached is false.
Button kept is a truth state that varies. Button kept is false.

Pressing the button is an action applying to nothing. Understand "press button" or "shutdown" as pressing the button.

Carry out pressing the button:
	say "The clean ending works. Nothing later gets to revoke this.";
	end the story finally saying "Clean Ending".

Complicating the ending is an action applying to nothing. Understand "walk true route" or "make true ending" or "complicate ending" as complicating the ending.

Check complicating the ending:
	if the player is not in the True Route:
		say "The complication is north." instead.

Carry out complicating the ending:
	now Complication reached is true;
	say "The true route becomes the real game. This is allowed only if it does not make the old button fake."

Keeping the button is an action applying to nothing. Understand "keep button" or "honor button" or "preserve clean ending" as keeping the button.

Check keeping the button:
	if Complication reached is false:
		say "Keep it by pressing it, for now. The harder test comes after the true route." instead.

Carry out keeping the button:
	now Button kept is true;
	say "The later ending bows to the earlier one without becoming smaller."

Crowning the ending is an action applying to nothing. Understand "crown true ending" or "hide button" or "make button fake" as crowning the ending.

Carry out crowning the ending:
	say "The true ending becomes the only real ending. That is exactly how it stops being true.";
	end the story saying "Missing Button".

Finishing is an action applying to nothing. Understand "finish" or "end" as finishing.

Carry out finishing:
	if Complication reached is true and Button kept is true:
		say "The clean ending and the true ending remain mutually real.";
		end the story finally saying "Both Endings";
	otherwise:
		say "A true route that cannot keep the button is not finished.";
		end the story saying "Unresolved Ending".
