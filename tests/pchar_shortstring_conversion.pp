program PCharShortStringConversion;

{$H-}

type
  TTiny = string[3];

var
  Buffer: array[0..5] of Char;
  PointerValue: PChar;
  Value: TTiny;
  Captured: TTiny;
  Source: ShortString;
  Ordinary: ShortString;
  Selected: Integer;

procedure TakeTiny(const Argument: TTiny);
begin
  Captured := Argument
end;

procedure SelectString(const Argument: ShortString);
begin
  Selected := 1
end;

procedure SelectString(const Argument: AnsiString);
begin
  Selected := 2
end;

begin
  Buffer[0] := 'a';
  Buffer[1] := 'b';
  Buffer[2] := 'c';
  Buffer[3] := 'd';
  Buffer[4] := #0;
  Buffer[5] := 'x';
  PointerValue := @Buffer[0];

{$R-}
  { Assignment and a const value formal share the same narrowing rule. }
  Value := PointerValue;
  if Value <> 'abc' then
    Halt(1);
  TakeTiny(PointerValue);
  if Captured <> 'abc' then
    Halt(2);

{$R+}
  { Explicit construction always requests the unchecked bounded copy. }
  Value := string[3](PointerValue);
  if Value <> 'abc' then
    Halt(3);
  Value := TTiny(PointerValue);
  if Value <> 'abc' then
    Halt(4);

  Ordinary := string(PointerValue);
  if Ordinary <> 'abcd' then
    Halt(5);

  { The keyword form is an expression and therefore admits postfix indexing. }
  if string[3](PointerValue)[2] <> 'b' then
    Halt(6);

  { The result owns its bytes rather than retaining the source pointer. }
  Value := string[3](PointerValue);
  Buffer[0] := 'z';
  if Value <> 'abc' then
    Halt(7);
  Buffer[0] := 'a';

  Source := 'abcdef';
  Value := string[3](Source);
  if Value <> 'abc' then
    Halt(8);

  { AnsiString is a better destination than narrowing ShortString. }
  SelectString(PointerValue);
  if Selected <> 2 then
    Halt(9);

  { A checked implicit copy accepts a source of exactly the declared capacity. }
  Buffer[3] := #0;
  Value := PointerValue;
  if Value <> 'abc' then
    Halt(10);
  Buffer[3] := 'd';

  PointerValue := nil;
  Value := string[3](PointerValue);
  if Value <> '' then
    Halt(11);
  Value := PointerValue;
  if Value <> '' then
    Halt(12)
end.
