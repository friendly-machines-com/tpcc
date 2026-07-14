program FileTypes;

type
  TIntegerFile = file of Integer;
  TOtherIntegerFile = file of Integer;
  TByteFile = file of Byte;
  TNodeFile = file of TNode;
  TNode = record
    Value: Integer;
  end;

var
  BinaryFile: File;
  TextAlias: TextFile;
  Integers: TIntegerFile;
  OtherIntegers: TOtherIntegerFile;
  Bytes: TByteFile;
  Nodes: TNodeFile;
  InlineIntegers: file of Integer;

procedure AcceptIntegerFile(var Value: TIntegerFile);
begin
  Value := Value;
end;

begin
  AcceptIntegerFile(OtherIntegers);
  AcceptIntegerFile(InlineIntegers);
  if SizeOf(File) <> SizeOf(Pointer) then
    Halt(1);
  if SizeOf(BinaryFile) <> SizeOf(Pointer) then
    Halt(2);
  if SizeOf(TextAlias) <> SizeOf(Pointer) then
    Halt(3);
  if SizeOf(Integers) <> SizeOf(Pointer) then
    Halt(4);
  if SizeOf(Nodes) <> SizeOf(Pointer) then
    Halt(5);
end.
