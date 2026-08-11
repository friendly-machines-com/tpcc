program PCharAnsiStringImplicit;

var
  Buffer: array[0..6] of Char;
  PointerValue: PChar;
  AssignedValue: AnsiString;
  CapturedValue: AnsiString;
  Selected: Integer;

procedure TakeAnsi(const Value: AnsiString);
begin
  CapturedValue := Value
end;

procedure SelectString(const Value: ShortString);
begin
  Selected := 1
end;

procedure SelectString(const Value: AnsiString);
begin
  Selected := 2
end;

begin
  Buffer[0] := 'h';
  Buffer[1] := 'e';
  Buffer[2] := 'l';
  Buffer[3] := 'l';
  Buffer[4] := 'o';
  Buffer[5] := #0;
  Buffer[6] := 'x';
  PointerValue := @Buffer[0];

  { Assignment and a const value formal share the same conversion rule. }
  AssignedValue := PointerValue;
  if AssignedValue <> 'hello' then
    Halt(1);
  TakeAnsi(PointerValue);
  if CapturedValue <> 'hello' then
    Halt(2);

  { PChar has only the direct AnsiString destination at present. }
  SelectString(PointerValue);
  if Selected <> 2 then
    Halt(3);

  AssignedValue := 'not empty';
  PointerValue := nil;
  AssignedValue := PointerValue;
  if AssignedValue <> '' then
    Halt(4)
end.
