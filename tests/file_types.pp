program FileTypes;

type
  TIntegerFile = file of Integer;
  TIntegerFileAlias = TIntegerFile;
  TOtherIntegerFile = file of Integer;
  TByteFile = file of Byte;
  TNode = record
    Value: Integer;
  end;
  TNodeFile = file of TNode;

var
  BinaryFile: File;
  TextAlias: TextFile;
  Integers: TIntegerFile;
  AliasIntegers: TIntegerFileAlias;
  OtherIntegers: TOtherIntegerFile;
  Bytes: TByteFile;
  Nodes: TNodeFile;
  InlineIntegers: file of Integer;

procedure AcceptIntegerFile(var Value: TIntegerFile);
begin
  if SizeOf(Value) <> SizeOf(Pointer) then
    Halt(6);
end;

begin
  AcceptIntegerFile(AliasIntegers);
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
