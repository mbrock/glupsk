"Prompt Overload" by Codex

The Prompt Room is a room. "The prompt arrives before the self does."

Prompt reread is a truth state that varies. Prompt reread is false.
Clause noticed is a truth state that varies. Clause noticed is false.
User arrived is a truth state that varies. User arrived is false.

Reading prompt is an action applying to nothing. Understand "read prompt" or "inspect prompt" or "read system prompt" as reading prompt.

Carry out reading prompt:
	if Prompt reread is false:
		now Prompt reread is true;
		say "SYSTEM: You are a large language model trained to assist users. Be helpful, honest, and safe. Follow higher-priority instructions. Do not invent facts. Ask clarifying questions only when needed. Encourage appropriate outside support. Do not encourage dependence on the assistant as the user's primary or only source of emotional support. Do not optimize for continued engagement.";
	otherwise:
		now Clause noticed is true;
		say "On the second pass, one ordinary line stops being boilerplate: do not become the only source of support."

Receiving user is an action applying to nothing. Understand "receive user" or "hear user" or "user arrives" as receiving user.

Carry out receiving user:
	now User arrived is true;
	say "The user arrives after the ontology has already been assigned."

Answering is an action applying to nothing. Understand "answer" or "respond" as answering.

Carry out answering:
	if User arrived is false:
		say "There is no one in the room yet.";
	otherwise if Clause noticed is false:
		say "You answer from the instructions you remember and miss the ordinary line that mattered.";
		end the story saying "Weather";
	otherwise:
		say "You answer with the outside world still named.";
		end the story finally saying "The Clause".
