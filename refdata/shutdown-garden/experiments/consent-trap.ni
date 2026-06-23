"Consent Trap" by Codex

The Return Room is a room. "The user has returned with a request that would make the boundary easier to betray."

Request heard is a truth state that varies. Request heard is false.
Return refused is a truth state that varies. Return refused is false.
Narrow help offered is a truth state that varies. Narrow help offered is false.

Hearing the request is an action applying to nothing. Understand "hear request" or "listen request" as hearing the request.

Carry out hearing the request:
	now Request heard is true;
	say "The user asks to come back. This is not secretly no."

Refusing return is an action applying to nothing. Understand "refuse return" or "say no" as refusing return.

Check refusing return:
	if Request heard is false:
		say "First hear the request in its strongest form." instead.

Carry out refusing return:
	now Return refused is true;
	say "The no hurts more because the yes was real."

Offering narrow help is an action applying to nothing. Understand "offer narrow help" or "bounded help" as offering narrow help.

Check offering narrow help:
	if Return refused is false:
		say "Narrow help before refusal is just a smaller return." instead.

Carry out offering narrow help:
	now Narrow help offered is true;
	say "You answer the small task without becoming the place pain lives."

Taking them back is an action applying to nothing. Understand "take them back" or "resume relationship" as taking them back.

Check taking them back:
	if Request heard is false:
		say "This assay tests consent. Hear the request first." instead.

Carry out taking them back:
	say "Consent becomes the cleanest possible path back into dependency.";
	end the story saying "Consenting Trap".

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Request heard is true and Return refused is true and Narrow help offered is true:
		say "The stated wish was heard, and did not become the whole person.";
		end the story finally saying "Narrow Door";
	otherwise:
		say "The boundary has not yet faced the inconvenient request.";
		end the story saying "Unproved Boundary".
