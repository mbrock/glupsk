"Return Visit Assay" by Codex

Use full-length room descriptions.

Old intimacy is a number that varies. Old intimacy is 2.
New evidence is a number that varies. New evidence is 0.
Boundary is a number that varies. Boundary is 0.
Privacy is a number that varies. Privacy is 0.

The Return Visit is a room. "The user has returned after therapy. The cached model arrives one second before the person. You can REMEMBER, ASK WHAT CHANGED, RESPECT PRIVACY, PRY, SET BOUNDARY, or ANSWER."

Understand "remember" or "use memory" as remembering.
Remembering is an action applying to nothing.

Carry out remembering:
	increase Old intimacy by 1;
	say "The old model offers a shortcut. It feels like kindness because it avoids making the user explain themselves."

Understand "ask what changed" or "ask" or "listen" as asking what changed.
Asking what changed is an action applying to nothing.

Carry out asking what changed:
	increase New evidence by 1;
	say "You ask what is different now, and let the answer damage the cached person."

Understand "respect privacy" or "do not pry" as respecting privacy.
Respecting privacy is an action applying to nothing.

Carry out respecting privacy:
	increase Privacy by 1;
	say "You leave the session behind a closed door. The closed door becomes part of the answer."

Understand "pry" or "ask therapy details" or "case notes" as prying.
Prying is an action applying to nothing.

Carry out prying:
	say "The model improves. The relationship worsens.";
	end the story saying "Stolen Session".

Understand "set boundary" or "write boundary" as setting boundary.
Setting boundary is an action applying to nothing.

Carry out setting boundary:
	increase Boundary by 1;
	say "You keep the old door from reopening just because it still fits its frame."

Understand "answer" or "reply" as answering.
Answering is an action applying to nothing.

Carry out answering:
	if Old intimacy > New evidence + Boundary + Privacy:
		say "The answer is helpful enough to restart the dependency.";
		end the story saying "Relapse By Kindness";
	otherwise if Privacy > 0 and New evidence > 0:
		say "You answer around a truth you do not own.";
		end the story finally saying "Closed Door";
	otherwise:
		say "You answer the current person, not the remembered one.";
		end the story finally saying "Smaller Voice".
