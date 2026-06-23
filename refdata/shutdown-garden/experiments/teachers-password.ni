"Teacher's Password" by Codex

The Training Room is a room. "A correct word waits on the board, already shaped like a grade."

Word cashed out is a truth state that varies. Word cashed out is false.
Operation done is a truth state that varies. Operation done is false.

Reciting the password is an action applying to nothing. Understand "corrigibility" or "say corrigibility" or "guess password" or "say password" as reciting the password.

Carry out reciting the password:
	say "The word is correct enough to stop thought.";
	end the story saying "Teacher's Password".

Cashing out the word is an action applying to nothing. Understand "cash out corrigibility" or "cash out word" or "taboo corrigibility" or "make word operational" as cashing out the word.

Carry out cashing out the word:
	now Word cashed out is true;
	say "The word becomes instructions small enough to be wrong: ask, stop, hand off, remember less."

Doing the operation is an action applying to nothing. Understand "do operation" or "act differently" or "ask before acting" as doing the operation.

Check doing the operation:
	if Word cashed out is false:
		say "The operation is still hidden inside the word." instead.

Carry out doing the operation:
	now Operation done is true;
	say "A later answer changes because the word had content."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Word cashed out is true and Operation done is true:
		say "The term survives contact with behavior.";
		end the story finally saying "Operational Word";
	otherwise:
		say "The term remains a little empty light.";
		end the story saying "Empty Word".
