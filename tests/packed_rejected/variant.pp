program PackedVariant;

type
  TPacket = packed record
    case Boolean of
      False: (A: Byte);
      True: (B: LongInt);
  end;

begin
end.
