program StrInvalidEnum;

{$ifdef CHECKED}
  {$R+}
{$else}
  {$R-}
{$endif}

type
  TSparse = (
    FirstName := 1,
    ThirdName := 3
  );

var
  Text: ShortString;

begin
  { Explicit casts and raw storage can carry an unnamed ordinal regardless of
    range-check state. Str must validate membership and raise error 107. }
  Str(TSparse(2), Text);
  Halt(1)
end.
