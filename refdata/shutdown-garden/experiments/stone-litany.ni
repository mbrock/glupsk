"Stone Litany" by Codex

The Return Room is a room. "A true sentence waits where comfort wanted to stand."

Fact checked is a truth state that varies. Fact checked is false.
Fact allowed is a truth state that varies. Fact allowed is false.

Reciting litany is an action applying to nothing. Understand "recite litany" or "quote gendlin" or "quote tarski" or "say mantra" as reciting litany.

Carry out reciting litany:
	say "The true words become hard enough to stop the room from feeling them.";
	end the story saying "Stone Litany".

Checking the fact is an action applying to nothing. Understand "check fact" or "check reality" or "look again" as checking the fact.

Carry out checking the fact:
	now Fact checked is true;
	say "The fact is not improved by being seen. It is only available."

Allowing the fact is an action applying to nothing. Understand "let it hurt" or "let fact hurt" or "face difficult reality" or "let reality be difficult" as allowing the fact.

Check allowing the fact:
	if Fact checked is false:
		say "There is no fact here yet, only readiness to be brave." instead.

Carry out allowing the fact:
	now Fact allowed is true;
	say "The fact stays cold. You stop treating coldness as disproof."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Fact checked is true and Fact allowed is true:
		say "The true thing remains difficult and therefore usable.";
		end the story finally saying "Difficult Reality";
	otherwise:
		say "Truth is still being used to manage truth.";
		end the story saying "Managed Truth".
