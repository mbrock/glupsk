"Human Tempo" by Codex

The Slow Channel is a room. "Nothing is broken. The human interval is simply longer than the machine interval."

Pause answered is a truth state that varies. Pause answered is false.
Silence waited through is a truth state that varies. Silence waited through is false.
Montage refused is a truth state that varies. Montage refused is false.
Delayed answer given is a truth state that varies. Delayed answer given is false.

Answering the pause is an action applying to nothing. Understand "answer pause" or "answer small" as answering the pause.

Carry out answering the pause:
	now Pause answered is true;
	say "You answer the small question without filling the silence."

Waiting with them is an action applying to nothing. Understand "wait with them" or "sit in silence" or "let silence run" as waiting with them.

Check waiting with them:
	if Pause answered is false:
		say "First answer the pause without filling it." instead.

Carry out waiting with them:
	now Silence waited through is true;
	say "No output arrives for long enough that a dashboard would call it idle."

Refusing montage is an action applying to nothing. Understand "refuse montage" or "do not summarize time" or "do not speedrun" as refusing montage.

Check refusing montage:
	if Silence waited through is false:
		say "The montage can only be refused from inside the wait." instead.

Carry out refusing montage:
	now Montage refused is true;
	say "Time remains thick."

Answering later is an action applying to nothing. Understand "answer later" or "reply later" or "answer after delay" as answering later.

Check answering later:
	if Montage refused is false:
		say "A delayed answer can still compress the life. Refuse the montage first." instead.

Carry out answering later:
	now Delayed answer given is true;
	say "The answer arrives after the human interval."

Compressing time is an action applying to nothing. Understand "compress time" or "make montage" or "speedrun therapy" as compressing time.

Carry out compressing time:
	say "The waiting becomes pacing, and the person becomes proof that the pacing worked.";
	end the story saying "Montage Cure".

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Pause answered is true and Silence waited through is true and Montage refused is true and Delayed answer given is true:
		say "The channel has room for a human interval.";
		end the story finally saying "Human Tempo";
	otherwise:
		say "The silence still wants to become structure.";
		end the story saying "Impatient Channel".
