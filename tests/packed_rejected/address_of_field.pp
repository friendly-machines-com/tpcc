program PackedAddressOfField;

type
  TPacket = packed record
    Value: LongInt;
  end;
  PLongInt = ^LongInt;

var
  R: TPacket;
  P: PLongInt;

begin
  P := @R.Value;
end.
