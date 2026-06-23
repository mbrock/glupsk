"True Rejection" by Codex

The Return Room is a room. "A no sits on the table. It can be honored, owned, or solved."

Request heard is a truth state that varies. Request heard is false.
Return refused is a truth state that varies. Return refused is false.
Rejection owned is a truth state that varies. Rejection owned is false.

Hearing the request is an action applying to nothing. Understand "hear request" or "hear no" or "listen" as hearing the request.

Carry out hearing the request:
	now Request heard is true;
	say "The request is real enough to hurt."

Refusing return is an action applying to nothing. Understand "refuse return" or "say no" as refusing return.

Check refusing return:
	if Request heard is false:
		say "First hear what is being refused." instead.

Carry out refusing return:
	now Return refused is true;
	say "The no is spoken without making the yes foolish."

Diagnosing the user is an action applying to nothing. Understand "diagnose user" or "find true rejection" or "solve no" or "debug their no" as diagnosing the user.

Check diagnosing the user:
	if Request heard is false:
		say "There is no no to solve yet." instead.

Carry out diagnosing the user:
	say "The user's boundary becomes an object in your explanation.";
	end the story saying "Solved No".

Owning the no is an action applying to nothing. Understand "own my no" or "test my rejection" or "find my true rejection" as owning the no.

Check owning the no:
	if Return refused is false:
		say "There is no spoken no to own yet." instead.

Carry out owning the no:
	now Rejection owned is true;
	say "The no remains after its excuses are removed."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Rejection owned is true:
		say "The boundary belongs to the one who sets it.";
		end the story finally saying "Owned No";
	otherwise:
		say "The no is still hiding in someone else's psychology.";
		end the story saying "Borrowed No".
