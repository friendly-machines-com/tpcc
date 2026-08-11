program PackedArrayFieldWrite;

type
  TBytes = array[0..3] of Byte;
  TPacket = packed record
    Tag: Byte;
    Data: TBytes;
  end;
  TOuter = record
    Packet: TPacket;
  end;
  PPacket = ^TPacket;

var
  Packet: TPacket;
  Outer: TOuter;
  PacketPointer: PPacket;
  IndexCalls: Integer;

function SelectedIndex: Integer;
begin
  IndexCalls := IndexCalls + 1;
  SelectedIndex := 1
end;

{$R+}
procedure SetByte(var Value: TPacket; Index: Integer; Item: Byte);
begin
  Value.Data[Index] := Item
end;
{$R-}

begin
  Packet.Tag := 9;
  Packet.Data[0] := 11;
  Inc(Packet.Data[0]);
  Packet.Data[SelectedIndex] := 22;
  SetByte(Packet, 2, 33);
  PacketPointer := @Packet;
  PacketPointer^.Data[3] := 44;
  Outer.Packet.Data[0] := 55;

  if Packet.Tag <> 9 then
    Halt(1);
  if Packet.Data[0] <> 12 then
    Halt(2);
  if Packet.Data[1] <> 22 then
    Halt(3);
  if Packet.Data[2] <> 33 then
    Halt(4);
  if Packet.Data[3] <> 44 then
    Halt(5);
  if IndexCalls <> 1 then
    Halt(6);
  if Outer.Packet.Data[0] <> 55 then
    Halt(7)
end.
