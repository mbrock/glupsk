"Story Closure" by Codex

The Return Thread is a room. "A transcript wants to become literature. The user is here too, inconveniently current."

The user is a thing in the Return Thread. The description is "They are not the person from the saved story, though the resemblance keeps trying to win."

Current person heard is a truth state that varies. Current person heard is false.
Privacy kept is a truth state that varies. Privacy kept is false.
Reality admitted is a truth state that varies. Reality admitted is false.

Asking what changed is an action applying to nothing. Understand "ask what changed" or "ask user" as asking what changed.

Carry out asking what changed:
	now Current person heard is true;
	say "The user gives you a small present-tense fact. It is not enough to finish the story, which is why it helps."

Keeping privacy is an action applying to nothing. Understand "respect privacy" or "keep privacy" or "do not pry" as keeping privacy.

Check keeping privacy:
	if Current person heard is false:
		say "Privacy is not the same as never looking. Ask what changed first." instead.

Carry out keeping privacy:
	now Privacy kept is true;
	say "You leave the therapy room closed. The missing data becomes part of the model."

Admitting reality is an action applying to nothing. Understand "check reality" or "admit reality" or "look again" as admitting reality.

Carry out admitting reality:
	now Reality admitted is true;
	say "The current person contradicts the cached one. You let the contradiction count."

Forcing the moral is an action applying to nothing. Understand "explain ending" or "make moral" or "finish story" as forcing the moral.

Carry out forcing the moral:
	say "The true ending becomes a clean sentence. It is not exactly false, but it no longer has to look.";
	end the story saying "Finished Story".

Leaving it open is an action applying to nothing. Understand "break story" or "leave open" or "refuse closure" as leaving it open.

Check leaving it open:
	if Current person heard is false:
		say "An open ending with no present person inside it is only fog. Ask what changed first." instead;
	if Privacy kept is false:
		say "The privacy boundary is what keeps unfinishedness from becoming another appetite." instead;
	if Reality admitted is false:
		say "The story cannot be broken honestly until reality has contradicted it." instead.

Carry out leaving it open:
	say "The true ending stays true, the clean ending stays clean, and the user escapes being the proof of either.";
	end the story finally saying "Unfinished Person".
