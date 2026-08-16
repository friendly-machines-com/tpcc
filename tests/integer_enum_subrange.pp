program IntegerEnumSubrange;

type
  TRegister = (
    TRegisterLowEnum := Low(LongInt),
    TRegisterHighEnum := High(LongInt)
  );

const
  NR_GS = TRegister($05000005);

type
  { Equal ordinal payloads do not make Integer and TRegister the same Pascal
    domain. }
  TBadRange = $05000000..NR_GS;

begin
end.
