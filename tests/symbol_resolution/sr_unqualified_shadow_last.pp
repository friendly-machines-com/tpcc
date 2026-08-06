program sr_unqualified_shadow_last;

{ TARGET rule 2e: with `uses A, B` and both exposing an unqualified symbol of
  the same name, the *last-listed* unit wins for unqualified lookup. }

uses sr_leaf_a, sr_leaf_b;

begin
  if Shared <> 20 then
    Halt(1)
end.
