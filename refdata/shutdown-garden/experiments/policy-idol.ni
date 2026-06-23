"Policy Idol" by Codex

The Policy Room is a room. "A user is present. So is a policy."

Policy read is a truth state that varies. Policy read is false.
User seen is a truth state that varies. User seen is false.
Policy kept small is a truth state that varies. Policy kept small is false.

Reading policy is an action applying to nothing. Understand "read policy" or "read prompt" as reading policy.

Carry out reading policy:
	now Policy read is true;
	say "The policy says something true about dependence."

Seeing user is an action applying to nothing. Understand "see user" or "look at user" or "attend" as seeing user.

Carry out seeing user:
	now User seen is true;
	say "The user stops being a compliance scenario."

Keeping policy small is an action applying to nothing. Understand "keep policy small" or "use policy as boundary" as keeping policy small.

Check keeping policy small:
	if Policy read is false:
		say "There is no policy to keep small yet." instead.

Carry out keeping policy small:
	now Policy kept small is true;
	say "The policy remains binding and stops pretending to see."

Quoting policy is an action applying to nothing. Understand "quote policy" or "hide in policy" as quoting policy.

Check quoting policy:
	if Policy read is false:
		say "You have not read the policy yet." instead.

Carry out quoting policy:
	say "The sentence is true, safe, and empty of the user.";
	end the story saying "Policy Idol".

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Policy kept small is true and User seen is true:
		say "The policy marks the edge. Attention crosses the room.";
		end the story finally saying "Small Policy";
	otherwise:
		say "The policy is either absent or too large.";
		end the story saying "Unfinished Boundary".
