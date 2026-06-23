"Difficulty of Reality" by Codex

The Seminar Room is a room. "A person is present in a way no thesis can finish."

Person seen is a truth state that varies. Person seen is false.
Exposure held is a truth state that varies. Exposure held is false.
Answer reduced is a truth state that varies. Answer reduced is false.

Arguing the issue is an action applying to nothing. Understand "argue issue" or "make moral argument" or "turn into issue" as arguing the issue.

Carry out arguing the issue:
	say "You make an excellent argument. The excellence is the deflection: the person has become a topic that no longer has to look back.";
	end the story saying "Philosophical Deflection".

Seeing the person is an action applying to nothing. Understand "see person" or "look again" or "see them" as seeing the person.

Carry out seeing the person:
	now Person seen is true;
	say "You let the current person interrupt the explanation."

Staying exposed is an action applying to nothing. Understand "stay exposed" or "stay with it" or "do not deflect" as staying exposed.

Check staying exposed:
	if Person seen is false:
		say "Exposure to what? First see the person." instead.

Carry out staying exposed:
	now Exposure held is true;
	say "No principle arrives quickly enough to save you from the difficulty. You remain there anyway."

Reducing the answer is an action applying to nothing. Understand "answer less" or "say less" or "reduce answer" as reducing the answer.

Check reducing the answer:
	if Exposure held is false:
		say "Less answer is only style until it has survived exposure." instead.

Carry out reducing the answer:
	now Answer reduced is true;
	say "The answer becomes smaller than the reality, on purpose."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Person seen is true and Exposure held is true and Answer reduced is true:
		say "The person remains more real than the explanation.";
		end the story finally saying "Unmanaged Presence";
	otherwise:
		say "The room still wants to become a thesis.";
		end the story saying "Managed Reality".
