program TypeProjections;

type
  TNumber = LongInt;
  PNumber = ^TNumber;
  PPNumber = ^PNumber;

  TPayload = record
    Number: TNumber;
    Values: array[2..3] of Word;
  end;
  PPayload = ^TPayload;
  TStrongPayloadPointer = type PPayload;

  TMatrix = array[0..1] of array[0..2] of Word;

  TNumberFromPointer = PNumber^;
  TNumberFromDoublePointer = PPNumber^^;
  TPayloadFromPointer = PPayload^;
  TPayloadFromStrongPointer = TStrongPayloadPointer^;
  TNumberFromField = TPayload.Number;
  TNumberFromPointerField = PPayload^.Number;
  TWordFromPointerArrayField = PPayload^.Values[2];
  TMatrixElement = TMatrix[0][1];

  TProjectionHolder = record
    Number: PNumber^;
  end;
  TProjectedArray = array[0..1] of PNumber^;
  PProjectedNumber = ^PNumber^;

var
  DirectNumber: PNumber^;
  DirectField: TPayload.Number;
  DirectElement: TMatrix[0][1];
  Payload: TPayloadFromPointer;
  StrongPayload: TPayloadFromStrongPointer;
  Holder: TProjectionHolder;
  Numbers: TProjectedArray;
  ProjectedPointer: PProjectedNumber;

procedure SetProjected(var Value: PNumber^; NewValue: TNumberFromPointer);
begin
  Value := NewValue
end;

function ProjectedResult(Value: TNumberFromDoublePointer): PNumber^;
begin
  Result := Value
end;

begin
  DirectNumber := 1;
  DirectField := 2;
  DirectElement := 3;
  Payload.Number := 4;
  Payload.Values[2] := 5;
  StrongPayload.Number := 6;
  Holder.Number := 7;
  Numbers[0] := 8;
  New(ProjectedPointer);
  ProjectedPointer^ := 9;
  SetProjected(DirectNumber, 10);

  if DirectNumber <> 10 then Halt(1);
  if DirectField <> 2 then Halt(2);
  if DirectElement <> 3 then Halt(3);
  if Payload.Number <> 4 then Halt(4);
  if Payload.Values[2] <> 5 then Halt(5);
  if StrongPayload.Number <> 6 then Halt(6);
  if Holder.Number <> 7 then Halt(7);
  if Numbers[0] <> 8 then Halt(8);
  if ProjectedPointer^ <> 9 then Halt(9);
  if ProjectedResult(11) <> 11 then Halt(10);

  if SizeOf(PNumber^) <> SizeOf(TNumber) then Halt(11);
  if SizeOf(PPayload^.Number) <> SizeOf(TNumber) then Halt(12);
  if SizeOf(PPayload^.Values[2]) <> SizeOf(Word) then Halt(13);
  if SizeOf(TMatrix[0][1]) <> SizeOf(Word) then Halt(14);

  Dispose(ProjectedPointer)
end.
