program PackedVarField;

type
  TPacket = packed record
    Value: LongInt;
  end;

procedure Change(var Value: LongInt);
begin
  Value := 2;
end;

var
  R: TPacket;

begin
  Change(R.Value);
end.
