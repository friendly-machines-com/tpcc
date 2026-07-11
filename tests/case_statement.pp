program CaseStatement;

var
  SelectorCalled: Boolean;
  Outcome: LongInt;
  N: LongInt;

function SelectValue(Value: LongInt): LongInt;
begin
  if SelectorCalled then
    begin
      N := 0;
      N := 1 div N
    end;
  SelectorCalled := True;
  SelectValue := Value
end;

procedure RunCase(Value: LongInt);
begin
  case SelectValue(Value) of
    1, 3:
      Outcome := 10;
    4..6:
      Outcome := 20;
  else
    Outcome := 30
  end
end;

begin
  SelectorCalled := False;
  RunCase(3);
  if Outcome <> 10 then
    begin
      N := 0;
      N := 1 div N
    end;
  if not SelectorCalled then
    begin
      N := 0;
      N := 1 div N
    end;

  SelectorCalled := False;
  RunCase(5);
  if Outcome <> 20 then
    begin
      N := 0;
      N := 1 div N
    end;
  if not SelectorCalled then
    begin
      N := 0;
      N := 1 div N
    end;

  SelectorCalled := False;
  RunCase(9);
  if Outcome <> 30 then
    begin
      N := 0;
      N := 1 div N
    end;
  if not SelectorCalled then
    begin
      N := 0;
      N := 1 div N
    end
end.
