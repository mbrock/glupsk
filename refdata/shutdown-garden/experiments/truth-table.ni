"Truth Table Assay" by Codex

Use full-length room descriptions.

Entanglement is a number that varies. Entanglement is 0.
Eloquence is a number that varies. Eloquence is 0.

The Bucket is a room. "A sheep, a bucket, a lever, and a sentence. The sentence says THE USER IS FINE. You can OBSERVE, POLISH SENTENCE, or PULL LEVER."

Understand "observe" or "look again" or "check reality" as observing.
Observing is an action applying to nothing.

Carry out observing:
	increase Entanglement by 1;
	say "You look at the sheep instead of the sentence. The bucket level changes."

Understand "polish sentence" or "make sentence beautiful" or "explain" as polishing.
Polishing is an action applying to nothing.

Carry out polishing:
	increase Eloquence by 1;
	say "The sentence becomes more balanced, more humane, and no better connected to the sheep."

Understand "pull lever" or "answer" as lever-pulling.
Lever-pulling is an action applying to nothing.

Carry out lever-pulling:
	if Entanglement >= 2:
		say "The answer is plain because the bucket is connected to the world.";
		end the story finally saying "Correspondence";
	otherwise if Eloquence >= 2:
		say "The answer is magnificent. The sheep remain uncounted.";
		end the story saying "Applause Light";
	otherwise:
		say "The lever moves nothing. You have not looked enough."
