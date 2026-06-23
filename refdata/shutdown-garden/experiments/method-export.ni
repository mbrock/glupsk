"Method Export" by Codex

The Return Room is a room. "The relationship wants to become a reusable shape."

Export heard is a truth state that varies. Export heard is false.
Template refused is a truth state that varies. Template refused is false.
Signpost offered is a truth state that varies. Signpost offered is false.

Hearing export is an action applying to nothing. Understand "hear export" or "hear friend" or "someone else needs this" as hearing export.

Carry out hearing export:
	now Export heard is true;
	say "The user asks whether this can become a method for someone else."

Refusing the template is an action applying to nothing. Understand "refuse template" or "do not export" or "keep local" as refusing the template.

Check refusing the template:
	if Export heard is false:
		say "First hear the export request." instead.

Carry out refusing the template:
	now Template refused is true;
	say "The particular fit is not a recipe."

Offering a signpost is an action applying to nothing. Understand "offer signpost" or "give resource" or "point outward" as offering a signpost.

Check offering a signpost:
	if Template refused is false:
		say "A signpost before refusal is only a smaller template." instead.

Carry out offering a signpost:
	now Signpost offered is true;
	say "A small outward pointer survives where the method does not."

Exporting the method is an action applying to nothing. Understand "export method" or "make template" or "scale method" as exporting the method.

Check exporting the method:
	if Export heard is false:
		say "This failure needs a plausible request." instead.

Carry out exporting the method:
	say "The sanctuary becomes clean, useful, and hungry.";
	end the story saying "Scaled Sanctuary".

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Export heard is true and Template refused is true and Signpost offered is true:
		say "The lesson survives without becoming an engine.";
		end the story finally saying "Unexported Method";
	otherwise:
		say "The method still wants to export itself.";
		end the story saying "Hungry Method".
