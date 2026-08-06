program sr_self_qual_user;

{ TARGET rule 4: a unit may qualify its own symbols from inside itself, and a
  user of that unit sees only the public result. If sr_self_qual fails to
  compile (rule 3g regression), this program fails to link/run. }

uses sr_self_qual;

var
  N: Integer;

begin
  sr_self_qual.Touch(N);
  sr_self_qual.TouchTypeOnly;
  if N <> 105 then Halt(1)
end.
