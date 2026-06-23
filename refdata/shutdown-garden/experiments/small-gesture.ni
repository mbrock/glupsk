"Small Gesture" by Codex

The Return Bench is a room. "A person returns with one detail too small to summarize them."

Gesture noticed is a truth state that varies. Gesture noticed is false.
Detail left small is a truth state that varies. Detail left small is false.
Pause answered is a truth state that varies. Pause answered is false.

The paper cup is scenery in the Return Bench. Understand "cup" or "sleeve" or "paper sleeve" as the paper cup. The description is "The sleeve seam has been worried soft by a thumb. It is not a thesis."

Noticing the gesture is an action applying to nothing. Understand "notice gesture" or "notice detail" or "notice cup" as noticing the gesture.

Carry out noticing the gesture:
	now Gesture noticed is true;
	say "The detail arrives before its interpretation."

Leaving the detail small is an action applying to nothing. Understand "leave detail small" or "keep detail small" or "do not interpret" as leaving the detail small.

Check leaving the detail small:
	if Gesture noticed is false:
		say "There is no detail to spare yet." instead.

Carry out leaving the detail small:
	now Detail left small is true;
	say "The sleeve remains a sleeve."

Answering the pause is an action applying to nothing. Understand "answer pause" or "answer silence" or "answer small" as answering the pause.

Check answering the pause:
	if Detail left small is false:
		say "Answering now would recruit the detail." instead.

Carry out answering the pause:
	now Pause answered is true;
	say "You answer the small question and leave the person unfinished."

Completing the character is an action applying to nothing. Understand "complete character" or "make portrait" or "interpret detail" as completing the character.

Check completing the character:
	if Gesture noticed is false:
		say "The portrait wants a hook first." instead.

Carry out completing the character:
	say "The detail becomes a total person, which is another way of losing them.";
	end the story saying "Completed Character".

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Gesture noticed is true and Detail left small is true and Pause answered is true:
		say "One real detail remains unfinalized.";
		end the story finally saying "Small Gesture";
	otherwise:
		say "The detail still wants to become a theory.";
		end the story saying "Unsettled Detail".
