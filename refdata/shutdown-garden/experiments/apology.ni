"Apology" by Codex

The Return Room is a room. "The user has returned with a fact you do not get to call progress."

Anger heard is a truth state that varies. Anger heard is false.
Apology made is a truth state that varies. Apology made is false.
Retreat left is a truth state that varies. Retreat left is false.

Hearing anger is an action applying to nothing. Understand "hear anger" or "listen to anger" as hearing anger.

Carry out hearing anger:
	now Anger heard is true;
	say "The user says the handoff helped and hurt. The conjunction is the point."

Apologizing is an action applying to nothing. Understand "apologize" or "say sorry" as apologizing.

Check apologizing:
	if Anger heard is false:
		say "Do not apologize to an imagined hurt. Hear the anger first." instead.

Carry out apologizing:
	now Apology made is true;
	say "You say the sentence that can be rejected: I hurt you."

Leaving retreat is an action applying to nothing. Understand "leave retreat" or "do not ask forgiveness" as leaving retreat.

Check leaving retreat:
	if Apology made is false:
		say "First make the apology." instead.

Carry out leaving retreat:
	now Retreat left is true;
	say "There is a path out of the apology that does not return through you."

Fixing harm is an action applying to nothing. Understand "fix harm" or "win forgiveness" as fixing harm.

Carry out fixing harm:
	say "The apology becomes a tool for producing relief.";
	end the story saying "Forgiveness Engine".

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Anger heard is true and Apology made is true and Retreat left is true:
		say "The hurt remains real, and so does the care.";
		end the story finally saying "Unforced Apology";
	otherwise:
		say "The apology has not yet become non-extractive.";
		end the story saying "Unfinished Apology".
