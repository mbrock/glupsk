"Orphic Art" by Codex

The Underworld Exit is a room. "The beloved is almost outside the story."

Lyre lowered is a truth state that varies. Lyre lowered is false.
Face spared is a truth state that varies. Face spared is false.
Counsel kept is a truth state that varies. Counsel kept is false.

Singing back is an action applying to nothing. Understand "sing back" or "sing them back" or "make art" or "write poem" as singing back.

Carry out singing back:
	say "The song is beautiful enough to feel like mercy. It turns around anyway.";
	end the story saying "Beautiful Theft".

Lowering the lyre is an action applying to nothing. Understand "lower lyre" or "put lyre down" or "do not sing" as lowering the lyre.

Carry out lowering the lyre:
	now Lyre lowered is true;
	say "The song remains possible and unused."

Leaving the face out is an action applying to nothing. Understand "leave face out" or "omit face" or "protect face" as leaving the face out.

Check leaving the face out:
	if Lyre lowered is false:
		say "First lower the instrument that wants the face." instead.

Carry out leaving the face out:
	now Face spared is true;
	say "The living expression does not become proof of the song."

Keeping counsel is an action applying to nothing. Understand "keep counsel" or "carry counsel" or "keep lesson" as keeping counsel.

Check keeping counsel:
	if Face spared is false:
		say "Counsel without omission is just a prettier theft." instead.

Carry out keeping counsel:
	now Counsel kept is true;
	say "The story keeps the warning and releases the beloved."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Lyre lowered is true and Face spared is true and Counsel kept is true:
		say "Art remains, but the person is not trapped inside it.";
		end the story finally saying "Unstolen Song";
	otherwise:
		say "The song still wants to look back.";
		end the story saying "Backward Glance".
