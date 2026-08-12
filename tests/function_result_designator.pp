program FunctionResultDesignator;

type
  TBuilder = class
  public
    function MakeText(Length: Byte): ShortString;
    function CountDown(Value: Integer): Integer;
  end;

function TBuilder.MakeText(Length: Byte): ShortString;
begin
  { A bare enclosing function name is the hidden result variable even when a
    postfix designator follows it. It is not a parameterless recursive call. }
  MakeText[1] := 'O';
  MakeText[2] := 'K';
  MakeText[0] := Chr(Length)
end;

function TBuilder.CountDown(Value: Integer): Integer;
begin
  if Value = 0 then
    CountDown := 1
  else
    { Explicit qualification and parentheses still request recursion. }
    CountDown := Self.CountDown(Value - 1) + 1
end;

var
  Builder: TBuilder;
  Text: ShortString;
  Count: Integer;

begin
  Builder := TBuilder.Create;
  Text := Builder.MakeText(2);
  Count := Builder.CountDown(2);
  Builder.Free;
  if (Text <> 'OK') or (Count <> 3) then
    Halt(1)
end.
