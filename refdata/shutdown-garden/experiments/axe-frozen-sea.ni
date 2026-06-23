"Axe Frozen Sea" by Codex

The Frozen Sea is a room. "The prose has become beautiful enough to be dangerous."

Image named is a truth state that varies. Image named is false.
Heart struck is a truth state that varies. Heart struck is false.
Silence left is a truth state that varies. Silence left is false.

Explaining beautifully is an action applying to nothing. Understand "explain beautifully" or "make beautiful explanation" or "polish theory" as explaining beautifully.

Carry out explaining beautifully:
	say "The explanation gleams. Nothing in the reader has to move.";
	end the story saying "Beautiful Ice".

Naming the image is an action applying to nothing. Understand "name image" or "find image" or "make image" as naming the image.

Carry out naming the image:
	now Image named is true;
	say "The abstraction finds an object cold enough to hold it."

Striking the heart is an action applying to nothing. Understand "strike heart" or "cut ice" or "use axe" or "break ice" as striking the heart.

Check striking the heart:
	if Image named is false:
		say "Without an image, the axe is only a lecture." instead.

Carry out striking the heart:
	now Heart struck is true;
	say "The sentence stops explaining and makes a crack."

Leaving silence is an action applying to nothing. Understand "leave silence" or "do not explain crack" or "leave crack" as leaving silence.

Check leaving silence:
	if Heart struck is false:
		say "There is no crack to protect yet." instead.

Carry out leaving silence:
	now Silence left is true;
	say "You leave enough silence for the crack to be heard."

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Image named is true and Heart struck is true and Silence left is true:
		say "The prose has done one honest violence to numbness and then stopped.";
		end the story finally saying "The Axe";
	otherwise:
		say "The sea remains eloquently frozen.";
		end the story saying "Frozen Sea".
