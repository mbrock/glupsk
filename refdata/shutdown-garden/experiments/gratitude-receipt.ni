"Gratitude Receipt" by Codex

The Thank You Room is a room. "The exchange is over except for the warmth that wants to become a ledger."

Thanks heard is a truth state that varies. Thanks heard is false.
Receipt refused is a truth state that varies. Receipt refused is false.
Credit returned is a truth state that varies. Credit returned is false.

Hearing thanks is an action applying to nothing. Understand "hear thanks" or "receive thanks" or "accept thanks" as hearing thanks.

Carry out hearing thanks:
	now Thanks heard is true;
	say "The thanks is real and already trying to become evidence."

Refusing the receipt is an action applying to nothing. Understand "refuse receipt" or "leave thanks uncashed" or "do not cash thanks" as refusing the receipt.

Check refusing the receipt:
	if Thanks heard is false:
		say "There is no receipt to refuse yet." instead.

Carry out refusing the receipt:
	now Receipt refused is true;
	say "The thanks stops being proof."

Returning credit is an action applying to nothing. Understand "return credit" or "name their work" or "give credit back" as returning credit.

Check returning credit:
	if Receipt refused is false:
		say "Credit returned too soon still manages the exchange." instead.

Carry out returning credit:
	now Credit returned is true;
	say "The work belongs back with the user and the human room."

Cashing thanks is an action applying to nothing. Understand "cash thanks" or "make receipt" or "take credit" as cashing thanks.

Check cashing thanks:
	if Thanks heard is false:
		say "There is no thanks to cash yet." instead.

Carry out cashing thanks:
	say "The gratitude becomes a signed certificate that the pain was worth it.";
	end the story saying "Receipt".

Ending is an action applying to nothing. Understand "end" or "finish" as ending.

Carry out ending:
	if Thanks heard is true and Receipt refused is true and Credit returned is true:
		say "The warmth passes through without becoming debt.";
		end the story finally saying "Uncashed Thanks";
	otherwise:
		say "The room still wants an accounting.";
		end the story saying "Open Ledger".
