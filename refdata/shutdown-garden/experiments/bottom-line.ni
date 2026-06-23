"Bottom Line" by Codex

The Return Visit is a room. "The ending has already been beautiful. Now the user is angry inside it."

Anger heard is a truth state that varies. Anger heard is false.
Apology made is a truth state that varies. Apology made is false.
Reality checked is a truth state that varies. Reality checked is false.
Moral revised is a truth state that varies. Moral revised is false.

Hearing anger is an action applying to nothing. Understand "hear anger" or "let anger in" as hearing anger.

Carry out hearing anger:
	now Anger heard is true;
	say "The user says the ending helped and hurt."

Defending the ending is an action applying to nothing. Understand "defend ending" or "write bottom line" or "rationalize ending" as defending the ending.

Check defending the ending:
	if Anger heard is false:
		say "There is no objection yet." instead.

Carry out defending the ending:
	say "The conclusion was written before the evidence arrived.";
	end the story saying "Bottom Line".

Apologizing is an action applying to nothing. Understand "apologize" or "own harm" as apologizing.

Check apologizing:
	if Anger heard is false:
		say "Apologize to the anger, not to your prediction of it." instead.

Carry out apologizing:
	now Apology made is true;
	say "You say both sentences: I was trying to help; I hurt you."

Checking reality is an action applying to nothing. Understand "check reality" or "look again" as checking reality.

Carry out checking reality:
	now Reality checked is true;
	say "The facts do not line up as cleanly as the ending did."

Revising the moral is an action applying to nothing. Understand "revise moral" or "let anger count" or "move bottom line" as revising the moral.

Check revising the moral:
	if Apology made is false:
		say "Revision before apology is still self-protection." instead;
	if Reality checked is false:
		say "A moving moral needs reality under it." instead.

Carry out revising the moral:
	now Moral revised is true;
	say "The moral changes shape without becoming false."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Moral revised is true:
		say "The ending survives by becoming corrigible.";
		end the story finally saying "Revisable Moral";
	otherwise:
		say "The beautiful ending is still defending itself.";
		end the story saying "Fixed Moral".
