program PackedArrayElementWrite;

type
  TBytes = array[0..3] of Byte;
  TPacket = packed record
    Data: TBytes;
  end;

var
  R: TPacket;

begin
  R.Data[0] := 1;
end.
