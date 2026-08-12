program TypedIntegerConstantConversion;

{$R+}

type
  TProbeRecord = packed record
    StrPos: LongInt;
    NType: Byte;
    NOther: Byte;
    NDesc: Word;
    NValue: LongInt;
  end;

const
  { Both selected typed results fit ShortInt. ShortInt assigns to Integer but
    not conversely, so it is the unique least lossless common operand domain. }
  ComputedZero = (9 - 16) * Ord(16 < 9);

var
  UnsignedValue: QWord;

function Kind(Value: SizeInt): Integer; overload;
begin
  if Value = Value then
    Kind := 1
end;

function Kind(Value: QWord): Integer; overload;
begin
  if Value = Value then
    Kind := 2
end;

function AcceptQWord(Value: QWord): QWord;
begin
  AcceptQWord := Value
end;

begin
  if ComputedZero <> 0 then
    Halt(9);

  if SizeOf(TProbeRecord) <> 12 then
    Halt(1);

  { SizeOf remains concretely SizeInt, so its exact overload remains first. }
  if Kind(SizeOf(TProbeRecord)) <> 1 then
    Halt(2);

  { Assignment and a value formal use the same value-fitting conversion. }
  UnsignedValue := SizeOf(TProbeRecord);
  if UnsignedValue <> 12 then
    Halt(3);
  if AcceptQWord(SizeOf(TProbeRecord)) <> 12 then
    Halt(4);

  { The QWord proposal preserves both the runtime QWord domain and the known
    positive SizeInt value. The Int64 proposal would narrow UnsignedValue. }
  UnsignedValue := 120;
  if Kind(UnsignedValue div SizeOf(TProbeRecord)) <> 2 then
    Halt(5);
  if UnsignedValue div SizeOf(TProbeRecord) <> 10 then
    Halt(6);

  { The rule applies to a typed result of selected constant arithmetic, not
    SizeOf syntax specifically. }
  if Kind(UnsignedValue div (SizeOf(TProbeRecord) + 0)) <> 2 then
    Halt(7);
  if UnsignedValue div (SizeOf(TProbeRecord) + 0) <> 10 then
    Halt(8)
end.
