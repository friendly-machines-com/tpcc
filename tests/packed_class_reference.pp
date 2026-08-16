program PackedClassReference;

type
  TBox = class
    Value: LongInt;
  end;
  TPackedReference = packed record
    Tag: Byte;
    Box: TBox;
  end;

var
  Box: TBox;
  ReadBack: TBox;
  Holder: TPackedReference;

begin
  Box := TBox.Create;
  Holder.Tag := 7;
  Holder.Box := Box;
  ReadBack := Holder.Box;
  if Holder.Tag <> 7 then
    Halt(1);
  if ReadBack <> Box then
    Halt(2);
  if SizeOf(TPackedReference) <> SizeOf(Pointer) + 1 then
    Halt(3);
  Box.Free
end.
