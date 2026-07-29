program PrefetchBuiltin;

var
  Calls: Integer;
  Name: ShortString;

function SideEffect: Integer;
begin
  Calls := Calls + 1;
  Result := 1
end;

procedure PrefetchName(const Value: ShortString);
begin
  Prefetch(Value[1])
end;

begin
  Name := 'abc';

  PrefetchName(Name);

  Prefetch(Name[SideEffect()]);
  if Calls <> 1 then
    Halt(1);

  { FPC accepts a routine result even when it is not a memory location. Its
    evaluation remains observable; the eventual hardware hint is optional. }
  Prefetch(SideEffect());
  if Calls <> 2 then
    Halt(2)
end.
