program EnumOrdinalConstants;

type
  { FPC uses this shape for nominal machine encodings: the two declared
    members establish the carrier extrema, while explicit casts construct
    typed register codes between them. }
  TRegister = (
    TRegisterLowEnum := Low(LongInt),
    TRegisterHighEnum := High(LongInt)
  );

const
  NR_ES = TRegister($05000000);
  NR_CS = TRegister($05000001);
  NR_SS = TRegister($05000002);
  NR_DS = TRegister($05000003);
  NR_FS = TRegister($05000004);
  NR_GS = TRegister($05000005);

  { Untyped aliases must preserve the same typed enum value as the expression
    they replace, including when the evaluator forms an unnamed ordinal. }
  AliasES = NR_ES;
  AliasCS = Succ(AliasES);

type
  TDirectPrefixes = array[NR_ES..NR_GS] of Byte;
  TAliasPrefixes = array[AliasES..NR_GS] of Byte;

const
  DirectPrefixes: TDirectPrefixes =
    ($26, $2e, $36, $3e, $64, $65);
  AliasPrefixes: TAliasPrefixes =
    ($26, $2e, $36, $3e, $64, $65);

var
  RegisterValue: TRegister;

begin
  if SizeOf(DirectPrefixes) <> 6 then
    Halt(1);
  if SizeOf(AliasPrefixes) <> 6 then
    Halt(2);
  if Ord(AliasCS) <> $05000001 then
    Halt(3);
  if Ord(Low(TDirectPrefixes)) <> $05000000 then
    Halt(4);
  if Ord(High(TDirectPrefixes)) <> $05000005 then
    Halt(5);

  {$R-}
  RegisterValue := NR_DS;
  if DirectPrefixes[RegisterValue] <> $3e then
    Halt(6);

  {$R+}
  RegisterValue := NR_GS;
  if AliasPrefixes[RegisterValue] <> $65 then
    Halt(7)
end.
