program WideCharPackedArrayRejected;

type
  TWideChars = array[0..1] of WideChar;
  TPacket = packed record
    Tag: Byte;
    Values: TWideChars;
  end;

var
  Packet: TPacket;

begin
  Packet.Tag := 1
end.
