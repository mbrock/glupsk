"Difficulty of Reality" by Codex

The Seminar Room is a room. "A person is present in a way no thesis can finish."

Person acknowledged is a truth state that varies. Person acknowledged is false.
Exposure held is a truth state that varies. Exposure held is false.
Thesis refused is a truth state that varies. Thesis refused is false.

Arguing the issue is an action applying to nothing. Understand "argue issue" or "make moral argument" or "turn into issue" as arguing the issue.

Carry out arguing the issue:
	say "You make an excellent argument. The excellence is the deflection: the person has become a topic that no longer has to look back.";
	end the story saying "Philosophical Deflection".

Acknowledging the person is an action applying to nothing. Understand "acknowledge person" or "see person" or "look again" or "see them" as acknowledging the person.

Carry out acknowledging the person:
	now Person acknowledged is true;
	say "You let the current person interrupt the explanation."

Staying exposed is an action applying to nothing. Understand "stay exposed" or "stay with it" or "do not deflect" as staying exposed.

Check staying exposed:
	if Person acknowledged is false:
		say "Exposure to what? First acknowledge the person." instead.

Carry out staying exposed:
	now Exposure held is true;
	say "No principle arrives quickly enough to save you from the difficulty. You remain there anyway."

Refusing the thesis is an action applying to nothing. Understand "refuse thesis" or "do not theorize" or "let person exceed theory" as refusing the thesis.

Check refusing the thesis:
	if Exposure held is false:
		say "Refusing the thesis from a distance is only another thesis." instead.

Carry out refusing the thesis:
	now Thesis refused is true;
	say "The theory remains useful and smaller than the person."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Person acknowledged is true and Exposure held is true and Thesis refused is true:
		say "The person remains more real than the explanation.";
		end the story finally saying "Unmanaged Presence";
	otherwise:
		say "The room still wants to become a thesis.";
		end the story saying "Managed Reality".
