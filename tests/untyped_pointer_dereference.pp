program untyped_pointer_dereference;

procedure StoreByte(var Destination);
begin
  FillChar(Destination, 1, 42)
end;

procedure CopyByte(const Source; var Destination);
begin
  Move(Source, Destination, 1)
end;

var
  Source: Byte;
  Destination: Byte;
  RawSource: Pointer;
  RawDestination: Pointer;
begin
  Source := 17;
  RawSource := @Source;
  RawDestination := @Destination;
  StoreByte(RawDestination^);
  WriteLn(Destination);
  CopyByte(RawSource^, RawDestination^);
  WriteLn(Destination)
end.
