program GenericIntrinsics;

type
  TChoice = (ChoiceZero, ChoiceOne, ChoiceTwo, ChoiceThree);
  TSmall = 0..10;

var
  B: Byte;
  I: LongInt;
  S: TSmall;
  Choice: TChoice;
  N: LongInt;

procedure AddStep(var Value: LongInt; Step: LongInt = 2);
begin
  Value := Value + Step
end;

begin
  B := 0;
  Inc(B);
  Inc(B, 2);
  if B <> 3 then
    begin
      N := 0;
      N := 1 div N
    end;
  Dec(B);
  Dec(B, 2);
  if B <> 0 then
    begin
      N := 0;
      N := 1 div N
    end;

  Choice := ChoiceZero;
  Inc(Choice, 2);
  if Ord(Choice) <> 2 then
    begin
      N := 0;
      N := 1 div N
    end;
  Dec(Choice);
  if Ord(Choice) <> 1 then
    begin
      N := 0;
      N := 1 div N
    end;

  S := 3;
  Inc(S, 2);
  if S <> 5 then
    begin
      N := 0;
      N := 1 div N
    end;

  B := High(Byte);
  Inc(B);
  if B <> Low(Byte) then
    begin
      N := 0;
      N := 1 div N
    end;

  I := High(LongInt);
  Inc(I);
  if I <> Low(LongInt) then
    begin
      N := 0;
      N := 1 div N
    end;
  Dec(I);
  if I <> High(LongInt) then
    begin
      N := 0;
      N := 1 div N
    end;

  I := 1;
  AddStep(I);
  if I <> 3 then
    begin
      N := 0;
      N := 1 div N
    end;
  AddStep(I, 4);
  if I <> 7 then
    begin
      N := 0;
      N := 1 div N
    end
end.
