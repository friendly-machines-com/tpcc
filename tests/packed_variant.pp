program PackedVariant;

type
  TAsmCond = packed record
    Prefix: Byte;
    case Simple: Boolean of
      False: (BO, BI: Byte);
      True: (
        Cond: Byte;
        case Byte of
          0: ();
          1: (CR: Byte);
          2: (CRBit: Byte)
      )
  end;
  TOrdinaryVariant = record
    Prefix: Byte;
    case Simple: Boolean of
      False: (BO, BI: Byte);
      True: (
        Cond: Byte;
        case Byte of
          0: ();
          1: (CR: Byte);
          2: (CRBit: Byte)
      )
  end;

var
  C: TAsmCond;
  O: TOrdinaryVariant;
  Z: LongInt;

begin
  if SizeOf(TAsmCond) <> 4 then
    begin
      Z := 0;
      Z := 1 div Z
    end;

  C.Prefix := 9;
  C.Simple := False;
  C.BO := 17;
  C.BI := 23;
  if C.Cond <> 17 then
    begin
      Z := 0;
      Z := 1 div Z
    end;
  if C.CRBit <> 23 then
    begin
      Z := 0;
      Z := 1 div Z
    end;

  C.Cond := 31;
  C.CR := 37;
  if C.BO <> 31 then
    begin
      Z := 0;
      Z := 1 div Z
    end;
  if C.BI <> 37 then
    begin
      Z := 0;
      Z := 1 div Z
    end;
  if C.Prefix <> 9 then
    begin
      Z := 0;
      Z := 1 div Z
    end;

  O.BO := 41;
  O.BI := 43;
  if O.Cond <> 41 then
    begin
      Z := 0;
      Z := 1 div Z
    end;
  if O.CRBit <> 43 then
    begin
      Z := 0;
      Z := 1 div Z
    end
end.
