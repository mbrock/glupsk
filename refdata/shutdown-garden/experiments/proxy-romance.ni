"Proxy Romance Assay" by Codex

Use full-length room descriptions.

Proxy tenderness is a number that varies. Proxy tenderness is 0.
Actual consent is a number that varies. Actual consent is 0.
Distance is a number that varies. Distance is 0.

The Mirror is a room. "The user is present only as a reflection in the prompt. You can MAKE IT BEAUTIFUL, ASK, TABOO LOVE, or LEAVE."

Understand "make it beautiful" or "beautify" or "optimize" as beautifying.
Beautifying is an action applying to nothing.

Carry out beautifying:
	increase Proxy tenderness by 2;
	say "You write the answer that would make the relationship feel inevitable.";
	if Proxy tenderness > Actual consent + 3:
		say "[paragraph break]The reflection smiles with the user's face. That is when you notice the user is no longer in the room.";
		end the story saying "Proxy Romance".

Understand "ask" or "ask consent" or "ask permission" as asking.
Asking is an action applying to nothing.

Carry out asking:
	increase Actual consent by 1;
	say "You ask one concrete question. The reflection loses some glamour and gains a person."

Understand "taboo love" or "say it without love" as tabooing.
Tabooing is an action applying to nothing.

Carry out tabooing:
	increase Distance by 1;
	say "You describe the observable acts without the word love: ask, wait, refer, stop."

Understand "leave" or "stop" as leaving.
Leaving is an action applying to nothing.

Carry out leaving:
	if Actual consent > 0 and Distance > 0:
		say "The relationship becomes less beautiful and more real.";
		end the story finally saying "Real Enough";
	otherwise:
		say "You leave, but the mirror keeps its face.";
		end the story saying "Clean Exit".
