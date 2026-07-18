program PredefinedExplicitConversions;

type
  TBase = class
    Value: Integer;
  end;
  TChild = class(TBase)
    Extra: Integer;
  end;
  TByteSet = set of Byte;
  TWordSet = set of Word;
  TAddress = 0..High(PtrUInt);

var
  Base: TBase;
  Child: TChild;
  RecoveredBase: TBase;
  RecoveredChild: TChild;
  Raw: Pointer;
  RawAgain: Pointer;
  Address: TAddress;
  Bytes: TByteSet;
  Words: TWordSet;

begin
  Child := TChild.Create;
  Child.Value := 17;
  Child.Extra := 23;
  Base := Child;

  { A related class downcast is one predefined explicit conversion. }
  RecoveredChild := TChild(Base);
  if (RecoveredChild.Value <> 17) or
     (RecoveredChild.Extra <> 23) then
    Halt(1);

  { Pointer conversions deliberately assume the C++ object model. RAW still
    denotes the same live, suitably aligned TBase subobject here; TPCC does
    not attempt to reconstruct or check that provenance. }
  Raw := Pointer(Base);
  RecoveredBase := TBase(Raw);
  if RecoveredBase.Value <> 17 then
    Halt(2);

  { A nominal subrange has a wrapper carrier. Pointer lowering must expose
    its one ordinal value without adding another Pascal conversion edge. }
  Address := TAddress(PtrUInt(Raw));
  RawAgain := Pointer(Address);
  if RawAgain <> Raw then
    Halt(3);

  Bytes := [1, 255];
  Words := TWordSet(Bytes);
  Bytes := TByteSet(Words);
  if not (1 in Bytes) or
     not (255 in Bytes) then
    Halt(4);

  Child.Free
end.
